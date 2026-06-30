// HGCalEnergyLossDBReader.cc
//
// Read HGCalEnergyLossConditions from SQLite CondDB
// and compare it with the reference EnergyLoss JSON.

#include "FWCore/Framework/interface/one/EDAnalyzer.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/EventSetup.h"
#include "FWCore/Framework/interface/ESHandle.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "FWCore/Utilities/interface/ESGetToken.h"
#include "FWCore/Utilities/interface/Exception.h"

#include "CondFormats/DataRecord/interface/HGCalEnergyLossRcd.h"
#include "CondFormats/HGCalObjects/interface/HGCalEnergyLossConditions.h"

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include <cmath>
#include <fstream>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

class HGCalEnergyLossDBReader : public edm::one::EDAnalyzer<> {
public:
  explicit HGCalEnergyLossDBReader(const edm::ParameterSet&);
  ~HGCalEnergyLossDBReader() override = default;

  static void fillDescriptions(edm::ConfigurationDescriptions& descriptions);

private:
  void analyze(const edm::Event&, const edm::EventSetup&) override;

  void printConditions(const HGCalEnergyLossConditions& cond) const;
  bool compareWithJson(const HGCalEnergyLossConditions& cond) const;
  bool compareFloatVector(const std::string& field,
                          const std::vector<float>& db,
                          const std::vector<float>& ref) const;

  edm::ESGetToken<HGCalEnergyLossConditions, HGCalEnergyLossRcd> energyLossToken_;
  std::string refJsonFile_;
  double tolerance_;
  bool verbose_;
  bool done_;
};

HGCalEnergyLossDBReader::HGCalEnergyLossDBReader(const edm::ParameterSet& ps)
    : energyLossToken_(esConsumes<HGCalEnergyLossConditions, HGCalEnergyLossRcd>()),
      refJsonFile_(ps.getParameter<std::string>("refJsonFile")),
      tolerance_(ps.getParameter<double>("tolerance")),
      verbose_(ps.getUntrackedParameter<bool>("verbose", false)),
      done_(false) {}

void HGCalEnergyLossDBReader::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
  edm::ParameterSetDescription desc;
  desc.add<std::string>("refJsonFile", "");
  desc.add<double>("tolerance", 1e-6);
  desc.addUntracked<bool>("verbose", false);
  descriptions.addWithDefaultLabel(desc);
}

void HGCalEnergyLossDBReader::analyze(const edm::Event&, const edm::EventSetup& iSetup) {
  if (done_)
    return;
  done_ = true;

  auto handle = iSetup.getHandle(energyLossToken_);

  if (!handle.isValid()) {
    throw cms::Exception("HGCalEnergyLossDBReader")
        << "Failed to get HGCalEnergyLossConditions from EventSetup using HGCalEnergyLossRcd.";
  }

  const auto& conditions = *handle;
  printConditions(conditions);

  if (!refJsonFile_.empty()) {
    const bool ok = compareWithJson(conditions);

    edm::LogInfo("HGCalEnergyLossDBReader")
        << "\n=========================================="
        << "\n  EnergyLoss Closure Result: "
        << (ok ? "*** PASSED ***" : "*** FAILED ***")
        << "\n==========================================";

    std::cout << "\n=========================================="
              << "\n  EnergyLoss Closure Result: "
              << (ok ? "*** PASSED ***" : "*** FAILED ***")
              << "\n==========================================\n";

    if (!ok) {
      throw cms::Exception("ClosureTestFailed")
          << "HGCal EnergyLoss closure test failed.";
    }
  }
}

void HGCalEnergyLossDBReader::printConditions(const HGCalEnergyLossConditions& cond) const {
  auto mean = [](const std::vector<float>& v) -> float {
    return v.empty() ? 0.f : std::accumulate(v.begin(), v.end(), 0.f) / v.size();
  };

  edm::LogInfo("HGCalEnergyLossDBReader")
      << "\n=========================================="
      << "\n  HGCal EnergyLoss Readback"
      << "\n  dEdx entries             : " << cond.dEdx.size()
      << "\n  SF_thickness_Si entries  : " << cond.SF_thickness_Si.size()
      << "\n  SF_thickness_SiPM entries: " << cond.SF_thickness_SiPM.size()
      << "\n  mean dEdx                : " << mean(cond.dEdx)
      << "\n==========================================";

  std::cout << "\n=========================================="
            << "\n  HGCal EnergyLoss Readback"
            << "\n  dEdx entries             : " << cond.dEdx.size()
            << "\n  SF_thickness_Si entries  : " << cond.SF_thickness_Si.size()
            << "\n  SF_thickness_SiPM entries: " << cond.SF_thickness_SiPM.size()
            << "\n  mean dEdx                : " << mean(cond.dEdx)
            << "\n==========================================\n";

  if (verbose_ && !cond.dEdx.empty()) {
    edm::LogInfo("HGCalEnergyLossDBReader")
        << "first dEdx=" << cond.dEdx.front()
        << " last dEdx=" << cond.dEdx.back()
        << " first SF_thickness_Si=" << cond.SF_thickness_Si.front()
        << " SF_thickness_SiPM=" << cond.SF_thickness_SiPM.front();
  }
}

bool HGCalEnergyLossDBReader::compareFloatVector(const std::string& field,
                                                 const std::vector<float>& db,
                                                 const std::vector<float>& ref) const {
  if (db.size() != ref.size()) {
    edm::LogError("HGCalEnergyLossDBReader")
        << "Size mismatch field=" << field
        << " db=" << db.size()
        << " ref=" << ref.size();
    return false;
  }

  bool ok = true;
  for (std::size_t i = 0; i < ref.size(); ++i) {
    const double denom = std::abs(ref[i]) > 1e-10 ? std::abs(ref[i]) : 1.0;
    const double rel = std::abs(db[i] - ref[i]) / denom;

    if (rel > tolerance_) {
      ok = false;
      edm::LogWarning("HGCalEnergyLossDBReader")
          << "Mismatch field=" << field
          << " index=" << i
          << " db=" << db[i]
          << " ref=" << ref[i]
          << " rel_diff=" << rel;
    }
  }

  return ok;
}

bool HGCalEnergyLossDBReader::compareWithJson(const HGCalEnergyLossConditions& cond) const {
  json ref;

  if (refJsonFile_ == "-" || refJsonFile_ == "stdin") {
    ref = json::parse(std::cin, nullptr, true, /*ignore_comments*/ true);
  } else {
    std::ifstream infile(refJsonFile_);
    if (!infile.is_open()) {
      throw cms::Exception("HGCalEnergyLossDBReader")
          << "Cannot open reference EnergyLoss JSON file: " << refJsonFile_;
    }
    ref = json::parse(infile, nullptr, true, /*ignore_comments*/ true);
  }

  bool ok = true;
  ok &= compareFloatVector("dEdx", cond.dEdx, ref.at("dEdx").get<std::vector<float>>());
  ok &= compareFloatVector("SF_thickness_Si",
                           cond.SF_thickness_Si,
                           ref.at("SF_thickness_Si").get<std::vector<float>>());
  ok &= compareFloatVector("SF_thickness_SiPM",
                           cond.SF_thickness_SiPM,
                           ref.at("SF_thickness_SiPM").get<std::vector<float>>());

  return ok;
}

DEFINE_FWK_MODULE(HGCalEnergyLossDBReader);
