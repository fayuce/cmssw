import FWCore.ParameterSet.Config as cms
from FWCore.ParameterSet.VarParsing import VarParsing

options = VarParsing("analysis")

options.register(
    "sqliteFile",
    "hgcal_trigger_expanded.db",
    VarParsing.multiplicity.singleton,
    VarParsing.varType.string,
    "Output SQLite CondDB file"
)

options.register(
    "condTag",
    "HGCalTriggerConfiguration_expanded_v1",
    VarParsing.multiplicity.singleton,
    VarParsing.varType.string,
    "CondDB tag for HGCalTriggerConfiguration"
)

options.register(
    "record",
    "HGCalTriggerConfigurationRcd",
    VarParsing.multiplicity.singleton,
    VarParsing.varType.string,
    "CondDB record for HGCalTriggerConfiguration"
)

options.register(
    "sinceRun",
    1,
    VarParsing.multiplicity.singleton,
    VarParsing.varType.int,
    "IOV since run"
)

options.register(
    "fedconfig",
    "Geometry/HGCalMapping/data/TestConfigs/config_trigger_feds_v3.json",
    VarParsing.multiplicity.singleton,
    VarParsing.varType.string,
    "FED trigger configuration JSON"
)

options.register(
    "modconfig",
    "Geometry/HGCalMapping/data/TestConfigs/config_trigger_econts_v3.json",
    VarParsing.multiplicity.singleton,
    VarParsing.varType.string,
    "ECON-T trigger configuration JSON"
)

options.register(
    "moduleMap",
    "Geometry/HGCalMapping/data/ModuleMaps/modulelocator_P5v9.txt",
    VarParsing.multiplicity.singleton,
    VarParsing.varType.string,
    "HGCal module map"
)

options.parseArguments()

print(">>> Output SQLite:", options.sqliteFile)
print(">>> Record:       ", options.record)
print(">>> Tag:          ", options.condTag)
print(">>> FED config:   ", options.fedconfig)
print(">>> ECON-T config:", options.modconfig)
print(">>> Module map:   ", options.moduleMap)

process = cms.Process("HGCalTriggerConfigurationCondDBWriter")

process.source = cms.Source(
    "EmptySource",
    firstRun=cms.untracked.uint32(options.sinceRun)
)

process.maxEvents = cms.untracked.PSet(
    input=cms.untracked.int32(1)
)

process.load("Configuration.Geometry.GeometryExtendedRun4D104_cff")
process.load("Geometry.HGCalMapping.hgCalMappingESProducer_cfi")

from Geometry.HGCalMapping.hgcalmapping_cff import customise_hgcalmapper

customise_hgcalmapper(
    process,
    modules=options.moduleMap
)

from CondCore.CondDB.CondDB_cfi import CondDB

process.CondDB = CondDB.clone(
    connect=cms.string("sqlite_file:" + options.sqliteFile)
)

process.PoolDBOutputService = cms.Service(
    "PoolDBOutputService",
    process.CondDB,
    timetype=cms.untracked.string("runnumber"),
    toPut=cms.VPSet(
        cms.PSet(
            record=cms.string(options.record),
            tag=cms.string(options.condTag)
        )
    )
)

process.writeHGCalTriggerConfiguration = cms.EDAnalyzer(
    "HGCalTriggerConfigurationDBAnalyzer",
    fedjson=cms.FileInPath(options.fedconfig),
    modjson=cms.FileInPath(options.modconfig),
    record=cms.string(options.record),
    tag=cms.string(options.condTag),
    sinceRun=cms.uint64(options.sinceRun),
    indexSource=cms.ESInputTag("", "")
)

process.p = cms.Path(process.writeHGCalTriggerConfiguration)
