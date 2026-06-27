// Author: Izaak Neutelings (March 2024)

// includes for CMSSW
#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "FWCore/ParameterSet/interface/FileInPath.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/Utilities/interface/Exception.h"

// includes for Alpaka
#include "HeterogeneousCore/AlpakaCore/interface/alpaka/ESGetToken.h"
#include "HeterogeneousCore/AlpakaCore/interface/alpaka/ESProducer.h"
#include "HeterogeneousCore/AlpakaCore/interface/alpaka/ModuleFactory.h"
#include "HeterogeneousCore/AlpakaInterface/interface/config.h"
#include "HeterogeneousCore/AlpakaInterface/interface/host.h"
#include "HeterogeneousCore/AlpakaInterface/interface/memory.h"

// includes for HGCal, calibration, and configuration parameters
#include "CondFormats/HGCalObjects/interface/HGCalMappingModuleIndexer.h"
#include "CondFormats/HGCalObjects/interface/HGCalRecHitCalibrationConditions.h"
#include "CondFormats/HGCalObjects/interface/HGCalCalibrationParameterHost.h"
#include "CondFormats/HGCalObjects/interface/HGCalMappingParameterHost.h"
#include "CondFormats/HGCalObjects/interface/alpaka/HGCalCalibrationParameterDevice.h"
#include "CondFormats/DataRecord/interface/HGCalElectronicsMappingRcd.h"
#include "CondFormats/DataRecord/interface/HGCalRecHitCalibrationRcd.h"
#include "CondFormats/DataRecord/interface/HGCalModuleConfigurationRcd.h"  // depends on HGCalElectronicsMappingRcd
#include "DataFormats/ForwardDetId/interface/HGCSiliconDetId.h"            // for HGCSiliconDetId::waferType
#include "RecoLocalCalo/HGCalRecAlgos/interface/HGCalESProducerTools.h"    // for json, search_modkey

// includes for standard libraries
#include <string>
#include <fstream>    // needed to read json file with std::ifstream
#include <algorithm>  // for std::fill

namespace ALPAKA_ACCELERATOR_NAMESPACE {

  namespace hgcalrechit {
    using namespace ::hgcal;  // for check_keys, fill_SoA


    bool matchGlobPattern(const std::string& pattern, const std::string& value) {
      const size_t n = pattern.size();
      const size_t m = value.size();

      std::vector<std::vector<bool>> dp(n + 1, std::vector<bool>(m + 1, false));
      dp[0][0] = true;

      for (size_t i = 1; i <= n; ++i) {
        if (pattern[i - 1] == '*')
          dp[i][0] = dp[i - 1][0];
      }

      for (size_t i = 1; i <= n; ++i) {
        for (size_t j = 1; j <= m; ++j) {
          if (pattern[i - 1] == '*') {
            dp[i][j] = dp[i - 1][j] || dp[i][j - 1];
          } else if (pattern[i - 1] == '?' || pattern[i - 1] == value[j - 1]) {
            dp[i][j] = dp[i - 1][j - 1];
          }
        }
      }

      return dp[n][m];
    }

    const HGCalRecHitCalibrationConditions::ModuleData* findCalibrationModule(
    const std::string& module,
    const HGCalRecHitCalibrationConditions& calibConditions) {
  // 1. Exact match first.
  for (const auto& calibModule : calibConditions.modules) {
    if (calibModule.typeCode == module) {
      return &calibModule;
    }
  }

  // 2. Then choose the most specific matching glob pattern.
  // This prevents "*" from winning over patterns such as "ML-F3WX-IH001*".
  const HGCalRecHitCalibrationConditions::ModuleData* best = nullptr;
  std::size_t bestPatternLength = 0;

  for (const auto& calibModule : calibConditions.modules) {
    const auto& pattern = calibModule.typeCode;

    if (!matchGlobPattern(pattern, module)) {
      continue;
    }

    if (pattern.size() > bestPatternLength) {
      best = &calibModule;
      bestPatternLength = pattern.size();
    }
  }

  return best;
}
 class HGCalCalibrationESProducer : public ESProducer {
    public:
      HGCalCalibrationESProducer(const edm::ParameterSet& iConfig)
          : ESProducer(iConfig),
            filenameEnergy_(iConfig.getParameter<edm::FileInPath>("filenameEnergyLoss")) {
        auto cc = setWhatProduced(this);
        indexToken_ = cc.consumes(iConfig.getParameter<edm::ESInputTag>("indexSource"));
        mapToken_ = cc.consumes(iConfig.getParameter<edm::ESInputTag>("mapSource"));
        calibToken_ = cc.consumes(iConfig.getParameter<edm::ESInputTag>("calibSource"));
      }

      static void fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
        edm::ParameterSetDescription desc;
        desc.add<edm::ESInputTag>("calibSource", edm::ESInputTag(""))
            ->setComment("Label for HGCal RecHit calibration conditions from CondDB/EventSetup");
        desc.add<edm::FileInPath>("filenameEnergyLoss")
            ->setComment("Path to JSON file with energy loss & thickness corrections");
        desc.add<edm::ESInputTag>("indexSource", edm::ESInputTag(""))
            ->setComment("Label for module indexer to set SoA size");
        desc.add<edm::ESInputTag>("mapSource", edm::ESInputTag(""))
            ->setComment("Label for SoA with module mapper/information");
        descriptions.addWithDefaultLabel(desc);
      }

      // @short compute thickness correction to energy loss
      // 0: CE_E_120um, 1: CE_E_200um, 2: CE_E_300um,
      // 3: CE_H_120um, 4: CE_H_200um, 5: CE_H_300um
      float getThicknessCorrection(const std::vector<float>& sfs,
                                   const uint32_t& idetid,
                                   const int& celltype,
                                   const std::string& fname) {
        using waferType = HGCSiliconDetId::waferType;
        const HGCSiliconDetId detid(idetid);
        const bool isCEE = (detid.det() == DetId::HGCalEE);  //layer<=17;
        uint32_t idx = -1;
        if (celltype == waferType::HGCalHD120)
          idx = (isCEE ? 0 : 3);
        else if (celltype == waferType::HGCalHD200 or celltype == waferType::HGCalLD200)
          idx = (isCEE ? 1 : 4);
        else if (celltype == waferType::HGCalLD300)
          idx = (isCEE ? 2 : 5);
        else {
          cms::Exception ex("InvalidData");
          ex << "Could not find thickness correction for celltype " << celltype << " in layer" << detid.layer()
             << "in '" << fname << "'!";
          ex.addContext("Calling hgcal::getThicknessCorrection()");
        }
        if (idx >= sfs.size()) {
          cms::Exception ex("InvalidData");
          ex << "The index of the thickness correction ()" << idx << ") for celltype " << celltype << " in layer"
             << detid.layer() << "is too large for '" << fname << "'(" << sfs.size() << ")!";
          ex.addContext("Calling hgcal::getThicknessCorrection()");
        }
        return sfs[idx];
      }

      // @short create the ESProducer product: a SoA with channel-level calibration constants
      std::optional<hgcalrechit::HGCalCalibParamHost> produce(const HGCalModuleConfigurationRcd& iRecord) {
        auto const& moduleIndexer = iRecord.get(indexToken_);
        auto const& moduleMapper = iRecord.get(mapToken_);
        auto const& calibConditions = iRecord.get(calibToken_);
        edm::LogInfo("HGCalCalibrationESProducer")
            << "produce: loaded HGCalRecHitCalibrationConditions with "
            << calibConditions.nModules() << " module(s)";

        // load dense indexing
        const uint32_t nchans = moduleIndexer.maxDataSize();  // channel-level size
        hgcalrechit::HGCalCalibParamHost product(cms::alpakatools::host(), nchans);

        // load energy-loss parameters from JSON; RecHit calibration constants come from CondDB/EventSetup
        std::ifstream infileEnergy(filenameEnergy_.fullPath().c_str());
        json energy_data = json::parse(infileEnergy, nullptr, true, /*ignore_comments*/ true);

        // check keys
        const std::vector<std::string> energy_keys = {"dEdx", "SF_thickness_Si", "SF_thickness_SiPM"};
        check_keys(energy_data, energy_keys, filenameEnergy_.fullPath());
        const float nlayers = energy_data["dEdx"].size();  // number of absorber layers
        if (nlayers != 47)                                 // TODO: retrieve from nlayers from Geometry
          edm::LogError("HGCalCalibrationESProducer")
              << "Expected 47 layers, but got " << nlayers << " in " << filenameEnergy_.fullPath();
        const std::vector<float> energylosses = energy_data["dEdx"].get<std::vector<float>>();

        std::size_t modulesVisited = 0;
        std::size_t exactMatches = 0;
        std::size_t patternMatches = 0;
        std::size_t fallbackMatches = 0;
        std::size_t unmatchedModules = 0;

        // loop over all module typecodes, e.g. "ML-F3PT-TX-0003"
        for (const auto& [module, ids] : moduleIndexer.typecodeMap()) {
          ++modulesVisited;
          const auto [fedid, modid] = ids;

          // retrieve matching calibration payload; glob patterns are allowed in payload typeCode
          const auto* calib = findCalibrationModule(module, calibConditions);
          if (calib == nullptr) {
            ++unmatchedModules;
            edm::LogWarning("HGCalCalibrationESProducer")
                << "No RecHit calibration payload found for module '" << module << "'. Skipping this module.";
            continue;
          }

          if (calib->typeCode == module) {
            ++exactMatches;
          } else if (calib->typeCode == "*") {
            ++fallbackMatches;
            edm::LogInfo("HGCalCalibrationESProducer")
                << "Module '" << module << "' uses fallback '*' RecHit calibration entry.";
          } else {
            ++patternMatches;
            edm::LogInfo("HGCalCalibrationESProducer")
                << "Module '" << module << "' matched RecHit calibration pattern '" << calib->typeCode << "'.";
          }

          // get dimensions
          const uint32_t imod = moduleIndexer.getIndexForModule(fedid, modid);  // dense index in module SoA
          const uint32_t offset = moduleIndexer.getIndexForModuleData(module);  // first channel index
          const uint32_t nchans = moduleIndexer.getNumChannels(module);         // number of channels in mapper
          uint32_t nrows = calib->nChannels();                                  // number of channels in CondDB payload

          // check number of channels & ROCs make sense
          if (nrows % 37 != 0) {
            edm::LogWarning("HGCalCalibrationESProducer")
                << "nchannels=" << nrows << ", which is not divisible by 37 (#channels per e-Rx)!";
          }
          if (nchans != nrows) {
            edm::LogWarning("HGCalCalibrationESProducer")
                << "nchannels does not match between module indexer ('" << module << "'," << nchans
                << ") and CondDB payload ('" << calib->typeCode << "'," << nrows << ")!";
            nrows = std::min(nrows, nchans);  // take smallest to avoid overlap
          }

          // fill calibration parameters for ADC, CM, TOT, MIPS scale, ...
          fill_SoA_column<float>(product.view().ADC_ped(), calib->ADC_ped, offset, nrows);
          fill_SoA_column<float>(product.view().Noise(), calib->Noise, offset, nrows);
          fill_SoA_column<float>(product.view().CM_slope(), calib->CM_slope, offset, nrows);
          fill_SoA_column<float>(product.view().CM_ped(), calib->CM_ped, offset, nrows);
          fill_SoA_column<float>(product.view().BXm1_slope(), calib->BXm1_slope, offset, nrows);
          fill_SoA_column<float>(product.view().TOTtoADC(), calib->TOTtoADC, offset, nrows);
          fill_SoA_column<float>(product.view().TOT_ped(), calib->TOT_ped, offset, nrows);
          fill_SoA_column<float>(product.view().TOT_lin(), calib->TOT_lin, offset, nrows);
          fill_SoA_column<float>(product.view().TOT_P0(), calib->TOT_P0, offset, nrows);
          fill_SoA_column<float>(product.view().TOT_P1(), calib->TOT_P1, offset, nrows);
          fill_SoA_column<float>(product.view().TOT_P2(), calib->TOT_P2, offset, nrows);
          fill_SoA_column<float>(product.view().MIPS_scale(), calib->MIPS_scale, offset, nrows);

          std::vector<unsigned char> valid(nrows);
          std::transform(calib->valid.begin(), calib->valid.begin() + nrows, valid.begin(),
                         [](int32_t v) { return static_cast<unsigned char>(v); });
          fill_SoA_column<unsigned char>(product.view().valid(), valid, offset, nrows);

          std::vector<std::vector<float>> defaultTOA_CTDC;
          std::vector<std::vector<float>> defaultTOA_FTDC;
          std::vector<std::vector<float>> defaultTOA_TW;

          const auto* TOA_CTDC = &calib->TOA_CTDC;
          const auto* TOA_FTDC = &calib->TOA_FTDC;
          const auto* TOA_TW = &calib->TOA_TW;

          if (TOA_CTDC->empty()) {
            defaultTOA_CTDC.assign(nrows, std::vector<float>(32, 0.f));
            TOA_CTDC = &defaultTOA_CTDC;
          }
          if (TOA_FTDC->empty()) {
            defaultTOA_FTDC.assign(nrows, std::vector<float>(8, 0.f));
            TOA_FTDC = &defaultTOA_FTDC;
          }
          if (TOA_TW->empty()) {
            defaultTOA_TW.assign(nrows, std::vector<float>(3, 0.f));
            TOA_TW = &defaultTOA_TW;
          }

          // fill vectors for ToA correction parameters
          for (size_t n = 0; n < nrows; n++) {
            auto vi = product.view()[offset + n];
            fill_SoA_eigen_row<float>(vi.TOA_CTDC(), *TOA_CTDC, n);
            fill_SoA_eigen_row<float>(vi.TOA_FTDC(), *TOA_FTDC, n);
            fill_SoA_eigen_row<float>(vi.TOA_TW(), *TOA_TW, n);
          }

          // energy loss of absorption layers that sandwich the sensors is provided already averaged
          // https://twiki.cern.ch/twiki/pub/CMS/HGCALSimulationAndPerformance/CalibratedRecHits.pdf
          const int layer = moduleMapper.view().plane()[imod];  // counts from 1
          float dEdx = energylosses[layer - 1];

          // compute thickness correction
          float sf_from_config;
          const bool isSiPM = moduleMapper.view().isSiPM()[imod];
          const int celltype = moduleMapper.view().celltype()[imod];
          const uint32_t detid = moduleMapper.view().detid()[imod];
          if (isSiPM)  // scintillator
            sf_from_config = energy_data["SF_thickness_SiPM"][0];
          else  // Si module
            sf_from_config =
                getThicknessCorrection(energy_data["SF_thickness_Si"], detid, celltype, filenameEnergy_.fullPath());
          edm::LogInfo("HGCalCalibrationESProducer")
              << "layer = " << layer << ", celltype = " << celltype << ", isSiPM = " << isSiPM << ", dEdx = " << dEdx
              << ", sf_from_config = " << sf_from_config << std::endl;
          if (sf_from_config <= 0)
            throw cms::Exception("ConfigError") << "EM reconstruction scale factor (SF_thickness_Si) is not positive.";

          float sf = 1. / sf_from_config;
          dEdx *= sf * 1e-3;  // apply correction and convert from MeV to GeV
          fill_SoA_column_single<float>(product.view().EM_scale().data(), dEdx, offset, nrows);

        }  // end of loop over modules

        edm::LogInfo("HGCalCalibrationESProducer")
            << "\n"
            << "\n  HGCal RecHit Calibration Matching Summary"
            << "\n  modules visited      : " << modulesVisited
            << "\n  exact matches        : " << exactMatches
            << "\n  pattern matches      : " << patternMatches
            << "\n  fallback '*' matches : " << fallbackMatches
            << "\n  unmatched modules    : " << unmatchedModules
            << "\n";

        if (fallbackMatches > 0) {
          edm::LogWarning("HGCalCalibrationESProducer")
              << "Some modules used the fallback '*' RecHit calibration entry. "
              << "This is allowed, but a more specific calibration entry may be needed for production.";
        }

        if (unmatchedModules > 0) {
          edm::LogWarning("HGCalCalibrationESProducer")
              << unmatchedModules << " module(s) had no matching RecHit calibration payload.";
        }

        return product;
      }  // end of produce()

    private:
      edm::ESGetToken<HGCalMappingModuleIndexer, HGCalElectronicsMappingRcd> indexToken_;
      edm::ESGetToken<hgcal::HGCalMappingModuleParamHost, HGCalElectronicsMappingRcd> mapToken_;
      edm::ESGetToken<HGCalRecHitCalibrationConditions, HGCalRecHitCalibrationRcd> calibToken_;
      const edm::FileInPath filenameEnergy_;
    };

  }  // namespace hgcalrechit

}  // namespace ALPAKA_ACCELERATOR_NAMESPACE

DEFINE_FWK_EVENTSETUP_ALPAKA_MODULE(hgcalrechit::HGCalCalibrationESProducer);
