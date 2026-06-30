import FWCore.ParameterSet.Config as cms
from FWCore.ParameterSet.VarParsing import VarParsing

options = VarParsing('analysis')

options.register('jsonFile',
                 '/eos/cms/store/group/dpg_hgcal/tb_hgcal/DPG/calibrations/EnergyLoss/hgcal_energyloss_v16.json',
                 VarParsing.multiplicity.singleton,
                 VarParsing.varType.string,
                 'Input EnergyLoss JSON file')

options.register('sqliteFile',
                 'hgcal_energy_loss_v16.db',
                 VarParsing.multiplicity.singleton,
                 VarParsing.varType.string,
                 'Output SQLite file')

options.register('record',
                 'HGCalEnergyLossRcd',
                 VarParsing.multiplicity.singleton,
                 VarParsing.varType.string,
                 'CondDB record name')

options.register('energyLossTag',
                 'HGCalEnergyLoss_v16',
                 VarParsing.multiplicity.singleton,
                 VarParsing.varType.string,
                 'CondDB tag name')

options.register('sinceRun',
                 1,
                 VarParsing.multiplicity.singleton,
                 VarParsing.varType.int,
                 'IOV since run')

options.register('writeToCondDB',
                 True,
                 VarParsing.multiplicity.singleton,
                 VarParsing.varType.bool,
                 'Write payload to CondDB')

options.parseArguments()

process = cms.Process("HGCalEnergyLossCondDB")

process.source = cms.Source("EmptySource")
process.maxEvents = cms.untracked.PSet(input=cms.untracked.int32(1))

process.load("FWCore.MessageService.MessageLogger_cfi")
process.MessageLogger.cerr.threshold = "INFO"

from CondCore.CondDB.CondDB_cfi import CondDB

process.PoolDBOutputService = cms.Service(
    "PoolDBOutputService",
    CondDB.clone(connect=cms.string(f"sqlite_file:{options.sqliteFile}")),
    timetype=cms.untracked.string("runnumber"),
    toPut=cms.VPSet(
        cms.PSet(
            record=cms.string(options.record),
            tag=cms.string(options.energyLossTag)
        )
    )
)

process.energyLossWriter = cms.EDAnalyzer(
    "HGCalEnergyLossDBAnalyzer",
    jsonFile=cms.string(options.jsonFile),
    record=cms.string(options.record),
    tag=cms.string(options.energyLossTag),
    sinceRun=cms.uint64(options.sinceRun),
    writeToCondDB=cms.bool(options.writeToCondDB),
    verbose=cms.untracked.bool(False)
)

print(f">>> EnergyLoss JSON:   {options.jsonFile!r}")
print(f">>> SQLite file:       {options.sqliteFile!r}")
print(f">>> Record:            {options.record!r}")
print(f">>> Tag:               {options.energyLossTag!r}")
print(f">>> writeToCondDB:     {options.writeToCondDB!r}")

process.p = cms.Path(process.energyLossWriter)
