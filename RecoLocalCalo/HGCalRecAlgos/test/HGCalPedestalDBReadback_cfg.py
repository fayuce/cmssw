# HGCalPedestalDBReadback_cfg.py
#
# Closure test: reads HGCalPedestalConditions back from a SQLite CondDB file
# (written by HGCalPedestalCondDB_cfg.py) and optionally compares every
# channel value against the original MySQL JSON.
#
# The analyzer reads SQLite directly via CondCore/CondDB C++ API — no
# EventSetup or PoolDBESSource is used.  This bypasses the plugin loading-order
# problem where PoolDBESSource was constructed before EVENTSETUP_RECORD_REG
# could register HGCalPedestalRcd.
#
# Usage:
#   # Print only (no comparison):
#   cmsRun HGCalPedestalDBReadback_cfg.py \
#     sqliteFile=/eos/.../hgcal_pedestals_test.db
#
#   # Full closure test (compare with original JSON):
#   cmsRun HGCalPedestalDBReadback_cfg.py \
#     sqliteFile=/eos/.../hgcal_pedestals_test.db \
#     refJson=pedestal_MH_B1W_DNT0177_115457.json \
#     outputJson=sqlite_readback.json
#
# Author: Fatma Nur Yuce (EP-UCM, RDH), CERN 2026

import FWCore.ParameterSet.Config as cms
import FWCore.ParameterSet.VarParsing as VarParsing

# ---------------------------------------------------------------------------
# Command-line options
# ---------------------------------------------------------------------------
options = VarParsing.VarParsing('standard')

options.register(
    'sqliteFile',
    'hgcal_pedestals_test.db',
    VarParsing.VarParsing.multiplicity.singleton,
    VarParsing.VarParsing.varType.string,
    "SQLite file written by HGCalPedestalCondDB_cfg.py"
)

options.register(
    'tag',
    'HGCalPedestals_test_2026_06_22',
    VarParsing.VarParsing.multiplicity.singleton,
    VarParsing.VarParsing.varType.string,
    "CondDB tag to read back"
)

options.register(
    'runNumber',
    1,
    VarParsing.VarParsing.multiplicity.singleton,
    VarParsing.VarParsing.varType.int,
    "Run/IOV since value (must match the sinceRun used when writing)"
)

options.register(
    'refJson',
    '',
    VarParsing.VarParsing.multiplicity.singleton,
    VarParsing.VarParsing.varType.string,
    "Original MySQL JSON (empty = print only, no closure comparison)"
)

options.register(
    'outputJson',
    '',
    VarParsing.VarParsing.multiplicity.singleton,
    VarParsing.VarParsing.varType.string,
    "Write payload read back from SQLite to this JSON file (empty = skip)"
)

options.register(
    'verbose',
    False,
    VarParsing.VarParsing.multiplicity.singleton,
    VarParsing.VarParsing.varType.bool,
    "Print per-channel values from CondDB"
)

options.parseArguments()

# ---------------------------------------------------------------------------
# Process
# ---------------------------------------------------------------------------
process = cms.Process("HGCalPedestalDBReadback")

process.MessageLogger = cms.Service(
    "MessageLogger",
    destinations = cms.untracked.vstring('cout'),
    cout = cms.untracked.PSet(
        threshold = cms.untracked.string('INFO')
    )
)

# One empty event triggers analyze(); run number is explicit so the analyzer
# knows which IOV since value to look up.
process.source = cms.Source(
    "EmptySource",
    firstRun = cms.untracked.uint32(options.runNumber)
)
process.maxEvents = cms.untracked.PSet(
    input = cms.untracked.int32(1)
)

# ---------------------------------------------------------------------------
# Analyzer — reads SQLite directly via CondCore/CondDB C++ API
# No PoolDBESSource or EventSetup record registration needed.
# ---------------------------------------------------------------------------
process.hgcalPedestalReader = cms.EDAnalyzer(
    "HGCalPedestalDBReader",
    sqliteFile     = cms.string(str(options.sqliteFile)),
    tag            = cms.string(str(options.tag)),
    run            = cms.uint64(options.runNumber),
    refJsonFile    = cms.string(str(options.refJson)),
    outputJsonFile = cms.string(str(options.outputJson)),
    tolerance      = cms.double(1e-5),
    verbose        = cms.untracked.bool(bool(options.verbose))
)

process.p = cms.Path(process.hgcalPedestalReader)
