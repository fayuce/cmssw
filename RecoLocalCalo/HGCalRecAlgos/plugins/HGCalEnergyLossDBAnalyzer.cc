// HGCalEnergyLossDBAnalyzer.cc
//
// EDAnalyzer:
//   EnergyLoss JSON -> HGCalEnergyLossConditions -> SQLite CondDB

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
#include "CondFormats/HGCalObjects/interface/HGCalEnergyLossConditions.h"

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include <fstream>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

class HGCalEnergyLossDBAnalyzer : public edm::one::EDAnalyzer<> {
public:
  explicit HGCalEnergyLossDBAnalyzer(const edm::ParameterSet&);
  ~HGCalEnergyLossDBAnalyzer() override = default;

  static void fillDescriptions(edm::ConfigurationDescriptions&);

private:
  void beginJob() override;
  void analyze(const edm::Event&, const edm::EventSetup&) override {}
  void endJob() override;

  HGCalEnergyLossConditions readFromJSON(const std::string& filename) const;
  void validate(const HGCalEnergyLossConditions& payload) const;
  void printSummary(const HGCalEnergyLossConditions& payload) const;
  void writeToCondDB(const HGCalEnergyLossConditions& payload) const;

  static std::vector<float> getFloatVector(const json& j, const std::string& key);

  std::string m_jsonFile;
  std::string m_record;
  std::string m_tag;
  unsigned long long m_sinceRun;
  bool m_writeToCondDB;
  bool m_verbose;
};

HGCalEnergyLossDBAnalyzer::HGCalEnergyLossDBAnalyzer(const edm::ParameterSet& ps)
    : m_jsonFile(ps.getParameter<std::string>("jsonFile")),
      m_record(ps.getParameter<std::string>("record")),
      m_tag(ps.getParameter<std::string>("tag")),
      m_sinceRun(ps.getParameter<unsigned long long>("sinceRun")),
      m_writeToCondDB(ps.getParameter<bool>("writeToCondDB")),
      m_verbose(ps.getUntrackedParameter<bool>("verbose", false)) {}

void HGCalEnergyLossDBAnalyzer::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
  edm::ParameterSetDescription desc;
  desc.add<std::string>("jsonFile", "hgcal_energyloss_v16.json");
  desc.add<std::string>("record", "HGCalEnergyLossRcd");
  desc.add<std::string>("tag", "HGCalEnergyLoss_test");
  desc.add<unsigned long long>("sinceRun", 1ULL);
  desc.add<bool>("writeToCondDB", false);
  desc.addUntracked<bool>("verbose", false);
  descriptions.addWithDefaultLabel(desc);
}

void HGCalEnergyLossDBAnalyzer::beginJob() {
  edm::LogInfo("HGCalEnergyLossDBAnalyzer")
      << "\n"
      << "\n  HGCal EnergyLoss DB Analyzer"
      << "\n  jsonFile     : " << m_jsonFile
      << "\n  record       : " << m_record
      << "\n  tag          : " << m_tag
      << "\n  sinceRun     : " << m_sinceRun
      << "\n  writeToCondDB: " << (m_writeToCondDB ? "YES" : "NO (dry-run)")
      << "\n";
}

std::vector<float> HGCalEnergyLossDBAnalyzer::getFloatVector(const json& j, const std::string& key) {
  if (!j.contains(key)) {
    throw cms::Exception("HGCalEnergyLossDBAnalyzer")
        << "Missing required key '" << key << "'";
  }
  return j.at(key).get<std::vector<float>>();
}

HGCalEnergyLossConditions HGCalEnergyLossDBAnalyzer::readFromJSON(const std::string& filename) const {
  json input;

  if (filename == "-" || filename == "stdin") {
    edm::LogInfo("HGCalEnergyLossDBAnalyzer")
        << "Reading EnergyLoss JSON from stdin";
    input = json::parse(std::cin);
  } else {
    std::ifstream infile(filename);
    if (!infile.is_open()) {
      throw cms::Exception("HGCalEnergyLossDBAnalyzer")
          << "Cannot open EnergyLoss JSON file: " << filename;
    }
    input = json::parse(infile, nullptr, true, /*ignore_comments*/ true);
  }

  HGCalEnergyLossConditions payload;
  payload.dEdx = getFloatVector(input, "dEdx");
  payload.SF_thickness_Si = getFloatVector(input, "SF_thickness_Si");
  payload.SF_thickness_SiPM = getFloatVector(input, "SF_thickness_SiPM");

  validate(payload);
  return payload;
}

void HGCalEnergyLossDBAnalyzer::validate(const HGCalEnergyLossConditions& payload) const {
  if (payload.dEdx.size() != 47) {
    edm::LogWarning("HGCalEnergyLossDBAnalyzer")
        << "Expected 47 dEdx layer entries, got " << payload.dEdx.size();
  }

  if (payload.SF_thickness_Si.size() != 6) {
    throw cms::Exception("HGCalEnergyLossDBAnalyzer")
        << "Expected 6 SF_thickness_Si entries, got "
        << payload.SF_thickness_Si.size();
  }

  if (payload.SF_thickness_SiPM.size() != 1) {
    throw cms::Exception("HGCalEnergyLossDBAnalyzer")
        << "Expected 1 SF_thickness_SiPM entry, got "
        << payload.SF_thickness_SiPM.size();
  }
}

void HGCalEnergyLossDBAnalyzer::printSummary(const HGCalEnergyLossConditions& payload) const {
  auto mean = [](const std::vector<float>& v) -> float {
    return v.empty() ? 0.f : std::accumulate(v.begin(), v.end(), 0.f) / v.size();
  };

  edm::LogInfo("HGCalEnergyLossDBAnalyzer")
      << "\n"
      << "\n  EnergyLoss Summary"
      << "\n  dEdx entries             : " << payload.dEdx.size()
      << "\n  SF_thickness_Si entries  : " << payload.SF_thickness_Si.size()
      << "\n  SF_thickness_SiPM entries: " << payload.SF_thickness_SiPM.size()
      << "\n  mean dEdx                : " << mean(payload.dEdx)
      << "\n";

  if (m_verbose && !payload.dEdx.empty()) {
    edm::LogInfo("HGCalEnergyLossDBAnalyzer")
        << "  first dEdx=" << payload.dEdx.front()
        << " last dEdx=" << payload.dEdx.back()
        << " first SF_thickness_Si=" << payload.SF_thickness_Si.front()
        << " SF_thickness_SiPM=" << payload.SF_thickness_SiPM.front();
  }
}

void HGCalEnergyLossDBAnalyzer::writeToCondDB(const HGCalEnergyLossConditions& payload) const {
  edm::Service<cond::service::PoolDBOutputService> poolDbService;

  if (!poolDbService.isAvailable()) {
    throw cms::Exception("HGCalEnergyLossDBAnalyzer")
        << "PoolDBOutputService is not available.";
  }

  if (poolDbService->isNewTagRequest(m_record)) {
    poolDbService->createOneIOV<HGCalEnergyLossConditions>(payload, m_sinceRun, m_record);
  } else {
    poolDbService->appendOneIOV<HGCalEnergyLossConditions>(payload, m_sinceRun, m_record);
  }

  edm::LogInfo("HGCalEnergyLossDBAnalyzer")
      << "\n"
      << "\n  CondDB write OK:"
      << "\n  record   : " << m_record
      << "\n  tag      : " << m_tag
      << "\n  sinceRun : " << m_sinceRun
      << "\n  nLayers  : " << payload.nLayers()
      << "\n";

  std::cout << "\n  CondDB write OK:"
            << "\n  record   : " << m_record
            << "\n  tag      : " << m_tag
            << "\n  sinceRun : " << m_sinceRun
            << "\n  nLayers  : " << payload.nLayers()
            << "\n";
}

void HGCalEnergyLossDBAnalyzer::endJob() {
  const auto payload = readFromJSON(m_jsonFile);
  printSummary(payload);

  if (m_writeToCondDB) {
    writeToCondDB(payload);
  } else {
    edm::LogInfo("HGCalEnergyLossDBAnalyzer")
        << "writeToCondDB=False.";
  }
}

DEFINE_FWK_MODULE(HGCalEnergyLossDBAnalyzer);
