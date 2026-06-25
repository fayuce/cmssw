// HGCalRecHitCalibrationDBReader.cc
//
// Read HGCalRecHitCalibrationConditions from SQLite CondDB
// and compare it with the reference level0 calibration JSON.

#include "FWCore/Framework/interface/one/EDAnalyzer.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/EventSetup.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "FWCore/Utilities/interface/Exception.h"

#include "CondCore/CondDB/interface/ConnectionPool.h"
#include "CondFormats/HGCalObjects/interface/HGCalRecHitCalibrationConditions.h"

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include <cmath>
#include <fstream>
#include <numeric>
#include <string>
#include <vector>

class HGCalRecHitCalibrationDBReader : public edm::one::EDAnalyzer<> {
public:
  explicit HGCalRecHitCalibrationDBReader(const edm::ParameterSet&);
  ~HGCalRecHitCalibrationDBReader() override = default;

  static void fillDescriptions(edm::ConfigurationDescriptions& descriptions);

private:
  void analyze(const edm::Event&, const edm::EventSetup&) override;

  void printConditions(const HGCalRecHitCalibrationConditions& cond) const;
  bool compareWithJson(const HGCalRecHitCalibrationConditions& cond) const;

  bool compareIntVector(const std::string& module,
                        const std::string& field,
                        const std::vector<int32_t>& db,
                        const std::vector<int32_t>& ref) const;

  bool compareFloatVector(const std::string& module,
                          const std::string& field,
                          const std::vector<float>& db,
                          const std::vector<float>& ref) const;

  bool compareFloat2DVector(const std::string& module,
                            const std::string& field,
                            const std::vector<std::vector<float>>& db,
                            const std::vector<std::vector<float>>& ref) const;

  static std::vector<float> getFloatVectorOrDefault(const json& j,
                                                    const std::string& key,
                                                    std::size_t n,
                                                    float value);

  static std::vector<std::vector<float>> getFloat2DOrDefault(const json& j,
                                                             const std::string& key,
                                                             std::size_t n,
                                                             std::size_t width,
                                                             float value);

  std::string sqliteFile_;
  std::string tag_;
  unsigned long long run_;
  std::string refJsonFile_;
  double tolerance_;
  bool verbose_;
  bool done_;
};

HGCalRecHitCalibrationDBReader::HGCalRecHitCalibrationDBReader(const edm::ParameterSet& ps)
    : sqliteFile_(ps.getParameter<std::string>("sqliteFile")),
      tag_(ps.getParameter<std::string>("tag")),
      run_(ps.getParameter<unsigned long long>("run")),
      refJsonFile_(ps.getParameter<std::string>("refJsonFile")),
      tolerance_(ps.getParameter<double>("tolerance")),
      verbose_(ps.getUntrackedParameter<bool>("verbose", false)),
      done_(false) {}

void HGCalRecHitCalibrationDBReader::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
  edm::ParameterSetDescription desc;
  desc.add<std::string>("sqliteFile", "hgcal_rechit_calibration_test.db");
  desc.add<std::string>("tag", "HGCalRecHitCalibration_test_2026_06_25");
  desc.add<unsigned long long>("run", 1ULL);
  desc.add<std::string>("refJsonFile", "");
  desc.add<double>("tolerance", 1e-5);
  desc.addUntracked<bool>("verbose", false);
  descriptions.addWithDefaultLabel(desc);
}

std::vector<float> HGCalRecHitCalibrationDBReader::getFloatVectorOrDefault(
    const json& j, const std::string& key, std::size_t n, float value) {
  if (!j.contains(key))
    return std::vector<float>(n, value);
  return j.at(key).get<std::vector<float>>();
}

std::vector<std::vector<float>> HGCalRecHitCalibrationDBReader::getFloat2DOrDefault(
    const json& j, const std::string& key, std::size_t n, std::size_t width, float value) {
  if (!j.contains(key))
    return std::vector<std::vector<float>>(n, std::vector<float>(width, value));
  return j.at(key).get<std::vector<std::vector<float>>>();
}

void HGCalRecHitCalibrationDBReader::analyze(const edm::Event&, const edm::EventSetup&) {
  if (done_)
    return;
  done_ = true;

  namespace conddb = cond::persistency;

  conddb::ConnectionPool pool;
  auto session = pool.createSession("sqlite_file:" + sqliteFile_);
  session.transaction().start(true);

  auto iovProxy = session.readIov(tag_);
  auto iovs = iovProxy.selectAll();
  auto it = iovs.find(static_cast<cond::Time_t>(run_));

  if (it == iovs.end()) {
    throw cms::Exception("HGCalRecHitCalibrationDBReader")
        << "No IOV found in tag '" << tag_ << "' for run=" << run_;
  }

  cond::Hash payloadId = (*it).payloadId;
  auto conditions = session.fetchPayload<HGCalRecHitCalibrationConditions>(payloadId);
  session.transaction().commit();

  if (!conditions) {
    throw cms::Exception("HGCalRecHitCalibrationDBReader")
        << "Null payload for hash " << payloadId;
  }

  printConditions(*conditions);

  if (!refJsonFile_.empty()) {
    const bool ok = compareWithJson(*conditions);

    edm::LogInfo("HGCalRecHitCalibrationDBReader")
        << "\n=========================================="
        << "\n  RecHit Calibration Closure Result: "
        << (ok ? "*** PASSED ***" : "*** FAILED ***")
        << "\n==========================================";

    if (!ok) {
      throw cms::Exception("ClosureTestFailed")
          << "HGCal RecHit calibration closure test failed.";
    }
  }
}

void HGCalRecHitCalibrationDBReader::printConditions(
    const HGCalRecHitCalibrationConditions& cond) const {
  edm::LogInfo("HGCalRecHitCalibrationDBReader")
      << "\n=========================================="
      << "\n  HGCal RecHit Calibration Readback"
      << "\n  nModules: " << cond.nModules()
      << "\n==========================================";

  for (const auto& module : cond.modules) {
    const int n = module.nChannels();
    const int nValid = std::accumulate(module.valid.begin(), module.valid.end(), 0);

    const float meanPed =
        n > 0 ? std::accumulate(module.ADC_ped.begin(), module.ADC_ped.end(), 0.f) / n : 0.f;
    const float meanNoise =
        n > 0 ? std::accumulate(module.Noise.begin(), module.Noise.end(), 0.f) / n : 0.f;

    edm::LogInfo("HGCalRecHitCalibrationDBReader")
        << "Module: " << module.typeCode
        << " channels=" << n
        << " valid=" << nValid << "/" << n
        << " mean_ADC_ped=" << meanPed
        << " mean_Noise=" << meanNoise;

    if (verbose_ && n > 0) {
      edm::LogInfo("HGCalRecHitCalibrationDBReader")
          << "  first channel=" << module.channel.front()
          << " ADC_ped=" << module.ADC_ped.front()
          << " Noise=" << module.Noise.front()
          << " CM_ped=" << module.CM_ped.front()
          << " CM_slope=" << module.CM_slope.front()
          << " TOTtoADC=" << module.TOTtoADC.front()
          << " Valid=" << module.valid.front();
    }
  }
}

bool HGCalRecHitCalibrationDBReader::compareIntVector(
    const std::string& module,
    const std::string& field,
    const std::vector<int32_t>& db,
    const std::vector<int32_t>& ref) const {
  if (db.size() != ref.size()) {
    edm::LogError("HGCalRecHitCalibrationDBReader")
        << "Size mismatch module=" << module << " field=" << field
        << " db=" << db.size() << " ref=" << ref.size();
    return false;
  }

  bool ok = true;
  for (std::size_t i = 0; i < ref.size(); ++i) {
    if (db[i] != ref[i]) {
      ok = false;
      edm::LogWarning("HGCalRecHitCalibrationDBReader")
          << "Mismatch module=" << module << " field=" << field
          << " index=" << i << " db=" << db[i] << " ref=" << ref[i];
    }
  }
  return ok;
}

bool HGCalRecHitCalibrationDBReader::compareFloatVector(
    const std::string& module,
    const std::string& field,
    const std::vector<float>& db,
    const std::vector<float>& ref) const {
  if (db.size() != ref.size()) {
    edm::LogError("HGCalRecHitCalibrationDBReader")
        << "Size mismatch module=" << module << " field=" << field
        << " db=" << db.size() << " ref=" << ref.size();
    return false;
  }

  bool ok = true;
  for (std::size_t i = 0; i < ref.size(); ++i) {
    const double denom = std::abs(ref[i]) > 1e-10 ? std::abs(ref[i]) : 1.0;
    const double rel = std::abs(db[i] - ref[i]) / denom;

    if (rel > tolerance_) {
      ok = false;
      edm::LogWarning("HGCalRecHitCalibrationDBReader")
          << "Mismatch module=" << module << " field=" << field
          << " index=" << i
          << " db=" << db[i]
          << " ref=" << ref[i]
          << " rel_diff=" << rel;
    }
  }
  return ok;
}

bool HGCalRecHitCalibrationDBReader::compareFloat2DVector(
    const std::string& module,
    const std::string& field,
    const std::vector<std::vector<float>>& db,
    const std::vector<std::vector<float>>& ref) const {
  if (db.size() != ref.size()) {
    edm::LogError("HGCalRecHitCalibrationDBReader")
        << "Outer size mismatch module=" << module << " field=" << field
        << " db=" << db.size() << " ref=" << ref.size();
    return false;
  }

  bool ok = true;
  for (std::size_t i = 0; i < ref.size(); ++i) {
    if (db[i].size() != ref[i].size()) {
      ok = false;
      edm::LogError("HGCalRecHitCalibrationDBReader")
          << "Inner size mismatch module=" << module << " field=" << field
          << " index=" << i
          << " db=" << db[i].size()
          << " ref=" << ref[i].size();
      continue;
    }

    for (std::size_t k = 0; k < ref[i].size(); ++k) {
      const double denom = std::abs(ref[i][k]) > 1e-10 ? std::abs(ref[i][k]) : 1.0;
      const double rel = std::abs(db[i][k] - ref[i][k]) / denom;

      if (rel > tolerance_) {
        ok = false;
        edm::LogWarning("HGCalRecHitCalibrationDBReader")
            << "Mismatch module=" << module << " field=" << field
            << " index=(" << i << "," << k << ")"
            << " db=" << db[i][k]
            << " ref=" << ref[i][k]
            << " rel_diff=" << rel;
      }
    }
  }
  return ok;
}

bool HGCalRecHitCalibrationDBReader::compareWithJson(
    const HGCalRecHitCalibrationConditions& cond) const {
  std::ifstream f(refJsonFile_);
  if (!f.is_open()) {
    edm::LogError("HGCalRecHitCalibrationDBReader")
        << "Cannot open reference JSON: " << refJsonFile_;
    return false;
  }

  json input = json::parse(f);
  bool ok = true;

  if (cond.modules.size() != input.size()) {
    edm::LogError("HGCalRecHitCalibrationDBReader")
        << "Module count mismatch: CondDB=" << cond.modules.size()
        << " JSON=" << input.size();
    ok = false;
  }

  for (const auto& item : input.items()) {
    const std::string& typeCode = item.key();
    const json& j = item.value();

    const auto* module = cond.getModule(typeCode);
    if (!module) {
      edm::LogError("HGCalRecHitCalibrationDBReader")
          << "Module '" << typeCode << "' not found in CondDB payload.";
      ok = false;
      continue;
    }

    const auto refChannel = j.at("Channel").get<std::vector<int32_t>>();
    const auto refValid = j.at("Valid").get<std::vector<int32_t>>();
    const std::size_t n = refChannel.size();

    ok &= compareIntVector(typeCode, "Channel", module->channel, refChannel);
    ok &= compareIntVector(typeCode, "Valid", module->valid, refValid);

    ok &= compareFloatVector(typeCode, "ADC_ped", module->ADC_ped, j.at("ADC_ped").get<std::vector<float>>());
    ok &= compareFloatVector(typeCode, "Noise", module->Noise, j.at("Noise").get<std::vector<float>>());
    ok &= compareFloatVector(typeCode, "CM_ped", module->CM_ped, j.at("CM_ped").get<std::vector<float>>());
    ok &= compareFloatVector(typeCode, "CM_slope", module->CM_slope, j.at("CM_slope").get<std::vector<float>>());
    ok &= compareFloatVector(typeCode, "BXm1_slope", module->BXm1_slope, j.at("BXm1_slope").get<std::vector<float>>());
    ok &= compareFloatVector(typeCode, "BXm1_ped", module->BXm1_ped, getFloatVectorOrDefault(j, "BXm1_ped", n, 0.f));

    ok &= compareFloatVector(typeCode, "TOTtoADC", module->TOTtoADC, j.at("TOTtoADC").get<std::vector<float>>());
    ok &= compareFloatVector(typeCode, "TOT_ped", module->TOT_ped, j.at("TOT_ped").get<std::vector<float>>());
    ok &= compareFloatVector(typeCode, "TOT_lin", module->TOT_lin, j.at("TOT_lin").get<std::vector<float>>());
    ok &= compareFloatVector(typeCode, "TOT_P0", module->TOT_P0, j.at("TOT_P0").get<std::vector<float>>());
    ok &= compareFloatVector(typeCode, "TOT_P1", module->TOT_P1, j.at("TOT_P1").get<std::vector<float>>());
    ok &= compareFloatVector(typeCode, "TOT_P2", module->TOT_P2, j.at("TOT_P2").get<std::vector<float>>());

    ok &= compareFloat2DVector(typeCode, "TOA_CTDC", module->TOA_CTDC, getFloat2DOrDefault(j, "TOA_CTDC", n, 32, 0.f));
    ok &= compareFloat2DVector(typeCode, "TOA_FTDC", module->TOA_FTDC, getFloat2DOrDefault(j, "TOA_FTDC", n, 8, 0.f));
    ok &= compareFloat2DVector(typeCode, "TOA_TW", module->TOA_TW, getFloat2DOrDefault(j, "TOA_TW", n, 3, 0.f));

    ok &= compareFloatVector(typeCode, "TOAtops", module->TOAtops, getFloatVectorOrDefault(j, "TOAtops", n, 1.f));
    ok &= compareFloatVector(typeCode, "MIPS_scale", module->MIPS_scale, j.at("MIPS_scale").get<std::vector<float>>());
  }

  edm::LogInfo("HGCalRecHitCalibrationDBReader")
      << "Closure comparison completed: "
      << (ok ? "no mismatches above tolerance" : "mismatches found");

  return ok;
}

DEFINE_FWK_MODULE(HGCalRecHitCalibrationDBReader);
