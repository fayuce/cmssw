import FWCore.ParameterSet.Config as cms
from FWCore.ParameterSet.VarParsing import VarParsing

options = VarParsing("analysis")

options.register("sqliteFile",
                 "hgcal_configuration_v1.db",
                 VarParsing.multiplicity.singleton,
                 VarParsing.varType.string,
                 "Input SQLite file")

options.register("condTag",
                 "HGCalConfiguration_v1",
                 VarParsing.multiplicity.singleton,
                 VarParsing.varType.string,
                 "CondDB tag for HGCalConfiguration")

options.register("record",
                 "HGCalConfigurationRcd",
                 VarParsing.multiplicity.singleton,
                 VarParsing.varType.string,
                 "CondDB record for HGCalConfiguration")

options.parseArguments()

process = cms.Process("HGCalConfigurationReadback")

process.source = cms.Source("EmptySource")
process.maxEvents = cms.untracked.PSet(input=cms.untracked.int32(1))

process.load("FWCore.MessageService.MessageLogger_cfi")
process.MessageLogger.cerr.threshold = "INFO"
process.MessageLogger.cerr.noTimeStamps = True
process.MessageLogger.cerr.FwkReport.reportEvery = 1

from CondCore.CondDB.CondDB_cfi import CondDB

process.HGCalConfigurationDBESSource = cms.ESSource(
    "PoolDBESSource",
    CondDB.clone(connect=cms.string(f"sqlite_file:{options.sqliteFile}")),
    toGet=cms.VPSet(
        cms.PSet(
            record=cms.string(options.record),
            tag=cms.string(options.condTag)
        )
    )
)

process.reader = cms.EDAnalyzer(
    "HGCalConfigurationDBReader",
    configSource=cms.ESInputTag("")
)

process.p = cms.Path(process.reader)

print(f">>> Input SQLite: {options.sqliteFile!r}")
print(f">>> Record:       {options.record!r}")
print(f">>> Tag:          {options.condTag!r}")
