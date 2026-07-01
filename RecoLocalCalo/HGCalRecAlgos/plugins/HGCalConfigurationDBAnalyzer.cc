#include "CondCore/DBOutputService/interface/PoolDBOutputService.h"
#include "CondFormats/DataRecord/interface/HGCalElectronicsMappingRcd.h"
#include "CondFormats/HGCalObjects/interface/HGCalConfiguration.h"
#include "CondFormats/HGCalObjects/interface/HGCalMappingModuleIndexer.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/EventSetup.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/Framework/interface/one/EDAnalyzer.h"
#include "FWCore/ParameterSet/interface/FileInPath.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ServiceRegistry/interface/Service.h"
#include "FWCore/Utilities/interface/ESGetToken.h"
#include "RecoLocalCalo/HGCalRecAlgos/interface/HGCalESProducerTools.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

class HGCalConfigurationDBAnalyzer : public edm::one::EDAnalyzer<> {
public:
  explicit HGCalConfigurationDBAnalyzer(const edm::ParameterSet& iConfig)
      : fedjson_(iConfig.getParameter<edm::FileInPath>("fedjson")),
        modjson_(iConfig.getParameter<edm::FileInPath>("modjson")),
        record_(iConfig.getParameter<std::string>("record")),
        tag_(iConfig.getParameter<std::string>("tag")),
        sinceRun_(iConfig.getParameter<unsigned long long>("sinceRun")),
        bePassthroughMode_(iConfig.getParameter<int32_t>("bePassthroughMode")),
        cbHeaderMarker_(iConfig.getParameter<int32_t>("cbHeaderMarker")),
        slinkHeaderMarker_(iConfig.getParameter<int32_t>("slinkHeaderMarker")),
        econdHeaderMarker_(iConfig.getParameter<int32_t>("econdHeaderMarker")),
        charMode_(iConfig.getParameter<int32_t>("charMode")),
        indexToken_(esConsumes<HGCalMappingModuleIndexer, HGCalElectronicsMappingRcd>(
            iConfig.getParameter<edm::ESInputTag>("indexSource"))) {}

  void analyze(const edm::Event&, const edm::EventSetup& iSetup) override {
    const auto& moduleMap = iSetup.getData(indexToken_);

    const std::string fedjsonurl = fedjson_.fullPath();
    const std::string modjsonurl = modjson_.fullPath();

    std::ifstream fedfile(fedjsonurl);
    std::ifstream modfile(modjsonurl);

    if (!fedfile.is_open()) {
      throw cms::Exception("HGCalConfigurationDBAnalyzer") << "Cannot open FED config JSON: " << fedjsonurl;
    }
    if (!modfile.is_open()) {
      throw cms::Exception("HGCalConfigurationDBAnalyzer") << "Cannot open ECOND config JSON: " << modjsonurl;
    }

    const json fed_config_data = json::parse(fedfile, nullptr, true, /*ignore_comments*/ true);
    const json mod_config_data = json::parse(modfile, nullptr, true, /*ignore_comments*/ true);

    auto config = buildConfiguration(moduleMap, fed_config_data, mod_config_data, fedjsonurl, modjsonurl);

    edm::Service<cond::service::PoolDBOutputService> poolDbService;
    if (!poolDbService.isAvailable()) {
      throw cms::Exception("HGCalConfigurationDBAnalyzer") << "PoolDBOutputService is not available";
    }

    poolDbService->writeOneIOV(*config, sinceRun_, record_);

    std::cout << "Wrote HGCalConfiguration to CondDB: record=" << record_ << ", tag=" << tag_
              << ", sinceRun=" << sinceRun_ << ", " << *config << std::endl;
  }

private:
  static int32_t gethex(const std::string& value, const int32_t value_override) {
    return (value_override >= 0 ? value_override : std::stoi(value, nullptr, 16));
  }

  static int32_t getint(const int32_t value, const int32_t value_override) {
    return (value_override >= 0 ? value_override : value);
  }

  std::unique_ptr<HGCalConfiguration> buildConfiguration(const HGCalMappingModuleIndexer& moduleMap,
                                                         const json& fed_config_data,
                                                         const json& mod_config_data,
                                                         const std::string& fedjsonurl,
                                                         const std::string& modjsonurl) const {
    uint32_t nfeds = moduleMap.numFEDs();
    uint32_t ntot_mods = 0;
    uint32_t ntot_rocs = 0;

    const std::vector<std::string> fedkeys = {"mismatchPassthroughMode", "cbHeaderMarker", "slinkHeaderMarker"};
    const std::vector<std::string> modkeys = {"headerMarker", "CalibrationSC"};

    if (nfeds != fed_config_data.size()) {
      std::cout << "Warning: total number of FEDs in JSON " << fedjsonurl << " (" << fed_config_data.size()
                << ") does not match indexer (" << nfeds << ")" << std::endl;
    }

    auto config = std::make_unique<HGCalConfiguration>();
    config->feds.resize(moduleMap.maxFEDSize());

    for (std::size_t fedid = 0; fedid < moduleMap.maxFEDSize(); ++fedid) {
      if (moduleMap.fedReadoutSequences()[fedid].readoutTypes_.empty()) {
        continue;
      }

      const auto fedkey = hgcal::search_fedkey(fedid, fed_config_data, fedjsonurl);
      hgcal::check_keys(fed_config_data, fedkey, fedkeys, fedjsonurl);

      HGCalFedConfig fed;
      fed.mismatchPassthroughMode =
          getint(fed_config_data[fedkey]["mismatchPassthroughMode"], bePassthroughMode_);
      fed.cbHeaderMarker = gethex(fed_config_data[fedkey]["cbHeaderMarker"], cbHeaderMarker_);
      fed.slinkHeaderMarker = gethex(fed_config_data[fedkey]["slinkHeaderMarker"], slinkHeaderMarker_);

      for (const auto& [typecode, ids] : moduleMap.typecodeMap()) {
        auto [fedid_, imod] = ids;
        if (fedid_ != fedid) {
          continue;
        }

        ntot_mods++;

        const auto modkey = hgcal::search_modkey(typecode, mod_config_data, modjsonurl);
        hgcal::check_keys(mod_config_data, modkey, modkeys, modjsonurl);

        if (imod >= fed.econds.size()) {
          fed.econds.resize(imod + 1);
        }

        HGCalECONDConfig mod;
        mod.headerMarker = gethex(mod_config_data[modkey]["headerMarker"], econdHeaderMarker_);

        uint32_t nrocs = moduleMap.getNumERxs(fedid, imod);
        uint32_t nrocs2 = mod_config_data[modkey]["CalibrationSC"].size();

        if (nrocs != nrocs2) {
          std::cout << "Warning: number of eRx ROCs for ECON-D " << typecode << " in " << modjsonurl << " ("
                    << nrocs2 << ") does not match indexer for fedid=" << fedid << ", imod=" << imod << " ("
                    << nrocs << ")" << std::endl;
        }

        mod.rocs.resize(nrocs);

        mod.enabledErx = (0b1 << nrocs) - 0b1;
        if (mod_config_data[modkey].count("enabledErx") > 0) {
          mod.enabledErx = gethex(mod_config_data[modkey]["enabledErx"], -1);
        }

        for (uint32_t iroc = 0; iroc < nrocs; ++iroc) {
          ntot_rocs++;
          HGCalROCConfig roc;
          roc.charMode = getint(mod_config_data[modkey]["CalibrationSC"][iroc], charMode_);
          roc.muxMode = -1;
          mod.rocs[iroc] = roc;
        }

        fed.econds[imod] = mod;
      }

      config->feds[fedid] = fed;
    }

    if (ntot_mods != moduleMap.maxModulesCount()) {
      std::cout << "Warning: total number of ECON-D modules filled (" << ntot_mods << ") does not match indexer ("
                << moduleMap.maxModulesCount() << ")" << std::endl;
    }

    if (ntot_rocs != moduleMap.maxERxSize()) {
      std::cout << "Warning: total number of eRx half-ROCs filled (" << ntot_rocs << ") does not match indexer ("
                << moduleMap.maxERxSize() << ")" << std::endl;
    }

    return config;
  }

  const edm::FileInPath fedjson_;
  const edm::FileInPath modjson_;
  const std::string record_;
  const std::string tag_;
  const unsigned long long sinceRun_;
  const int32_t bePassthroughMode_;
  const int32_t cbHeaderMarker_;
  const int32_t slinkHeaderMarker_;
  const int32_t econdHeaderMarker_;
  const int32_t charMode_;
  const edm::ESGetToken<HGCalMappingModuleIndexer, HGCalElectronicsMappingRcd> indexToken_;
};

DEFINE_FWK_MODULE(HGCalConfigurationDBAnalyzer);
