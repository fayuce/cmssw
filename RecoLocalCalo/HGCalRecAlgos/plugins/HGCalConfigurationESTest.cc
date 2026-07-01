#include "FWCore/Framework/interface/one/EDAnalyzer.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/EventSetup.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/Utilities/interface/ESGetToken.h"

#include "CondFormats/HGCalObjects/interface/HGCalConfiguration.h"
#include "CondFormats/DataRecord/interface/HGCalModuleConfigurationRcd.h"

#include <iostream>

class HGCalConfigurationESTest : public edm::one::EDAnalyzer<> {
public:
  explicit HGCalConfigurationESTest(const edm::ParameterSet&) {
    configToken_ = esConsumes<HGCalConfiguration, HGCalModuleConfigurationRcd>();
  }

  void analyze(const edm::Event&, const edm::EventSetup& iSetup) override {
    const auto& config = iSetup.getData(configToken_);

    std::size_t nfeds = 0;
    std::size_t ntotmod = 0;
    std::size_t ntotroc = 0;

    for (const auto& fed : config.feds) {
      if (fed.econds.empty())
        continue;

      ++nfeds;

      for (const auto& econd : fed.econds) {
        if (econd.rocs.empty())
          continue;

        ++ntotmod;
        ntotroc += econd.rocs.size();
      }
    }

    std::cout << "HGCalConfigurationESTest: config from ESProducer: "
              << "nfed=" << nfeds
              << ", ntotmod=" << ntotmod
              << ", ntotroc=" << ntotroc
              << std::endl;
  }

private:
  edm::ESGetToken<HGCalConfiguration, HGCalModuleConfigurationRcd> configToken_;
};

DEFINE_FWK_MODULE(HGCalConfigurationESTest);
