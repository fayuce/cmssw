// HGCalPedestalDBReader.cc
//
// EDAnalyzer: closure test readback for HGCal pedestal CondDB migration.
//   Reads HGCalPedestalConditions directly from SQLite via the CondCore/CondDB
//   C++ API — bypassing EventSetup/PoolDBESSource entirely.  This avoids the
//   plugin loading-order issue where PoolDBESSource is constructed before
//   EVENTSETUP_RECORD_REG(HGCalPedestalRcd) is registered.
//
// Workflow:
//   # Read-only (print):
//   cmsRun HGCalPedestalDBReadback_cfg.py
//
//   # Closure test (compare with original JSON):
//   cmsRun HGCalPedestalDBReadback_cfg.py \
//     refJson=pedestal_MH_B1W_DNT0177_115457.json
//
// Author: Fatma Nur Yuce (EP-UCM, RDH), CERN 2026

#include "FWCore/Framework/interface/one/EDAnalyzer.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/EventSetup.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "FWCore/Utilities/interface/Exception.h"

#include "CondCore/CondDB/interface/ConnectionPool.h"
#include "CondFormats/HGCalObjects/interface/HGCalPedestalConditions.h"

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include <cmath>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

// =============================================================================
class HGCalPedestalDBReader : public edm::one::EDAnalyzer<> {
public:
  explicit HGCalPedestalDBReader(const edm::ParameterSet&);
  ~HGCalPedestalDBReader() override = default;
  static void fillDescriptions(edm::ConfigurationDescriptions&);

private:
  void analyze(const edm::Event&, const edm::EventSetup&) override;
  void endJob() override;

  void printConditions(const HGCalPedestalConditions&) const;
  void dumpToJson(const HGCalPedestalConditions&) const;
  bool compareWithJson(const HGCalPedestalConditions&) const;

  std::string m_sqliteFile;
  std::string m_tag;
  unsigned long long m_run;
  std::string m_refJsonFile;
  std::string m_outputJsonFile;
  float       m_tolerance;
  bool        m_verbose;
  bool        m_analyzed;
};

// =============================================================================
HGCalPedestalDBReader::HGCalPedestalDBReader(const edm::ParameterSet& ps)
    : m_sqliteFile   (ps.getParameter<std::string>          ("sqliteFile"))
    , m_tag          (ps.getParameter<std::string>          ("tag"))
    , m_run          (ps.getParameter<unsigned long long>   ("run"))
    , m_refJsonFile  (ps.getParameter<std::string>          ("refJsonFile"))
    , m_outputJsonFile(ps.getParameter<std::string>         ("outputJsonFile"))
    , m_tolerance    (ps.getParameter<double>               ("tolerance"))
    , m_verbose      (ps.getUntrackedParameter<bool>        ("verbose", false))
    , m_analyzed     (false)
{}

// =============================================================================
void HGCalPedestalDBReader::fillDescriptions(
    edm::ConfigurationDescriptions& descriptions)
{
  edm::ParameterSetDescription desc;
  desc.add<std::string>("sqliteFile", "hgcal_pedestals_test.db")
      ->setComment("SQLite CondDB file written by HGCalPedestalCondDB_cfg.py");
  desc.add<std::string>("tag", "HGCalPedestals_test_2026_06_22")
      ->setComment("CondDB tag to read back");
  desc.add<unsigned long long>("run", 1ULL)
      ->setComment("IOV since value (run number) to fetch");
  desc.add<std::string>("refJsonFile", "")
      ->setComment("Reference JSON from MySQL fetch (empty = print only)");
  desc.add<std::string>("outputJsonFile", "")
      ->setComment("Write payload back to JSON (empty = skip)");
  desc.add<double>("tolerance", 1e-5)
      ->setComment("Max allowed relative difference for float values");
  desc.addUntracked<bool>("verbose", false)
      ->setComment("Print per-channel values");
  descriptions.addWithDefaultLabel(desc);
}

// =============================================================================
// analyze — triggered once by EmptyIOVSource; reads CondDB directly
// =============================================================================
void HGCalPedestalDBReader::analyze(const edm::Event&, const edm::EventSetup&)
{
  if (m_analyzed) return;
  m_analyzed = true;

  namespace conddb = cond::persistency;

  conddb::ConnectionPool pool;
  auto session = pool.createSession("sqlite_file:" + m_sqliteFile);
  session.transaction().start(true);

  auto iovProxy = session.readIov(m_tag);
  auto iovs     = iovProxy.selectAll();
  auto it       = iovs.find(static_cast<cond::Time_t>(m_run));

  if (it == iovs.end()) {
    throw cms::Exception("HGCalPedestalDBReader")
        << "No IOV found in tag '" << m_tag
        << "' for run/since=" << m_run
        << ".  Check that the tag and run number are correct.";
  }

  cond::Hash payloadId = (*it).payloadId;

  auto conditions = session.fetchPayload<HGCalPedestalConditions>(payloadId);
  session.transaction().commit();

  if (!conditions) {
    throw cms::Exception("HGCalPedestalDBReader")
        << "fetchPayload returned null for hash " << payloadId;
  }

  printConditions(*conditions);
  dumpToJson(*conditions);

  if (!m_refJsonFile.empty()) {
    bool ok = compareWithJson(*conditions);
    edm::LogInfo("HGCalPedestalDBReader")
        << "\n=========================================="
        << "\n  Closure Test Result: " << (ok ? "*** PASSED ***" : "*** FAILED ***")
        << "\n==========================================";
    if (!ok)
      throw cms::Exception("ClosureTestFailed")
          << "HGCal pedestal closure test failed — see warnings above.";
  }
}

// =============================================================================
// dumpToJson
// =============================================================================
void HGCalPedestalDBReader::dumpToJson(const HGCalPedestalConditions& cond) const {
  if (m_outputJsonFile.empty())
    return;

  auto moduleToJson = [](const HGCalPedestalConditions::ModuleData& mod) {
    json out;
    out["typeCode"]  = mod.typeCode;
    out["reference"] = mod.reference;
    out["timestamp"] = mod.timestamp;
    out["payload"]   = {
        {"Channel",   mod.channel},
        {"Valid",     mod.valid},
        {"adc_ped",   mod.adc_ped},
        {"adc_rms",   mod.adc_rms},
        {"cm2_ped",   mod.cm2_ped},
        {"cm2_rms",   mod.cm2_rms},
        {"cm4_ped",   mod.cm4_ped},
        {"cm4_rms",   mod.cm4_rms},
        {"cm2_slope", mod.cm2_slope},
        {"cm4_slope", mod.cm4_slope}};
    return out;
  };

  json out;
  if (cond.modules.size() == 1) {
    out = moduleToJson(cond.modules.front());
  } else {
    out["modules"] = json::array();
    for (const auto& mod : cond.modules)
      out["modules"].push_back(moduleToJson(mod));
  }

  std::ofstream f(m_outputJsonFile);
  if (!f.is_open())
    throw cms::Exception("HGCalPedestalDBReader")
        << "Cannot open output JSON file: " << m_outputJsonFile;

  f << out.dump(2) << "\n";
  edm::LogInfo("HGCalPedestalDBReader")
      << "Wrote SQLite readback payload to " << m_outputJsonFile;
}

// =============================================================================
// printConditions
// =============================================================================
void HGCalPedestalDBReader::printConditions(
    const HGCalPedestalConditions& cond) const
{
  edm::LogInfo("HGCalPedestalDBReader")
      << "\n=========================================="
      << "\n  HGCal Pedestal Readback from CondDB"
      << "\n  nModules: " << cond.nModules()
      << "\n==========================================";

  for (const auto& mod : cond.modules) {
    const int N = mod.nChannels();
    if (N == 0) {
      edm::LogWarning("HGCalPedestalDBReader")
          << "Module " << mod.typeCode << " has no channels";
      continue;
    }

    float sum_ped = 0, min_ped = mod.adc_ped[0], max_ped = mod.adc_ped[0];
    float sum_rms = 0;
    int   nValid  = 0;
    for (int i = 0; i < N; ++i) {
      sum_ped += mod.adc_ped[i];
      sum_rms += mod.adc_rms[i];
      if (mod.adc_ped[i] < min_ped) min_ped = mod.adc_ped[i];
      if (mod.adc_ped[i] > max_ped) max_ped = mod.adc_ped[i];
      if (mod.valid[i]) ++nValid;
    }

    edm::LogInfo("HGCalPedestalDBReader")
        << "Module: "    << mod.typeCode
        << "  ref="      << mod.reference
        << "  ts="       << mod.timestamp
        << "  channels=" << N
        << "\n  valid="      << nValid << "/" << N
        << "  mean_adc_ped=" << (sum_ped / N)
        << "  min="          << min_ped
        << "  max="          << max_ped
        << "  mean_adc_rms=" << (sum_rms / N);

    if (m_verbose) {
      for (int i = 0; i < N; ++i) {
        edm::LogInfo("HGCalPedestalDBReader")
            << "  ch["   << mod.channel[i] << "]"
            << "  valid="     << mod.valid[i]
            << "  adc_ped="   << mod.adc_ped[i]
            << "  adc_rms="   << mod.adc_rms[i]
            << "  cm2_ped="   << mod.cm2_ped[i]
            << "  cm2_slope=" << mod.cm2_slope[i]
            << "  cm4_ped="   << mod.cm4_ped[i]
            << "  cm4_slope=" << mod.cm4_slope[i];
      }
    }
  }
}

// =============================================================================
// compareWithJson
// =============================================================================
bool HGCalPedestalDBReader::compareWithJson(
    const HGCalPedestalConditions& cond) const
{
  std::ifstream f(m_refJsonFile);
  if (!f.is_open()) {
    edm::LogError("HGCalPedestalDBReader")
        << "Cannot open reference JSON: " << m_refJsonFile;
    return false;
  }

  json j;
  try {
    j = json::parse(f);
  } catch (const json::exception& e) {
    edm::LogError("HGCalPedestalDBReader") << "JSON parse error: " << e.what();
    return false;
  }

  std::string typeCode = j.at("typeCode").get<std::string>();
  const HGCalPedestalConditions::ModuleData* mod = cond.getModule(typeCode);
  if (!mod) {
    edm::LogError("HGCalPedestalDBReader")
        << "Module '" << typeCode << "' not found in CondDB payload "
        << "(nModules=" << cond.nModules() << ")";
    return false;
  }

  const auto& p       = j.at("payload");
  auto ref_channel    = p.at("Channel")  .get<std::vector<int32_t>>();
  auto ref_valid      = p.at("Valid")    .get<std::vector<int32_t>>();
  auto ref_adc_ped    = p.at("adc_ped")  .get<std::vector<float>>();
  auto ref_adc_rms    = p.at("adc_rms")  .get<std::vector<float>>();
  auto ref_cm2_ped    = p.at("cm2_ped")  .get<std::vector<float>>();
  auto ref_cm2_rms    = p.at("cm2_rms")  .get<std::vector<float>>();
  auto ref_cm4_ped    = p.at("cm4_ped")  .get<std::vector<float>>();
  auto ref_cm4_rms    = p.at("cm4_rms")  .get<std::vector<float>>();
  auto ref_cm2_slope  = p.at("cm2_slope").get<std::vector<float>>();
  auto ref_cm4_slope  = p.at("cm4_slope").get<std::vector<float>>();

  const int N = (int)ref_adc_ped.size();
  if (mod->nChannels() != N) {
    edm::LogError("HGCalPedestalDBReader")
        << "Channel count mismatch: CondDB=" << mod->nChannels()
        << "  JSON=" << N;
    return false;
  }

  auto checkSize = [&](const char* field, size_t dbSz, size_t jsonSz) {
    if (dbSz != jsonSz || (int)jsonSz != N)
      throw cms::Exception("HGCalPedestalDBReader")
          << "Vector size mismatch for " << field
          << ": CondDB=" << dbSz << " JSON=" << jsonSz << " expected=" << N;
  };
  checkSize("channel",   mod->channel.size(),   ref_channel.size());
  checkSize("valid",     mod->valid.size(),     ref_valid.size());
  checkSize("adc_ped",   mod->adc_ped.size(),   ref_adc_ped.size());
  checkSize("adc_rms",   mod->adc_rms.size(),   ref_adc_rms.size());
  checkSize("cm2_ped",   mod->cm2_ped.size(),   ref_cm2_ped.size());
  checkSize("cm2_rms",   mod->cm2_rms.size(),   ref_cm2_rms.size());
  checkSize("cm4_ped",   mod->cm4_ped.size(),   ref_cm4_ped.size());
  checkSize("cm4_rms",   mod->cm4_rms.size(),   ref_cm4_rms.size());
  checkSize("cm2_slope", mod->cm2_slope.size(), ref_cm2_slope.size());
  checkSize("cm4_slope", mod->cm4_slope.size(), ref_cm4_slope.size());

  int nMismatches = 0;

  auto cmpFloat = [&](const char* field, float db, float ref, int ch) {
    float denom = std::abs(ref) > 1e-10f ? std::abs(ref) : 1.0f;
    float rel   = std::abs(db - ref) / denom;
    if (rel > m_tolerance) {
      ++nMismatches;
      edm::LogWarning("HGCalPedestalDBReader")
          << "MISMATCH ch=" << ch << "  " << field
          << "  conddb=" << db << "  json=" << ref
          << "  rel_diff=" << rel;
    }
  };

  for (int i = 0; i < N; ++i) {
    int ch = ref_channel[i];
    if (mod->channel[i] != ch) {
      ++nMismatches;
      edm::LogWarning("HGCalPedestalDBReader")
          << "MISMATCH index=" << i
          << "  channel: conddb=" << mod->channel[i] << "  json=" << ch;
    }
    if (mod->valid[i] != ref_valid[i]) {
      ++nMismatches;
      edm::LogWarning("HGCalPedestalDBReader")
          << "MISMATCH ch=" << ch
          << "  valid: conddb=" << mod->valid[i] << "  json=" << ref_valid[i];
    }
    cmpFloat("adc_ped",   mod->adc_ped[i],   ref_adc_ped[i],   ch);
    cmpFloat("adc_rms",   mod->adc_rms[i],   ref_adc_rms[i],   ch);
    cmpFloat("cm2_ped",   mod->cm2_ped[i],   ref_cm2_ped[i],   ch);
    cmpFloat("cm2_rms",   mod->cm2_rms[i],   ref_cm2_rms[i],   ch);
    cmpFloat("cm4_ped",   mod->cm4_ped[i],   ref_cm4_ped[i],   ch);
    cmpFloat("cm4_rms",   mod->cm4_rms[i],   ref_cm4_rms[i],   ch);
    cmpFloat("cm2_slope", mod->cm2_slope[i], ref_cm2_slope[i], ch);
    cmpFloat("cm4_slope", mod->cm4_slope[i], ref_cm4_slope[i], ch);
  }

  edm::LogInfo("HGCalPedestalDBReader")
      << "Closure comparison: " << N << " channels, "
      << nMismatches << " mismatches (tolerance=" << m_tolerance << ")";

  return (nMismatches == 0);
}

// =============================================================================
void HGCalPedestalDBReader::endJob() {
  if (!m_analyzed)
    edm::LogWarning("HGCalPedestalDBReader")
        << "No events processed — conditions were never read back. "
        << "Check maxEvents and source config.";
}

DEFINE_FWK_MODULE(HGCalPedestalDBReader);
