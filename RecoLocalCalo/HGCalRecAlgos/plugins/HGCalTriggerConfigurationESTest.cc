#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/EventSetup.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/Framework/interface/one/EDAnalyzer.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "FWCore/Utilities/interface/ESGetToken.h"

#include "CondFormats/DataRecord/interface/HGCalModuleConfigurationRcd.h"
#include "CondFormats/HGCalObjects/interface/HGCalTriggerConfiguration.h"

#include <iostream>

class HGCalTriggerConfigurationESTest : public edm::one::EDAnalyzer<> {
public:
  explicit HGCalTriggerConfigurationESTest(const edm::ParameterSet&)
      : token_(esConsumes<HGCalTriggerConfiguration, HGCalModuleConfigurationRcd>()) {}

  static void fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
    edm::ParameterSetDescription desc;
    descriptions.addWithDefaultLabel(desc);
  }

private:
  void analyze(const edm::Event&, const edm::EventSetup& iSetup) override {
    const auto& config = iSetup.getData(token_);

    uint32_t nfeds = 0;
    uint32_t ntdaq = 0;
    uint32_t necont = 0;

    for (const auto& fed : config.feds) {
      if (fed.tdaqs.empty() && fed.econtSwapOffset.empty() && fed.elinksMap.empty()) {
        continue;
      }
      ++nfeds;
      ntdaq += fed.tdaqs.size();
      for (const auto& tdaq : fed.tdaqs) {
        necont += tdaq.econts.size();
      }
    }

    std::cout << "HGCalTriggerConfigurationESTest: config from ESProducer: "
              << "nfeds=" << nfeds
              << ", ntdaq=" << ntdaq
              << ", necont=" << necont
              << std::endl;
  }

  edm::ESGetToken<HGCalTriggerConfiguration, HGCalModuleConfigurationRcd> token_;
};

DEFINE_FWK_MODULE(HGCalTriggerConfigurationESTest);
