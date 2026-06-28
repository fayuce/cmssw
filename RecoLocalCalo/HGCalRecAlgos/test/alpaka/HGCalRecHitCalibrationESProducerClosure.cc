// Closure test for RecHit calibration through the ESProducer path.
//
// This validates:
//   SQLite CondDB
//     -> PoolDBESSource
//     -> EventSetup / HGCalRecHitCalibrationRcd
//     -> HGCalRecHitCalibrationConditions
//     -> HGCalCalibrationESProducer
//     -> HGCalCalibParamDevice
//     -> comparison with reference RecHitCalib JSON content
//
// The reference JSON is not written as an intermediate file.
// Use referenceJson = "-" to read it from stdin.

#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "FWCore/Utilities/interface/Exception.h"
#include "FWCore/Utilities/interface/InputTag.h"

#include "HeterogeneousCore/AlpakaCore/interface/alpaka/stream/EDProducer.h"
#include "HeterogeneousCore/AlpakaCore/interface/alpaka/ESGetToken.h"
#include "HeterogeneousCore/AlpakaCore/interface/alpaka/Event.h"
#include "HeterogeneousCore/AlpakaInterface/interface/config.h"

#include "CondFormats/DataRecord/interface/HGCalElectronicsMappingRcd.h"
#include "CondFormats/DataRecord/interface/HGCalModuleConfigurationRcd.h"
#include "CondFormats/HGCalObjects/interface/HGCalMappingModuleIndexer.h"
#include "CondFormats/HGCalObjects/interface/alpaka/HGCalCalibrationParameterDevice.h"
#include "RecoLocalCalo/HGCalRecAlgos/interface/HGCalESProducerTools.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace ALPAKA_ACCELERATOR_NAMESPACE {

  class HGCalRecHitCalibrationESProducerClosure : public stream::EDProducer<> {
  public:
    explicit HGCalRecHitCalibrationESProducerClosure(const edm::ParameterSet& iConfig)
        : EDProducer(iConfig),
          referenceJson_(iConfig.getParameter<std::string>("referenceJson")),
          tolerance_(iConfig.getParameter<double>("tolerance")) {
      indexerToken_ = esConsumes(iConfig.getParameter<edm::ESInputTag>("indexSource"));
      calibParamToken_ = esConsumes(iConfig.getParameter<edm::ESInputTag>("calibParamSource"));

      std::string content;
      if (referenceJson_ == "-") {
        content.assign(std::istreambuf_iterator<char>(std::cin), std::istreambuf_iterator<char>());
      } else {
        std::ifstream f(referenceJson_);
        if (!f.is_open()) {
          throw cms::Exception("HGCalRecHitCalibrationESProducerClosure")
              << "Cannot open reference JSON: " << referenceJson_;
        }
        content.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
      }

      if (content.empty()) {
        throw cms::Exception("HGCalRecHitCalibrationESProducerClosure")
            << "Reference RecHitCalib JSON content is empty.";
      }

      refData_ = json::parse(content, nullptr, true, /*ignore_comments*/ true);
    }

    static void fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
      edm::ParameterSetDescription desc;
      desc.add("indexSource", edm::ESInputTag{})->setComment("Label for HGCal module indexer");
      desc.add("calibParamSource", edm::ESInputTag{})->setComment("Label for ESProducer calibration SoA product");
      desc.add<std::string>("referenceJson", "-")->setComment("Reference RecHitCalib JSON content/file; '-' means stdin");
      desc.add<double>("tolerance", 1e-5)->setComment("Relative tolerance for closure comparison");
      descriptions.addWithDefaultLabel(desc);
    }

  private:
    void produce(device::Event&, device::EventSetup const& iSetup) override {
      if (checked_) {
        return;
      }
      checked_ = true;

      auto const& moduleIndexer = iSetup.getData(indexerToken_);
      auto const& calibParamDevice = iSetup.getData(calibParamToken_);
      auto const calibView = calibParamDevice.view();
      const int calibSize = calibView.metadata().size();

      bool ok = true;
      std::size_t modulesChecked = 0;
      std::size_t channelsChecked = 0;
      std::size_t valuesCompared = 0;
      double maxRelDiff = 0.0;
      std::string maxRelDiffLocation = "n/a";

      auto matchGlobPattern = [](const std::string& pattern, const std::string& value) {
        const std::size_t n = pattern.size();
        const std::size_t m = value.size();
        std::vector<std::vector<bool>> dp(n + 1, std::vector<bool>(m + 1, false));
        dp[0][0] = true;

        for (std::size_t i = 1; i <= n; ++i) {
          if (pattern[i - 1] == '*') {
            dp[i][0] = dp[i - 1][0];
          }
        }

        for (std::size_t i = 1; i <= n; ++i) {
          for (std::size_t j = 1; j <= m; ++j) {
            if (pattern[i - 1] == '*') {
              dp[i][j] = dp[i - 1][j] || dp[i][j - 1];
            } else if (pattern[i - 1] == '?' || pattern[i - 1] == value[j - 1]) {
              dp[i][j] = dp[i - 1][j - 1];
            }
          }
        }

        return dp[n][m];
      };

      auto findReferenceKey = [&](const std::string& module) -> std::string {
        if (refData_.contains(module)) {
          return module;
        }

        std::string best;
        std::size_t bestPatternLength = 0;

        for (const auto& item : refData_.items()) {
          const std::string& pattern = item.key();
          if (!matchGlobPattern(pattern, module)) {
            continue;
          }
          if (pattern.size() > bestPatternLength) {
            best = pattern;
            bestPatternLength = pattern.size();
          }
        }

        return best;
      };

      auto compareFloat = [&](const std::string& module,
                              const std::string& field,
                              std::size_t channel,
                              double got,
                              double expected) {
        const double denom = std::abs(expected) > 1e-10 ? std::abs(expected) : 1.0;
        const double rel = std::abs(got - expected) / denom;

        if (rel > maxRelDiff) {
          maxRelDiff = rel;
          maxRelDiffLocation = "module=" + module + " field=" + field + " channel=" + std::to_string(channel);
        }

        ++valuesCompared;

        if (rel > tolerance_) {
          edm::LogError("HGCalRecHitCalibrationESProducerClosure")
              << "Mismatch module=" << module
              << " field=" << field
              << " channel=" << channel
              << " esproducer=" << got
              << " reference=" << expected
              << " rel_diff=" << rel;
          ok = false;
        }
      };

      auto compareInt = [&](const std::string& module,
                            const std::string& field,
                            std::size_t channel,
                            int got,
                            int expected) {
        ++valuesCompared;

        if (got != expected) {
          edm::LogError("HGCalRecHitCalibrationESProducerClosure")
              << "Mismatch module=" << module
              << " field=" << field
              << " channel=" << channel
              << " esproducer=" << got
              << " reference=" << expected;
          ok = false;
        }
      };

      auto getFloat2DValue = [](const json& ref, const std::string& field, std::size_t i, std::size_t k, float def) {
        if (!ref.contains(field)) {
          return def;
        }
        return ref.at(field).at(i).at(k).get<float>();
      };

      for (const auto& [typecode, ids] : moduleIndexer.typecodeMap()) {
        const auto [fedid, modid] = ids;

        const std::string refKey = findReferenceKey(typecode);
        if (refKey.empty()) {
          edm::LogWarning("HGCalRecHitCalibrationESProducerClosure")
              << "No reference RecHitCalib entry found for module '" << typecode << "'.";
          continue;
        }

        const auto& ref = refData_.at(refKey);
        const auto refChannel = ref.at("Channel").get<std::vector<int32_t>>();

        const uint32_t offset = moduleIndexer.getIndexForModuleData(fedid, modid, 0, 0);
        if (offset >= static_cast<uint32_t>(calibSize)) {
          edm::LogError("HGCalRecHitCalibrationESProducerClosure")
              << "Offset outside calibration SoA for module '" << typecode
              << "': offset=" << offset
              << " size=" << calibSize;
          ok = false;
          continue;
        }

        std::size_t nrows = std::min<std::size_t>(moduleIndexer.getNumChannels(typecode), refChannel.size());
        nrows = std::min<std::size_t>(nrows, static_cast<std::size_t>(calibSize - offset));

        ++modulesChecked;
        channelsChecked += nrows;

        for (std::size_t i = 0; i < nrows; ++i) {
          const auto& calib = calibView[offset + i];

          compareFloat(typecode, "ADC_ped", i, calib.ADC_ped(), ref.at("ADC_ped").at(i).get<float>());
          compareFloat(typecode, "Noise", i, calib.Noise(), ref.at("Noise").at(i).get<float>());
          compareFloat(typecode, "CM_slope", i, calib.CM_slope(), ref.at("CM_slope").at(i).get<float>());
          compareFloat(typecode, "CM_ped", i, calib.CM_ped(), ref.at("CM_ped").at(i).get<float>());
          compareFloat(typecode, "BXm1_slope", i, calib.BXm1_slope(), ref.at("BXm1_slope").at(i).get<float>());
          compareFloat(typecode, "TOTtoADC", i, calib.TOTtoADC(), ref.at("TOTtoADC").at(i).get<float>());
          compareFloat(typecode, "TOT_ped", i, calib.TOT_ped(), ref.at("TOT_ped").at(i).get<float>());
          compareFloat(typecode, "TOT_lin", i, calib.TOT_lin(), ref.at("TOT_lin").at(i).get<float>());
          compareFloat(typecode, "TOT_P0", i, calib.TOT_P0(), ref.at("TOT_P0").at(i).get<float>());
          compareFloat(typecode, "TOT_P1", i, calib.TOT_P1(), ref.at("TOT_P1").at(i).get<float>());
          compareFloat(typecode, "TOT_P2", i, calib.TOT_P2(), ref.at("TOT_P2").at(i).get<float>());
          compareFloat(typecode, "MIPS_scale", i, calib.MIPS_scale(), ref.at("MIPS_scale").at(i).get<float>());

          compareInt(typecode, "Valid", i, static_cast<int>(calib.valid()), ref.at("Valid").at(i).get<int32_t>());

          for (std::size_t k = 0; k < 32; ++k) {
            compareFloat(typecode, "TOA_CTDC", i, calib.TOA_CTDC()(k), getFloat2DValue(ref, "TOA_CTDC", i, k, 0.f));
          }
          for (std::size_t k = 0; k < 8; ++k) {
            compareFloat(typecode, "TOA_FTDC", i, calib.TOA_FTDC()(k), getFloat2DValue(ref, "TOA_FTDC", i, k, 0.f));
          }
          for (std::size_t k = 0; k < 3; ++k) {
            compareFloat(typecode, "TOA_TW", i, calib.TOA_TW()(k), getFloat2DValue(ref, "TOA_TW", i, k, 0.f));
          }
        }
      }

      edm::LogInfo("HGCalRecHitCalibrationESProducerClosure")
          << "ESProducer SQLite closure summary:"
          << " modulesChecked=" << modulesChecked
          << " channelsChecked=" << channelsChecked
          << " valuesCompared=" << valuesCompared
          << " maxRelDiff=" << maxRelDiff
          << " maxRelDiffLocation=" << maxRelDiffLocation
          << " tolerance=" << tolerance_
          << " result=" << (ok ? "PASSED" : "FAILED");

      if (!ok) {
        throw cms::Exception("HGCalRecHitCalibrationESProducerClosure")
            << "ESProducer SQLite closure comparison failed.";
      }
    }

    edm::ESGetToken<HGCalMappingModuleIndexer, HGCalElectronicsMappingRcd> indexerToken_;
    device::ESGetToken<hgcalrechit::HGCalCalibParamDevice, HGCalModuleConfigurationRcd> calibParamToken_;

    const std::string referenceJson_;
    const double tolerance_;
    json refData_;
    bool checked_ = false;
  };

}  // namespace ALPAKA_ACCELERATOR_NAMESPACE

#include "HeterogeneousCore/AlpakaCore/interface/alpaka/MakerMacros.h"
DEFINE_FWK_ALPAKA_MODULE(HGCalRecHitCalibrationESProducerClosure);
