#include "CondCore/DBOutputService/interface/PoolDBOutputService.h"
#include "CondFormats/HGCalObjects/interface/HGCalTriggerConfigurationTemplateConditions.h"
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
      throw cms::Exception("HGCalTriggerConfigurationTemplateDBAnalyzer")
          << "Cannot open JSON file: " << fileInPath.fullPath();
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
  }

  HGCalTriggerECONTTemplate makeECONTTemplate(const json& j) {
    HGCalTriggerECONTTemplate e;
    e.density = j.at("density").get<uint8_t>();
    e.dropLSB = j.at("dropLSB").get<uint8_t>();
    e.select = j.at("select").get<uint8_t>();
    e.stcType = j.at("stc_type").get<uint8_t>();
    e.eportTxNumen = j.at("eporttx_numen").get<uint8_t>();
    e.sumType = j.at("use_sum").get<uint8_t>();
    e.calv = j.at("calv").get<std::vector<uint16_t>>();
    e.mux = j.at("mux").get<std::vector<uint8_t>>();
    return e;
  }
}

class HGCalTriggerConfigurationTemplateDBAnalyzer : public edm::one::EDAnalyzer<> {
public:
  explicit HGCalTriggerConfigurationTemplateDBAnalyzer(const edm::ParameterSet& iConfig)
      : fedjson_(iConfig.getParameter<edm::FileInPath>("fedjson")),
        modjson_(iConfig.getParameter<edm::FileInPath>("modjson")),
        record_(iConfig.getParameter<std::string>("record")),
        tag_(iConfig.getParameter<std::string>("tag")),
        sinceRun_(iConfig.getParameter<unsigned long long>("sinceRun")) {}

  void analyze(const edm::Event&, const edm::EventSetup&) override {
    const auto fedData = json::parse(readFileToString(fedjson_), nullptr, true, true);
    const auto modData = json::parse(readFileToString(modjson_), nullptr, true, true);

    HGCalTriggerConfigurationTemplateConditions payload;

    for (auto it = fedData.begin(); it != fedData.end(); ++it) {
      const auto& jf = it.value();

      HGCalTriggerFedTemplate fed;

      if (it.key() == "*") {
        fed.isWildcard = true;
        fed.fedId = 0;
      } else if (jf.contains("fedId")) {
        fed.fedId = jf.at("fedId").get<uint32_t>();
      } else if (jf.contains("fedid")) {
        fed.fedId = jf.at("fedid").get<uint32_t>();
      } else if (jf.contains("fed")) {
        fed.fedId = jf.at("fed").get<uint32_t>();
      } else {
        try {
          fed.fedId = std::stoul(it.key());
        } catch (const std::exception&) {
          throw cms::Exception("HGCalTriggerConfigurationTemplateDBAnalyzer")
              << "Cannot determine FED id from key '" << it.key()
              << "'. Add fedId/fedid/fed field support matching the JSON structure.";
        }
      }

      fed.tdaqHeaderMarker = jf.at("tdaqHeaderMarker").get<std::string>();
      fed.neconts = jf.at("neconts").get<std::vector<uint32_t>>();
      fed.econtSwapOffset = jf.at("econtSwapOffset").get<std::vector<int32_t>>();

      if (jf.contains("elinksMap")) {
        for (auto el = jf.at("elinksMap").begin(); el != jf.at("elinksMap").end(); ++el) {
          const auto tdaqIndex = static_cast<uint8_t>(std::stoi(el.key()));
          fed.elinksMap[tdaqIndex] = el.value().get<std::vector<uint8_t>>();
        }
      }

      payload.feds.push_back(fed);
    }

    for (auto it = modData.begin(); it != modData.end(); ++it) {
      const auto& jm = it.value();

      HGCalTriggerModuleTemplate mod;

      if (jm.contains("density")) {
        mod.econts.push_back(makeECONTTemplate(jm));
      } else {
        for (auto econtIt = jm.begin(); econtIt != jm.end(); ++econtIt) {
          mod.econts.push_back(makeECONTTemplate(econtIt.value()));
        }
      }

      payload.modules[it.key()] = mod;
    }

    edm::Service<cond::service::PoolDBOutputService> poolDbService;
    if (!poolDbService.isAvailable()) {
      throw cms::Exception("HGCalTriggerConfigurationTemplateDBAnalyzer")
          << "PoolDBOutputService is not available";
    }

    poolDbService->writeOneIOV(payload, sinceRun_, record_);

    std::cout << "Wrote HGCalTriggerConfigurationTemplateConditions C++ payload to CondDB: record="
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

DEFINE_FWK_MODULE(HGCalTriggerConfigurationTemplateDBAnalyzer);
