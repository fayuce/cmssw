// HGCalRecHitCalibrationDBAnalyzer.cc
//
// EDAnalyzer:
//   level0 calibration JSON -> HGCalRecHitCalibrationConditions -> SQLite CondDB
//// 

#include "FWCore/Framework/interface/one/EDAnalyzer.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/EventSetup.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "FWCore/ServiceRegistry/interface/Service.h"
#include "FWCore/Utilities/interface/Exception.h"

#include "CondCore/DBOutputService/interface/PoolDBOutputService.h"
#include "CondFormats/HGCalObjects/interface/HGCalRecHitCalibrationConditions.h"

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include <algorithm>
#include <fstream>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

class HGCalRecHitCalibrationDBAnalyzer : public edm::one::EDAnalyzer<> {
public:
  explicit HGCalRecHitCalibrationDBAnalyzer(const edm::ParameterSet&);
  ~HGCalRecHitCalibrationDBAnalyzer() override = default;

  static void fillDescriptions(edm::ConfigurationDescriptions&);

private:
  void beginJob() override;
  void analyze(const edm::Event&, const edm::EventSetup&) override {}
  void endJob() override;

  HGCalRecHitCalibrationConditions readFromJSON(const std::string& filename) const;
  void validateModule(const HGCalRecHitCalibrationConditions::ModuleData& module) const;
  void printSummary(const HGCalRecHitCalibrationConditions& payload) const;
  void writeToCondDB(const HGCalRecHitCalibrationConditions& payload) const;

  static std::vector<float> getFloatVector(const json& j, const std::string& key);
  static std::vector<int32_t> getIntVector(const json& j, const std::string& key);
  static std::vector<std::vector<float>> getFloat2DOrDefault(
      const json& j,
      const std::string& key,
      std::size_t nrows,
      std::size_t width,
      float defaultValue);

  std::string m_jsonFile;
  std::string m_record;
  std::string m_tag;
  unsigned long long m_sinceRun;
  bool m_writeToCondDB;
  bool m_verbose;
};

HGCalRecHitCalibrationDBAnalyzer::HGCalRecHitCalibrationDBAnalyzer(const edm::ParameterSet& ps)
    : m_jsonFile(ps.getParameter<std::string>("jsonFile")),
      m_record(ps.getParameter<std::string>("record")),
      m_tag(ps.getParameter<std::string>("tag")),
      m_sinceRun(ps.getParameter<unsigned long long>("sinceRun")),
      m_writeToCondDB(ps.getParameter<bool>("writeToCondDB")),
      m_verbose(ps.getUntrackedParameter<bool>("verbose", false)) {}

void HGCalRecHitCalibrationDBAnalyzer::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
  edm::ParameterSetDescription desc;
  desc.add<std::string>("jsonFile", "level0_calib_hackathon.json");
  desc.add<std::string>("record", "HGCalRecHitCalibrationRcd");
  desc.add<std::string>("tag", "HGCalRecHitCalibration_test");
  desc.add<unsigned long long>("sinceRun", 1ULL);
  desc.add<bool>("writeToCondDB", false);
  desc.addUntracked<bool>("verbose", false);
  descriptions.addWithDefaultLabel(desc);
}

void HGCalRecHitCalibrationDBAnalyzer::beginJob() {
  edm::LogInfo("HGCalRecHitCalibrationDBAnalyzer")
      << "\n"
      << "\n  HGCal RecHit Calibration DB Analyzer"
      << "\n  jsonFile     : " << m_jsonFile
      << "\n  record       : " << m_record
      << "\n  tag          : " << m_tag
      << "\n  sinceRun     : " << m_sinceRun
      << "\n  writeToCondDB: " << (m_writeToCondDB ? "YES" : "NO (dry-run)")
      << "\n";
}

std::vector<float> HGCalRecHitCalibrationDBAnalyzer::getFloatVector(const json& j, const std::string& key) {
  if (!j.contains(key))
    throw cms::Exception("HGCalRecHitCalibrationDBAnalyzer") << "Missing required key '" << key << "'";
  return j.at(key).get<std::vector<float>>();
}

std::vector<int32_t> HGCalRecHitCalibrationDBAnalyzer::getIntVector(const json& j, const std::string& key) {
  if (!j.contains(key))
    throw cms::Exception("HGCalRecHitCalibrationDBAnalyzer") << "Missing required key '" << key << "'";
  return j.at(key).get<std::vector<int32_t>>();
}

std::vector<std::vector<float>> HGCalRecHitCalibrationDBAnalyzer::getFloat2DOrDefault(
    const json& j,
    const std::string& key,
    std::size_t nrows,
    std::size_t width,
    float defaultValue) {
  if (!j.contains(key))
    return std::vector<std::vector<float>>(nrows, std::vector<float>(width, defaultValue));

  return j.at(key).get<std::vector<std::vector<float>>>();
}

void HGCalRecHitCalibrationDBAnalyzer::validateModule(
    const HGCalRecHitCalibrationConditions::ModuleData& module) const {
  const std::size_t n = module.channel.size();

  auto check = [&](const std::string& name, std::size_t size) {
    if (size != n) {
      throw cms::Exception("HGCalRecHitCalibrationDBAnalyzer")
          << "Vector size mismatch for module " << module.typeCode
          << ", field '" << name << "': size=" << size
          << ", expected=" << n;
    }
  };

  check("valid", module.valid.size());
  check("ADC_ped", module.ADC_ped.size());
  check("Noise", module.Noise.size());
  check("CM_slope", module.CM_slope.size());
  check("CM_ped", module.CM_ped.size());
  check("BXm1_slope", module.BXm1_slope.size());
  check("BXm1_ped", module.BXm1_ped.size());
  check("TOTtoADC", module.TOTtoADC.size());
  check("TOT_ped", module.TOT_ped.size());
  check("TOT_lin", module.TOT_lin.size());
  check("TOT_P0", module.TOT_P0.size());
  check("TOT_P1", module.TOT_P1.size());
  check("TOT_P2", module.TOT_P2.size());
  check("TOA_CTDC", module.TOA_CTDC.size());
  check("TOA_FTDC", module.TOA_FTDC.size());
  check("TOA_TW", module.TOA_TW.size());
  check("TOAtops", module.TOAtops.size());
  check("MIPS_scale", module.MIPS_scale.size());
}

HGCalRecHitCalibrationConditions HGCalRecHitCalibrationDBAnalyzer::readFromJSON(
    const std::string& filename) const {
  json input;

  if (filename == "-" || filename == "stdin") {
    edm::LogInfo("HGCalRecHitCalibrationDBAnalyzer")
        << "Reading level0 calibration JSON from stdin";
    input = json::parse(std::cin);
  } else {
    std::ifstream infile(filename);
    if (!infile.is_open())
      throw cms::Exception("HGCalRecHitCalibrationDBAnalyzer") << "Cannot open JSON file: " << filename;

    input = json::parse(infile);
  }

  HGCalRecHitCalibrationConditions payload;
  payload.modules.reserve(input.size());

  for (const auto& item : input.items()) {
    const std::string& typeCode = item.key();
    const json& j = item.value();

    HGCalRecHitCalibrationConditions::ModuleData module;
    module.typeCode = typeCode;

    module.channel = getIntVector(j, "Channel");
    module.valid = getIntVector(j, "Valid");

    const std::size_t nrows = module.channel.size();

    module.ADC_ped = getFloatVector(j, "ADC_ped");
    module.Noise = getFloatVector(j, "Noise");
    module.CM_ped = getFloatVector(j, "CM_ped");
    module.CM_slope = getFloatVector(j, "CM_slope");
    module.BXm1_slope = getFloatVector(j, "BXm1_slope");
    module.BXm1_ped = getFloatVector(j, "BXm1_ped");

    module.TOTtoADC = getFloatVector(j, "TOTtoADC");
    module.TOT_ped = getFloatVector(j, "TOT_ped");
    module.TOT_lin = getFloatVector(j, "TOT_lin");
    module.TOT_P0 = getFloatVector(j, "TOT_P0");
    module.TOT_P1 = getFloatVector(j, "TOT_P1");
    module.TOT_P2 = getFloatVector(j, "TOT_P2");

    module.TOA_CTDC = getFloat2DOrDefault(j, "TOA_CTDC", nrows, 32, 0.f);
    module.TOA_FTDC = getFloat2DOrDefault(j, "TOA_FTDC", nrows, 8, 0.f);
    module.TOA_TW = getFloat2DOrDefault(j, "TOA_TW", nrows, 3, 0.f);

    if (j.contains("TOAtops")) {
      module.TOAtops = getFloatVector(j, "TOAtops");
    } else {
      module.TOAtops.assign(nrows, 1.f);
    }

    module.MIPS_scale = getFloatVector(j, "MIPS_scale");

    validateModule(module);

    edm::LogInfo("HGCalRecHitCalibrationDBAnalyzer")
        << "Loaded module " << module.typeCode
        << " with " << module.nChannels() << " channels";

    payload.modules.push_back(std::move(module));
  }

  return payload;
}

void HGCalRecHitCalibrationDBAnalyzer::printSummary(
    const HGCalRecHitCalibrationConditions& payload) const {
  edm::LogInfo("HGCalRecHitCalibrationDBAnalyzer")
      << "\n"
      << "\n  Summary"
      << "\n  nModules: " << payload.nModules()
      << "\n";

  for (const auto& module : payload.modules) {
    const int n = module.nChannels();
    int nValid = std::accumulate(module.valid.begin(), module.valid.end(), 0);

    float meanPed = n > 0 ? std::accumulate(module.ADC_ped.begin(), module.ADC_ped.end(), 0.f) / n : 0.f;
    float meanNoise = n > 0 ? std::accumulate(module.Noise.begin(), module.Noise.end(), 0.f) / n : 0.f;

    edm::LogInfo("HGCalRecHitCalibrationDBAnalyzer")
        << "Module: " << module.typeCode
        << " channels=" << n
        << " valid=" << nValid << "/" << n
        << " mean_ADC_ped=" << meanPed
        << " mean_Noise=" << meanNoise;

    if (m_verbose && n > 0) {
      edm::LogInfo("HGCalRecHitCalibrationDBAnalyzer")
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

void HGCalRecHitCalibrationDBAnalyzer::writeToCondDB(
    const HGCalRecHitCalibrationConditions& payload) const {
  edm::Service<cond::service::PoolDBOutputService> poolDbService;
  if (!poolDbService.isAvailable()) {
    throw cms::Exception("HGCalRecHitCalibrationDBAnalyzer")
        << "PoolDBOutputService is not available.";
  }

  poolDbService->writeOneIOV(payload, m_sinceRun, m_record);

  edm::LogInfo("HGCalRecHitCalibrationDBAnalyzer")
      << "CondDB write OK:"
      << "\n  record   : " << m_record
      << "\n  tag      : " << m_tag
      << "\n  sinceRun : " << m_sinceRun
      << "\n  modules  : " << payload.nModules();
}

void HGCalRecHitCalibrationDBAnalyzer::endJob() {
  auto payload = readFromJSON(m_jsonFile);
  printSummary(payload);

  if (m_writeToCondDB) {
    writeToCondDB(payload);
  } else {
    edm::LogInfo("HGCalRecHitCalibrationDBAnalyzer")
        << "writeToCondDB=False.";
  }
}

DEFINE_FWK_MODULE(HGCalRecHitCalibrationDBAnalyzer);
