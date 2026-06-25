import FWCore.ParameterSet.Config as cms
import FWCore.ParameterSet.VarParsing as VarParsing

options = VarParsing.VarParsing('standard')

options.register('jsonFile',
                 '/eos/cms/store/group/dpg_hgcal/tb_hgcal/DPG/calibrations/SepTB2024/level0_calib_hackathon.json',
                 VarParsing.VarParsing.multiplicity.singleton,
                 VarParsing.VarParsing.varType.string,
                 "Input level0 calibration JSON file")

options.register('sqliteFile',
                 'hgcal_rechit_calibration_test.db',
                 VarParsing.VarParsing.multiplicity.singleton,
                 VarParsing.VarParsing.varType.string,
                 "Output SQLite CondDB file")

options.register('record',
                 'HGCalRecHitCalibrationRcd',
                 VarParsing.VarParsing.multiplicity.singleton,
                 VarParsing.VarParsing.varType.string,
                 "CondDB record name")

options.register('tag',
                 'HGCalRecHitCalibration_test_2026_06_25',
                 VarParsing.VarParsing.multiplicity.singleton,
                 VarParsing.VarParsing.varType.string,
                 "CondDB tag name")

options.register('sinceRun',
                 1,
                 VarParsing.VarParsing.multiplicity.singleton,
                 VarParsing.VarParsing.varType.int,
                 "IOV start run number")

options.register('writeToCondDB',
                 False,
                 VarParsing.VarParsing.multiplicity.singleton,
                 VarParsing.VarParsing.varType.bool,
                 "If True, write to SQLite CondDB")

options.parseArguments()

process = cms.Process("HGCalRecHitCalibrationDB")

process.MessageLogger = cms.Service(
    "MessageLogger",
    destinations = cms.untracked.vstring('cout'),
    cout = cms.untracked.PSet(
        threshold = cms.untracked.string('INFO')
    )
)

process.source = cms.Source(
    "EmptyIOVSource",
    timetype   = cms.string('runnumber'),
    firstValue = cms.uint64(1),
    lastValue  = cms.uint64(1),
    interval   = cms.uint64(1)
)

process.load("CondCore.CondDB.CondDB_cfi")
process.CondDB.connect = cms.string('sqlite_file:' + str(options.sqliteFile))

process.PoolDBOutputService = cms.Service(
    "PoolDBOutputService",
    process.CondDB,
    timetype   = cms.untracked.string('runnumber'),
    toPut      = cms.VPSet(
        cms.PSet(
            record = cms.string(str(options.record)),
            tag    = cms.string(str(options.tag))
        )
    ),
    logconnect = cms.untracked.string('sqlite_file:hgcal_rechit_calibration_log.db')
)

process.hgcalRecHitCalibrationDBAnalyzer = cms.EDAnalyzer(
    "HGCalRecHitCalibrationDBAnalyzer",
    jsonFile      = cms.string(str(options.jsonFile)),
    record        = cms.string(str(options.record)),
    tag           = cms.string(str(options.tag)),
    sinceRun      = cms.uint64(options.sinceRun),
    writeToCondDB = cms.bool(bool(options.writeToCondDB)),
    verbose       = cms.untracked.bool(False)
)

process.p = cms.Path(process.hgcalRecHitCalibrationDBAnalyzer)
