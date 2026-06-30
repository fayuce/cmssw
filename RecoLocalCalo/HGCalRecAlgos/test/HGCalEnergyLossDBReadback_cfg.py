import FWCore.ParameterSet.Config as cms
from FWCore.ParameterSet.VarParsing import VarParsing

options = VarParsing('analysis')
options.register('sqliteFile',
                 'hgcal_energy_loss_v16.db',
                 VarParsing.multiplicity.singleton,
                 VarParsing.varType.string,
                 'Input SQLite file')
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
options.register('refJsonFile',
                 '/eos/cms/store/group/dpg_hgcal/tb_hgcal/DPG/calibrations/EnergyLoss/hgcal_energyloss_v16.json',
                 VarParsing.multiplicity.singleton,
                 VarParsing.varType.string,
                 'Reference EnergyLoss JSON file')
options.register('tolerance',
                 1e-6,
                 VarParsing.multiplicity.singleton,
                 VarParsing.varType.float,
                 'Closure tolerance')
options.parseArguments()

process = cms.Process("HGCalEnergyLossReadback")

process.source = cms.Source("EmptySource")
process.maxEvents = cms.untracked.PSet(input=cms.untracked.int32(1))

process.load("FWCore.MessageService.MessageLogger_cfi")
process.MessageLogger.cerr.threshold = "INFO"

from CondCore.CondDB.CondDB_cfi import CondDB
process.CondDBESSource = cms.ESSource(
    "PoolDBESSource",
    CondDB.clone(connect=cms.string(f"sqlite_file:{options.sqliteFile}")),
    toGet=cms.VPSet(
        cms.PSet(
            record=cms.string(options.record),
            tag=cms.string(options.energyLossTag)
        )
    )
)

process.energyLossReader = cms.EDAnalyzer(
    "HGCalEnergyLossDBReader",
    refJsonFile=cms.string(options.refJsonFile),
    tolerance=cms.double(options.tolerance),
    verbose=cms.untracked.bool(False)
)

print(f">>> SQLite file:       {options.sqliteFile!r}")
print(f">>> Record:            {options.record!r}")
print(f">>> Tag:               {options.energyLossTag!r}")
print(f">>> Reference JSON:    {options.refJsonFile!r}")

process.p = cms.Path(process.energyLossReader)
