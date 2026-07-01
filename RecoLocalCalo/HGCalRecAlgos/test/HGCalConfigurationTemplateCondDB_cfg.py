import FWCore.ParameterSet.Config as cms
from FWCore.ParameterSet.VarParsing import VarParsing

options = VarParsing("analysis")

options.register("sqliteFile", "hgcal_config_template.db", VarParsing.multiplicity.singleton, VarParsing.varType.string, "Output SQLite CondDB file")
options.register("condTag", "HGCalConfigurationTemplate_v1", VarParsing.multiplicity.singleton, VarParsing.varType.string, "CondDB tag")
options.register("record", "HGCalConfigurationTemplateRcd", VarParsing.multiplicity.singleton, VarParsing.varType.string, "CondDB record")
options.register("sinceRun", 1, VarParsing.multiplicity.singleton, VarParsing.varType.int, "IOV since run")
options.register("fedconfig", "Geometry/HGCalMapping/data/TestConfigs/config_feds_v1.json", VarParsing.multiplicity.singleton, VarParsing.varType.string, "FED configuration JSON")
options.register("modconfig", "Geometry/HGCalMapping/data/TestConfigs/config_econds_v1.json", VarParsing.multiplicity.singleton, VarParsing.varType.string, "ECON-D configuration JSON")

options.parseArguments()

print(">>> Output SQLite:", options.sqliteFile)
print(">>> Record:       ", options.record)
print(">>> Tag:          ", options.condTag)
print(">>> FED config:   ", options.fedconfig)
print(">>> ECON-D config:", options.modconfig)

process = cms.Process("HGCalConfigurationTemplateCondDBWriter")

process.source = cms.Source("EmptySource", firstRun=cms.untracked.uint32(options.sinceRun))
process.maxEvents = cms.untracked.PSet(input=cms.untracked.int32(1))

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

process.writeHGCalConfigurationTemplate = cms.EDAnalyzer(
    "HGCalConfigurationTemplateDBAnalyzer",
    fedjson=cms.FileInPath(options.fedconfig),
    modjson=cms.FileInPath(options.modconfig),
    record=cms.string(options.record),
    tag=cms.string(options.condTag),
    sinceRun=cms.uint64(options.sinceRun)
)

process.p = cms.Path(process.writeHGCalConfigurationTemplate)
