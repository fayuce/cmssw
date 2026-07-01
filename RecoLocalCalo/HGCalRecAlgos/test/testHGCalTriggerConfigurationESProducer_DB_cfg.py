import FWCore.ParameterSet.Config as cms

process = cms.Process("HGCalTriggerConfigurationESProducerDBTest")

process.source = cms.Source("EmptySource",
    firstRun = cms.untracked.uint32(1)
)

process.maxEvents = cms.untracked.PSet(
    input = cms.untracked.int32(1)
)

from CondCore.CondDB.CondDB_cfi import CondDB
CondDB.connect = "sqlite_file:hgcal_trigger_configuration_v2.db"

process.hgcalTriggerConfigurationDB = cms.ESSource(
    "PoolDBESSource",
    CondDB,
    toGet = cms.VPSet(
        cms.PSet(
            record = cms.string("HGCalTriggerConfigurationRcd"),
            tag = cms.string("HGCalTriggerConfiguration_v2")
        )
    )
)

process.hgCalTriggerConfigurationESProducer = cms.ESProducer(
    "HGCalTriggerConfigurationESProducer",
    useDB = cms.bool(True)
)

process.testHGCalTriggerConfiguration = cms.EDAnalyzer(
    "HGCalTriggerConfigurationESTest"
)

process.p = cms.Path(process.testHGCalTriggerConfiguration)
