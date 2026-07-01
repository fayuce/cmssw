import FWCore.ParameterSet.Config as cms
from FWCore.ParameterSet.VarParsing import VarParsing

options = VarParsing("analysis")

options.register("sqliteFile",
                 "hgcal_trigger_configuration_v2.db",
                 VarParsing.multiplicity.singleton,
                 VarParsing.varType.string,
                 "Input SQLite file")

options.register("condTag",
                 "HGCalTriggerConfiguration_v2",
                 VarParsing.multiplicity.singleton,
                 VarParsing.varType.string,
                 "CondDB tag for HGCalTriggerConfiguration")

options.register("record",
                 "HGCalTriggerConfigurationRcd",
                 VarParsing.multiplicity.singleton,
                 VarParsing.varType.string,
                 "CondDB record for HGCalTriggerConfiguration")

options.parseArguments()

process = cms.Process("HGCalTriggerConfigurationDBReadback")

process.source = cms.Source("EmptySource",
    firstRun = cms.untracked.uint32(1)
)

process.maxEvents = cms.untracked.PSet(
    input = cms.untracked.int32(1)
)

process.load("FWCore.MessageService.MessageLogger_cfi")
process.MessageLogger.cerr.threshold = "INFO"
process.MessageLogger.cerr.noTimeStamps = True
process.MessageLogger.cerr.FwkReport.reportEvery = 1

from CondCore.CondDB.CondDB_cfi import CondDB

process.hgcalTriggerConfiguration = cms.ESSource(
    "PoolDBESSource",
    CondDB.clone(connect=cms.string(f"sqlite_file:{options.sqliteFile}")),
    toGet = cms.VPSet(
        cms.PSet(
            record = cms.string(options.record),
            tag = cms.string(options.condTag)
        )
    )
)

process.readHGCalTriggerConfiguration = cms.EDAnalyzer(
    "HGCalTriggerConfigurationDBReader"
)

process.p = cms.Path(process.readHGCalTriggerConfiguration)

print(f">>> Input SQLite: {options.sqliteFile!r}")
print(f">>> Record:       {options.record!r}")
print(f">>> Tag:          {options.condTag!r}")
