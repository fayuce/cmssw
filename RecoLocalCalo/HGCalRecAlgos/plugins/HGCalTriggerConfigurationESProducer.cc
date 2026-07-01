#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/Framework/interface/ModuleFactory.h"
#include "FWCore/Framework/interface/SourceFactory.h"
#include "FWCore/Framework/interface/ESProducer.h"
#include "FWCore/Framework/interface/EventSetupRecordIntervalFinder.h"
#include "FWCore/Framework/interface/ESHandle.h"
#include "FWCore/Framework/interface/ESProducts.h"
#include "FWCore/Utilities/interface/ESGetToken.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "FWCore/ParameterSet/interface/FileInPath.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "FWCore/Utilities/interface/Exception.h"

#include "CondFormats/HGCalObjects/interface/HGCalTriggerConfiguration.h"
#include "CondFormats/HGCalObjects/interface/HGCalMappingModuleIndexerTrigger.h"
#include "CondFormats/DataRecord/interface/HGCalElectronicsMappingRcd.h"
#include "CondFormats/DataRecord/interface/HGCalModuleConfigurationRcd.h"
#include "CondFormats/DataRecord/interface/HGCalTriggerConfigurationRcd.h"

#include "RecoLocalCalo/HGCalRecAlgos/interface/HGCalESProducerTools.h"

#include <fstream>
#include <optional>
#include <regex>
#include <string>
#include <vector>

class HGCalTriggerConfigurationESProducer : public edm::ESProducer, public edm::EventSetupRecordIntervalFinder {
public:
  explicit HGCalTriggerConfigurationESProducer(const edm::ParameterSet& iConfig)
      : useDB_(iConfig.existsAs<bool>("useDB") ? iConfig.getParameter<bool>("useDB") : false) {
    auto cc = setWhatProduced(this);

    if (useDB_) {
      configToken_ = cc.consumes();
    } else {
      fedjson_ = iConfig.getParameter<edm::FileInPath>("fedjson");
      modjson_ = iConfig.getParameter<edm::FileInPath>("modjson");
      indexToken_ = cc.consumes(iConfig.getParameter<edm::ESInputTag>("indexSource"));
    }
  }

  static void fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
    edm::ParameterSetDescription desc;
    desc.add<bool>("useDB", false)->setComment("If true, read HGCalTriggerConfiguration from CondDB/EventSetup");
    desc.add<edm::ESInputTag>("indexSource", edm::ESInputTag(""))
        ->setComment("Label for module indexer to set SoA size when useDB=False");
    desc.addOptional<edm::FileInPath>("fedjson")->setComment("JSON file with FED configuration parameters");
    desc.addOptional<edm::FileInPath>("modjson")->setComment("JSON file with ECONT configuration parameters");
    descriptions.addWithDefaultLabel(desc);
  }

  std::unique_ptr<HGCalTriggerConfiguration> produce(const HGCalModuleConfigurationRcd& iRecord) {
    if (useDB_) {
      const auto& config = iRecord.get(configToken_);
      edm::LogInfo("HGCalTriggerConfigurationESProducer")
          << "produce: loaded HGCalTriggerConfiguration from CondDB";
      return std::make_unique<HGCalTriggerConfiguration>(config);
    }

    auto const& moduleMap = iRecord.get(indexToken_);

    edm::LogInfo("HGCalTriggerConfigurationESProducer")
        << "produce: fedjson=" << fedjson_->fullPath() << ", modjson=" << modjson_->fullPath();

    const std::string fedjsonurl(fedjson_->fullPath());
    const std::string modjsonurl(modjson_->fullPath());

    std::ifstream fedfile(fedjsonurl);
    std::ifstream modfile(modjsonurl);

    if (!fedfile.is_open()) {
      throw cms::Exception("Configuration") << "Cannot open FED JSON file: " << fedjsonurl;
    }
    if (!modfile.is_open()) {
      throw cms::Exception("Configuration") << "Cannot open module JSON file: " << modjsonurl;
    }

    const json fed_config_data = json::parse(fedfile, nullptr, true, true);
    const json mod_config_data = json::parse(modfile, nullptr, true, true);

    const uint32_t nfeds = moduleMap.numFEDs();
    const std::vector<std::string> fedkeys = {"tdaqHeaderMarker", "neconts", "econtSwapOffset"};
    const std::vector<std::string> modkeys = {
        "density", "dropLSB", "select", "stc_type", "eporttx_numen", "use_sum", "calv", "mux"};

    if (nfeds != fed_config_data.size()) {
      edm::LogWarning("HGCalTriggerConfigurationESProducer")
          << "Total number of FEDs found in JSON file " << fedjsonurl << " (" << fed_config_data.size()
          << ") does not match indexer (" << nfeds << ")";
    }

    auto config = std::make_unique<HGCalTriggerConfiguration>();
    config->feds.resize(moduleMap.maxFEDSize());

    for (const auto& tfed : moduleMap.fedReadoutSequences()) {
      if (tfed.readoutTypes_.empty()) {
        continue;
      }

      const auto fedid = tfed.id;
      const auto fedkey = hgcal::search_fedkey(fedid, fed_config_data, fedjsonurl);
      hgcal::check_keys(fed_config_data, fedkey, fedkeys, fedjsonurl);

      const uint32_t nTDAQ = uint32_t(fed_config_data[fedkey]["neconts"].size());

      HGCalTriggerFedConfig fedConfig;

      fedConfig.econtSwapOffset.resize(moduleMap.getNumModules(fedid));
      for (std::size_t iecont = 0; iecont < moduleMap.getNumModules(fedid); ++iecont) {
        fedConfig.econtSwapOffset[iecont] = int32_t(fed_config_data[fedkey]["econtSwapOffset"][iecont]);
      }

      if (hgcal::check_keys(fed_config_data, fedkey, {"elinksMap"}, fedjsonurl)) {
        for (auto itdaq = fed_config_data[fedkey]["elinksMap"].begin();
             itdaq != fed_config_data[fedkey]["elinksMap"].end();
             ++itdaq) {
          const auto tdaqIndex = static_cast<uint8_t>(std::stoi(itdaq.key()));
          fedConfig.elinksMap[tdaqIndex].resize(itdaq.value().size());
          for (std::size_t ielink = 0; ielink < itdaq.value().size(); ++ielink) {
            fedConfig.elinksMap[tdaqIndex][ielink] = uint8_t(itdaq.value()[ielink]);
          }
        }
      }

      fedConfig.tdaqs.resize(nTDAQ);

      uint32_t totalECONTsBeforeTDAQ = 0;
      for (std::size_t itdaq = 0; itdaq < nTDAQ; ++itdaq) {
        HGCalTDAQConfig tdaqConfig;

        tdaqConfig.tdaqBlockHeaderMarker =
            std::stoul(std::string(fed_config_data[fedkey]["tdaqHeaderMarker"]), nullptr, 16);

        const uint32_t nECONT = uint32_t(fed_config_data[fedkey]["neconts"][itdaq]);
        tdaqConfig.econts.resize(nECONT);

        for (const auto& [typecode, ids] : moduleMap.typecodeMap()) {
          const auto [fedidFromMap, imod] = ids;

          if ((fedidFromMap != fedid) ||
              !(totalECONTsBeforeTDAQ <= imod && imod < totalECONTsBeforeTDAQ + nECONT)) {
            continue;
          }

          const auto modkey = hgcal::search_modkey(typecode, mod_config_data, modjsonurl);
          const bool isSiPM = std::regex_match(typecode, std::regex(R"(T[LH]-.*)"));

          if (isSiPM && nECONT != 2) {
            throw cms::Exception("Configuration")
                << "SiPM module " << modkey << " requires exactly 2 ECON-Ts, but nECONT = " << nECONT;
          }

          const uint32_t iecont = imod - totalECONTsBeforeTDAQ;

          if (!isSiPM) {
            hgcal::check_keys(mod_config_data, modkey, modkeys, modjsonurl);

            const size_t nTC_calv = mod_config_data[modkey]["calv"].size();
            const size_t nTC_mux = mod_config_data[modkey]["mux"].size();
            const size_t nTC = nTC_mux;

            if (nTC != nTC_mux || nTC != nTC_calv) {
              continue;
            }

            HGCalECONTConfig econtConfig;
            econtConfig.density = uint8_t(mod_config_data[modkey]["density"]);
            econtConfig.dropLSB = uint8_t(mod_config_data[modkey]["dropLSB"]);
            econtConfig.select = uint8_t(mod_config_data[modkey]["select"]);
            econtConfig.stcType = uint8_t(mod_config_data[modkey]["stc_type"]);
            econtConfig.eportTxNumen = uint8_t(mod_config_data[modkey]["eporttx_numen"]);
            econtConfig.sumType = uint8_t(mod_config_data[modkey]["use_sum"]);

            econtConfig.calv.resize(nTC);
            econtConfig.tcMux.resize(nTC);
            econtConfig.offset.resize(nTC);

            for (std::size_t iTC = 0; iTC < nTC; ++iTC) {
              econtConfig.calv[iTC] = uint16_t(mod_config_data[modkey]["calv"][iTC]);
              econtConfig.tcMux[iTC] = uint8_t(mod_config_data[modkey]["mux"][iTC]);
              econtConfig.offset[iTC] = calculateCellOffset();
            }

            tdaqConfig.econts[iecont] = econtConfig;
          } else {
            for (uint32_t econtIdx = 0; econtIdx < 2; ++econtIdx) {
              const auto& modcfg = mod_config_data[modkey][std::to_string(econtIdx)];

              const size_t nTC_calv = modcfg["calv"].size();
              const size_t nTC_mux = modcfg["mux"].size();
              const size_t nTC = nTC_mux;

              if (nTC != nTC_mux || nTC != nTC_calv) {
                continue;
              }

              HGCalECONTConfig econtConfig;
              econtConfig.density = uint8_t(modcfg["density"]);
              econtConfig.dropLSB = uint8_t(modcfg["dropLSB"]);
              econtConfig.select = uint8_t(modcfg["select"]);
              econtConfig.stcType = uint8_t(modcfg["stc_type"]);
              econtConfig.eportTxNumen = uint8_t(modcfg["eporttx_numen"]);
              econtConfig.sumType = uint8_t(modcfg["use_sum"]);

              econtConfig.calv.resize(nTC);
              econtConfig.tcMux.resize(nTC);
              econtConfig.offset.resize(nTC);

              for (std::size_t iTC = 0; iTC < nTC; ++iTC) {
                econtConfig.calv[iTC] = uint16_t(modcfg["calv"][iTC]);
                econtConfig.tcMux[iTC] = uint8_t(modcfg["mux"][iTC]);
                econtConfig.offset[iTC] = calculateCellOffset();
              }

              tdaqConfig.econts[econtIdx] = econtConfig;
            }
          }
        }

        fedConfig.tdaqs[itdaq] = tdaqConfig;
        totalECONTsBeforeTDAQ += uint32_t(fed_config_data[fedkey]["neconts"][itdaq]);
      }

      config->feds[fedid] = fedConfig;
    }

    LogDebug("HGCalTriggerConfigurationESProducer") << *config;
    return config;
  }

private:
  uint32_t calculateCellOffset() const { return 0; }

  void setIntervalFor(const edm::eventsetup::EventSetupRecordKey&,
                      const edm::IOVSyncValue&,
                      edm::ValidityInterval& iValidity) override {
    iValidity = edm::ValidityInterval(edm::IOVSyncValue::beginOfTime(), edm::IOVSyncValue::endOfTime());
  }

  edm::ESGetToken<HGCalMappingModuleIndexerTrigger, HGCalElectronicsMappingRcd> indexToken_;
  edm::ESGetToken<HGCalTriggerConfiguration, HGCalTriggerConfigurationRcd> configToken_;

  bool useDB_;
  std::optional<edm::FileInPath> fedjson_;
  std::optional<edm::FileInPath> modjson_;
};

DEFINE_FWK_EVENTSETUP_MODULE(HGCalTriggerConfigurationESProducer);
