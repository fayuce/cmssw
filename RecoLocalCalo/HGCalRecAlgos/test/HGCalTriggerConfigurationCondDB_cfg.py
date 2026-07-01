import FWCore.ParameterSet.Config as cms

process = cms.Process("HGCalTriggerConfigurationCondDBWriter")

process.source = cms.Source("EmptySource",
    firstRun = cms.untracked.uint32(1)
)

process.maxEvents = cms.untracked.PSet(
    input = cms.untracked.int32(1)
)

process.load("Configuration.Geometry.GeometryExtendedRun4D104_cff")
process.load("Geometry.HGCalMapping.hgCalMappingESProducer_cfi")

from Geometry.HGCalMapping.hgcalmapping_cff import customise_hgcalmapper

customise_hgcalmapper(
    process,
    modules = "Geometry/HGCalMapping/data/ModuleMaps/modulelocator_Sep2024TBv2.txt"
)

from CondCore.CondDB.CondDB_cfi import CondDB

CondDB.connect = "sqlite_file:hgcal_trigger_configuration_v2.db"

process.PoolDBOutputService = cms.Service(
    "PoolDBOutputService",
    CondDB,
    timetype = cms.untracked.string("runnumber"),
    toPut = cms.VPSet(
        cms.PSet(
            record = cms.string("HGCalTriggerConfigurationRcd"),
            tag = cms.string("HGCalTriggerConfiguration_v2")
        )
    )
)

process.writeHGCalTriggerConfiguration = cms.EDAnalyzer(
    "HGCalTriggerConfigurationDBAnalyzer",
    fedjson = cms.FileInPath("Geometry/HGCalMapping/data/TestConfigs/config_trigger_feds_v2.json"),
    modjson = cms.FileInPath("Geometry/HGCalMapping/data/TestConfigs/config_trigger_econts_v2.json"),
    record = cms.string("HGCalTriggerConfigurationRcd"),
    tag = cms.string("HGCalTriggerConfiguration_v2"),
    sinceRun = cms.uint64(1),
    indexSource = cms.ESInputTag("", "")
)

process.p = cms.Path(process.writeHGCalTriggerConfiguration)
