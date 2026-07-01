#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/EventSetup.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/Framework/interface/one/EDAnalyzer.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "FWCore/Utilities/interface/ESGetToken.h"

#include "CondFormats/DataRecord/interface/HGCalTriggerConfigurationRcd.h"
#include "CondFormats/HGCalObjects/interface/HGCalTriggerConfiguration.h"

#include <iostream>

class HGCalTriggerConfigurationDBReader : public edm::one::EDAnalyzer<> {
public:
  explicit HGCalTriggerConfigurationDBReader(const edm::ParameterSet&)
      : token_(esConsumes<HGCalTriggerConfiguration, HGCalTriggerConfigurationRcd>()) {}

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

    std::cout << "Read HGCalTriggerConfiguration from CondDB: "
              << "nfeds=" << nfeds
              << ", ntdaq=" << ntdaq
              << ", necont=" << necont
              << std::endl;
  }

  edm::ESGetToken<HGCalTriggerConfiguration, HGCalTriggerConfigurationRcd> token_;
};

DEFINE_FWK_MODULE(HGCalTriggerConfigurationDBReader);
