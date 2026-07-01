#ifndef CondFormats_HGCalModuleConfigurationRcd_h
#define CondFormats_HGCalModuleConfigurationRcd_h
// -*- C++ -*-
//
// Package:     CondFormats/DataRecord
// Class  :     HGCalModuleConfigurationRcd
//
/**\class HGCalModuleConfigurationRcd HGCalModuleConfigurationRcd.h CondFormats/DataRecord/interface/HGCalModuleConfigurationRcd.h
 *
 * Description:
 *   This record is used for passing the configuration parameters to the calibration step in RAW -> RECO.
 *
 */
//
// Author:      Pedro Da Silva, Izaak Neutelings
// Created:     Mon, 29 May 2023 09:13:07 GMT
//

#include "FWCore/Framework/interface/DependentRecordImplementation.h"
#include "FWCore/Utilities/interface/mplVector.h"

#include "CondFormats/DataRecord/interface/HGCalElectronicsMappingRcd.h"
#include "CondFormats/DataRecord/interface/HGCalRecHitCalibrationRcd.h"
#include "CondFormats/DataRecord/interface/HGCalEnergyLossRcd.h"
#include "CondFormats/DataRecord/interface/HGCalConfigurationRcd.h"
#include "CondFormats/DataRecord/interface/HGCalTriggerConfigurationRcd.h"
#include "CondFormats/DataRecord/interface/HGCalConfigurationTemplateRcd.h"
#include "CondFormats/DataRecord/interface/HGCalTriggerConfigurationTemplateRcd.h"

class HGCalModuleConfigurationRcd
    : public edm::eventsetup::DependentRecordImplementation<
          HGCalModuleConfigurationRcd,
          edm::mpl::Vector<HGCalElectronicsMappingRcd,
                           HGCalRecHitCalibrationRcd,
                           HGCalEnergyLossRcd,
                           HGCalConfigurationRcd,
                           HGCalTriggerConfigurationRcd,
                           HGCalConfigurationTemplateRcd,
                           HGCalTriggerConfigurationTemplateRcd> > {};

#endif
