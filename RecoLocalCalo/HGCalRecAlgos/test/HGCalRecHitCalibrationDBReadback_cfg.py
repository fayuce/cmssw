import FWCore.ParameterSet.Config as cms
from FWCore.ParameterSet.VarParsing import VarParsing

process = cms.Process("HGCalRecHitCalibrationReadback")

options = VarParsing("analysis")

options.register(
    "sqliteFile",
    "hgcal_rechit_calibration_test.db",
    VarParsing.multiplicity.singleton,
    VarParsing.varType.string,
    "Input SQLite CondDB file",
)

options.register(
    "dbTag",
    "HGCalRecHitCalibration_test",
    VarParsing.multiplicity.singleton,
    VarParsing.varType.string,
    "CondDB tag for HGCal RecHit calibration payload",
)

options.register(
    "record",
    "HGCalRecHitCalibrationRcd",
    VarParsing.multiplicity.singleton,
    VarParsing.varType.string,
    "EventSetup record name for HGCal RecHit calibration payload",
)

options.register(
    "refJson",
    "",
    VarParsing.multiplicity.singleton,
    VarParsing.varType.string,
    "Reference level0/RecHitCalib JSON for closure comparison",
)

options.register(
    "tolerance",
    1e-5,
    VarParsing.multiplicity.singleton,
    VarParsing.varType.float,
    "Relative tolerance for closure comparison",
)

options.register(
    "verbose",
    False,
    VarParsing.multiplicity.singleton,
    VarParsing.varType.bool,
    "Print verbose readback information",
)

options.parseArguments()

process.source = cms.Source(
    "EmptySource",
    firstRun=cms.untracked.uint32(1),
)

process.maxEvents = cms.untracked.PSet(
    input=cms.untracked.int32(1)
)

process.load("FWCore.MessageService.MessageLogger_cfi")
process.MessageLogger.cerr.threshold = cms.untracked.string("INFO")
process.MessageLogger.cerr.HGCalRecHitCalibrationDBReader = cms.untracked.PSet(
    limit=cms.untracked.int32(-1)
)

process.load("CondCore.CondDB.CondDB_cfi")
process.CondDB.connect = cms.string("sqlite_file:" + options.sqliteFile)

process.hgcalRecHitCalibrationESSource = cms.ESSource(
    "PoolDBESSource",
    process.CondDB,
    toGet=cms.VPSet(
        cms.PSet(
            record=cms.string(options.record),
            tag=cms.string(options.dbTag),
        )
    ),
)

process.reader = cms.EDAnalyzer(
    "HGCalRecHitCalibrationDBReader",
    refJsonFile=cms.string(options.refJson),
    tolerance=cms.double(options.tolerance),
    verbose=cms.untracked.bool(options.verbose),
)

process.p = cms.Path(process.reader)
