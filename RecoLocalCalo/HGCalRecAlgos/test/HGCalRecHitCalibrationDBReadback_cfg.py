import FWCore.ParameterSet.Config as cms
import FWCore.ParameterSet.VarParsing as VarParsing

options = VarParsing.VarParsing('standard')

options.register('sqliteFile',
                 'hgcal_rechit_calibration_test.db',
                 VarParsing.VarParsing.multiplicity.singleton,
                 VarParsing.VarParsing.varType.string,
                 "Input SQLite CondDB file")

options.register('tag',
                 'HGCalRecHitCalibration_test_2026_06_25',
                 VarParsing.VarParsing.multiplicity.singleton,
                 VarParsing.VarParsing.varType.string,
                 "CondDB tag name")

options.register('run',
                 1,
                 VarParsing.VarParsing.multiplicity.singleton,
                 VarParsing.VarParsing.varType.int,
                 "IOV run value")

options.register('refJson',
                 '/eos/cms/store/group/dpg_hgcal/tb_hgcal/DPG/calibrations/SepTB2024/level0_calib_hackathon.json',
                 VarParsing.VarParsing.multiplicity.singleton,
                 VarParsing.VarParsing.varType.string,
                 "Reference level0 calibration JSON")

options.register('tolerance',
                 1e-5,
                 VarParsing.VarParsing.multiplicity.singleton,
                 VarParsing.VarParsing.varType.float,
                 "Relative tolerance")

options.parseArguments()

process = cms.Process("HGCalRecHitCalibrationReadback")

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

process.hgcalRecHitCalibrationDBReader = cms.EDAnalyzer(
    "HGCalRecHitCalibrationDBReader",
    sqliteFile  = cms.string(str(options.sqliteFile)),
    tag         = cms.string(str(options.tag)),
    run         = cms.uint64(options.run),
    refJsonFile = cms.string(str(options.refJson)),
    tolerance   = cms.double(options.tolerance),
    verbose     = cms.untracked.bool(False)
)

process.p = cms.Path(process.hgcalRecHitCalibrationDBReader)
