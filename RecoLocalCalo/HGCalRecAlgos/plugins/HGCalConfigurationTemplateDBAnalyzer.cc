#include "CondCore/DBOutputService/interface/PoolDBOutputService.h"
#include "CondFormats/HGCalObjects/interface/HGCalConfigurationTemplateConditions.h"
#include "FWCore/Framework/interface/one/EDAnalyzer.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/ParameterSet/interface/FileInPath.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ServiceRegistry/interface/Service.h"
#include "FWCore/Utilities/interface/Exception.h"
#include "RecoLocalCalo/HGCalRecAlgos/interface/HGCalESProducerTools.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {
  std::string readFileToString(const edm::FileInPath& fileInPath) {
    std::ifstream input(fileInPath.fullPath());
    if (!input.is_open()) {
      throw cms::Exception("HGCalConfigurationTemplateDBAnalyzer")
          << "Cannot open JSON file: " << fileInPath.fullPath();
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
  }
}

class HGCalConfigurationTemplateDBAnalyzer : public edm::one::EDAnalyzer<> {
public:
  explicit HGCalConfigurationTemplateDBAnalyzer(const edm::ParameterSet& iConfig)
      : fedjson_(iConfig.getParameter<edm::FileInPath>("fedjson")),
        modjson_(iConfig.getParameter<edm::FileInPath>("modjson")),
        record_(iConfig.getParameter<std::string>("record")),
        tag_(iConfig.getParameter<std::string>("tag")),
        sinceRun_(iConfig.getParameter<unsigned long long>("sinceRun")) {}

  void analyze(const edm::Event&, const edm::EventSetup&) override {
    const auto fedData = json::parse(readFileToString(fedjson_), nullptr, true, true);
    const auto modData = json::parse(readFileToString(modjson_), nullptr, true, true);

    HGCalConfigurationTemplateConditions payload;

    for (auto it = fedData.begin(); it != fedData.end(); ++it) {
      const auto& jf = it.value();

      HGCalFedConfigTemplate fed;

      if (it.key() == "*") {
        fed.isWildcard = true;
        fed.fedId = 0;
      } else {
        fed.fedId = std::stoul(it.key());
      }

      fed.mismatchPassthroughMode = jf.at("mismatchPassthroughMode").get<int32_t>();
      fed.cbHeaderMarker = jf.at("cbHeaderMarker").get<std::string>();
      fed.slinkHeaderMarker = jf.at("slinkHeaderMarker").get<std::string>();

      payload.feds.push_back(fed);
    }

    for (auto it = modData.begin(); it != modData.end(); ++it) {
      const auto& jm = it.value();

      HGCalECONDConfigTemplate mod;
      mod.headerMarker = jm.at("headerMarker").get<std::string>();
      mod.calibrationSC = jm.at("CalibrationSC").get<std::vector<int32_t>>();

      if (jm.contains("MultiPlex")) {
        mod.hasMultiPlex = true;
        mod.multiPlex = jm.at("MultiPlex").get<std::vector<int32_t>>();
      }

      if (jm.contains("enabledErx")) {
        mod.hasEnabledErx = true;
        mod.enabledErx = std::stoi(jm.at("enabledErx").get<std::string>(), nullptr, 16);
      }

      payload.modules[it.key()] = mod;
    }

    edm::Service<cond::service::PoolDBOutputService> poolDbService;
    if (!poolDbService.isAvailable()) {
      throw cms::Exception("HGCalConfigurationTemplateDBAnalyzer")
          << "PoolDBOutputService is not available";
    }

    poolDbService->writeOneIOV(payload, sinceRun_, record_);

    std::cout << "Wrote HGCalConfigurationTemplateConditions C++ payload to CondDB: record="
              << record_ << ", tag=" << tag_ << ", sinceRun=" << sinceRun_
              << ", nFeds=" << payload.feds.size()
              << ", nModuleTemplates=" << payload.modules.size() << std::endl;
  }

private:
  edm::FileInPath fedjson_;
  edm::FileInPath modjson_;
  std::string record_;
  std::string tag_;
  unsigned long long sinceRun_;
};

DEFINE_FWK_MODULE(HGCalConfigurationTemplateDBAnalyzer);
