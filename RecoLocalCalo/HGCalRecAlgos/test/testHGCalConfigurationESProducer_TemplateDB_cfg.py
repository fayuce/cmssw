import FWCore.ParameterSet.Config as cms
from FWCore.ParameterSet.VarParsing import VarParsing

options = VarParsing("analysis")

options.register("sqliteFile",
                 "hgcal_config_template.db",
                 VarParsing.multiplicity.singleton,
                 VarParsing.varType.string,
                 "Input SQLite file containing HGCalConfigurationTemplateConditions")

options.register("condTag",
                 "HGCalConfigurationTemplate_v1",
                 VarParsing.multiplicity.singleton,
                 VarParsing.varType.string,
                 "CondDB tag for HGCalConfigurationTemplateConditions")

options.register("record",
                 "HGCalConfigurationTemplateRcd",
                 VarParsing.multiplicity.singleton,
                 VarParsing.varType.string,
                 "CondDB record for HGCalConfigurationTemplateConditions")

options.register("modules",
                 "Geometry/HGCalMapping/data/ModuleMaps/modulelocator_Sep2024TBv2.txt",
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

process = cms.Process("HGCalConfigurationTemplateDBRuntimeTest")

process.source = cms.Source("EmptySource",
    firstRun = cms.untracked.uint32(1)
)

process.maxEvents = cms.untracked.PSet(
    input = cms.untracked.int32(1)
)

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

process.hgcalConfigurationTemplateDB = cms.ESSource(
    "PoolDBESSource",
    CondDB.clone(connect=cms.string(f"sqlite_file:{options.sqliteFile}")),
    toGet=cms.VPSet(
        cms.PSet(
            record=cms.string(options.record),
            tag=cms.string(options.condTag)
        )
    )
)

process.hgCalConfigurationESProducer = cms.ESSource(
    "HGCalConfigurationESProducer",
    useDB=cms.bool(False),
    configurationMode=cms.string("templatedDB"),
    templateSource=cms.ESInputTag(""),
    indexSource=cms.ESInputTag("hgCalMappingESProducer", ""),
    bePassthroughMode=cms.int32(-1),
    cbHeaderMarker=cms.int32(-1),
    slinkHeaderMarker=cms.int32(-1),
    econdHeaderMarker=cms.int32(-1),
    charMode=cms.int32(-1)
)

process.testHGCalConfiguration = cms.EDAnalyzer(
    "HGCalConfigurationESTest"
)

process.p = cms.Path(process.testHGCalConfiguration)

print(f">>> Template SQLite: {options.sqliteFile!r}")
print(f">>> Template Record: {options.record!r}")
print(f">>> Template Tag:    {options.condTag!r}")
print(f">>> Module map:      {options.modules!r}")
