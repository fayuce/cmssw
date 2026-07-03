import FWCore.ParameterSet.Config as cms
from FWCore.ParameterSet.VarParsing import VarParsing

options = VarParsing("analysis")

options.register("sqliteFile",
                 "hgcal_configuration_v1.db",
                 VarParsing.multiplicity.singleton,
                 VarParsing.varType.string,
                 "Output SQLite file")

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

options.register("sinceRun",
                 1,
                 VarParsing.multiplicity.singleton,
                 VarParsing.varType.int,
                 "Since run for the IOV")

options.register("fedconfig",
                 "Geometry/HGCalMapping/data/TestConfigs/config_feds_v1.json",
                 VarParsing.multiplicity.singleton,
                 VarParsing.varType.string,
                 "FED configuration JSON")

options.register("modconfig",
                 "Geometry/HGCalMapping/data/TestConfigs/config_econds_v1.json",
                 VarParsing.multiplicity.singleton,
                 VarParsing.varType.string,
                 "ECON-D configuration JSON")

options.register("modules",
                 "Geometry/HGCalMapping/data/ModuleMaps/modulelocator_P5v9.txt",
                 VarParsing.multiplicity.singleton,
                 VarParsing.varType.string,
                 "Module locator file")

options.register("sicells",
                 "",
                 VarParsing.multiplicity.singleton,
                 VarParsing.varType.string,
                 "Silicon cell mapping file")

options.register("sipmcells",
                 "",
                 VarParsing.multiplicity.singleton,
                 VarParsing.varType.string,
                 "SiPM cell mapping file")

options.register("geometry",
                 "ExtendedRun4D104",
                 VarParsing.multiplicity.singleton,
                 VarParsing.varType.string,
                 "Geometry version suffix")

options.parseArguments()

process = cms.Process("HGCalConfigurationCondDB")

process.source = cms.Source("EmptySource")
process.maxEvents = cms.untracked.PSet(input=cms.untracked.int32(1))

process.load("FWCore.MessageService.MessageLogger_cfi")
process.MessageLogger.cerr.threshold = "INFO"
process.MessageLogger.cerr.noTimeStamps = True
process.MessageLogger.search_modkey = {}
process.MessageLogger.search_fedkey = {}
process.MessageLogger.check_keys = {}
process.MessageLogger.cerr.FwkReport.reportEvery = 1

process.load(f"Configuration.Geometry.Geometry{options.geometry}Reco_cff")
process.load(f"Configuration.Geometry.Geometry{options.geometry}_cff")

from Geometry.HGCalMapping.hgcalmapping_cff import customise_hgcalmapper
kwargs = {k: getattr(options, k) for k in ["modules", "sicells", "sipmcells"] if getattr(options, k) != ""}
customise_hgcalmapper(process, **kwargs)

from CondCore.CondDB.CondDB_cfi import CondDB

process.PoolDBOutputService = cms.Service(
    "PoolDBOutputService",
    CondDB.clone(connect=cms.string(f"sqlite_file:{options.sqliteFile}")),
    timetype=cms.untracked.string("runnumber"),
    toPut=cms.VPSet(
        cms.PSet(
            record=cms.string(options.record),
            tag=cms.string(options.condTag)
        )
    )
)

process.hgcalConfigurationDBAnalyzer = cms.EDAnalyzer(
    "HGCalConfigurationDBAnalyzer",
    indexSource=cms.ESInputTag("hgCalMappingESProducer", ""),
    fedjson=cms.FileInPath(options.fedconfig),
    modjson=cms.FileInPath(options.modconfig),
    record=cms.string(options.record),
    tag=cms.string(options.condTag),
    sinceRun=cms.uint64(options.sinceRun),
    bePassthroughMode=cms.int32(-1),
    cbHeaderMarker=cms.int32(-1),
    slinkHeaderMarker=cms.int32(-1),
    econdHeaderMarker=cms.int32(-1),
    charMode=cms.int32(-1)
)

process.p = cms.Path(process.hgcalConfigurationDBAnalyzer)

print(f">>> Output SQLite: {options.sqliteFile!r}")
print(f">>> Record:        {options.record!r}")
print(f">>> Tag:           {options.condTag!r}")
print(f">>> FED config:    {options.fedconfig!r}")
print(f">>> ECON-D config: {options.modconfig!r}")
print(f">>> Module map:    {options.modules!r}")
