#include "CondFormats/DataRecord/interface/HGCalConfigurationRcd.h"
#include "CondFormats/HGCalObjects/interface/HGCalConfiguration.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/EventSetup.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/Framework/interface/one/EDAnalyzer.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/Utilities/interface/ESGetToken.h"

#include <iostream>

class HGCalConfigurationDBReader : public edm::one::EDAnalyzer<> {
public:
  explicit HGCalConfigurationDBReader(const edm::ParameterSet& iConfig)
      : configToken_(esConsumes<HGCalConfiguration, HGCalConfigurationRcd>(
            iConfig.getParameter<edm::ESInputTag>("configSource"))) {}

  void analyze(const edm::Event&, const edm::EventSetup& iSetup) override {
    const auto& config = iSetup.getData(configToken_);
    std::cout << "Read HGCalConfiguration from CondDB: " << config << std::endl;
  }

private:
  const edm::ESGetToken<HGCalConfiguration, HGCalConfigurationRcd> configToken_;
};

DEFINE_FWK_MODULE(HGCalConfigurationDBReader);
