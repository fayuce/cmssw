#include "CondCore/DBOutputService/interface/PoolDBOutputService.h"
#include "CondFormats/HGCalObjects/interface/HGCalConfigurationTemplateConditions.h"
#include "FWCore/Framework/interface/one/EDAnalyzer.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/ParameterSet/interface/FileInPath.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ServiceRegistry/interface/Service.h"
#include "FWCore/Utilities/interface/Exception.h"

#include <fstream>
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
    HGCalConfigurationTemplateConditions payload;
    payload.fedJson = readFileToString(fedjson_);
    payload.modJson = readFileToString(modjson_);

    edm::Service<cond::service::PoolDBOutputService> poolDbService;
    if (!poolDbService.isAvailable()) {
      throw cms::Exception("HGCalConfigurationTemplateDBAnalyzer")
          << "PoolDBOutputService is not available";
    }

    poolDbService->writeOneIOV(payload, sinceRun_, record_);

    std::cout << "Wrote HGCalConfigurationTemplateConditions to CondDB: record="
              << record_ << ", tag=" << tag_ << ", sinceRun=" << sinceRun_
              << ", fedJsonSize=" << payload.fedJson.size()
              << ", modJsonSize=" << payload.modJson.size() << std::endl;
  }

private:
  edm::FileInPath fedjson_;
  edm::FileInPath modjson_;
  std::string record_;
  std::string tag_;
  unsigned long long sinceRun_;
};

DEFINE_FWK_MODULE(HGCalConfigurationTemplateDBAnalyzer);
