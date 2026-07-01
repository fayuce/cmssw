import FWCore.ParameterSet.Config as cms

process = cms.Process("HGCalTriggerConfigurationDBReadback")

process.source = cms.Source("EmptySource",
    firstRun = cms.untracked.uint32(1)
)

process.maxEvents = cms.untracked.PSet(
    input = cms.untracked.int32(1)
)

from CondCore.CondDB.CondDB_cfi import CondDB

CondDB.connect = "sqlite_file:hgcal_trigger_configuration_v2.db"

process.hgcalTriggerConfiguration = cms.ESSource(
    "PoolDBESSource",
    CondDB,
    toGet = cms.VPSet(
        cms.PSet(
            record = cms.string("HGCalTriggerConfigurationRcd"),
            tag = cms.string("HGCalTriggerConfiguration_v2")
        )
    )
)

process.readHGCalTriggerConfiguration = cms.EDAnalyzer(
    "HGCalTriggerConfigurationDBReader"
)

process.p = cms.Path(process.readHGCalTriggerConfiguration)
