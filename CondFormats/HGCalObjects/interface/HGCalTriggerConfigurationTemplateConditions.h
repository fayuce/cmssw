#ifndef CondFormats_HGCalObjects_HGCalTriggerConfigurationTemplateConditions_h
#define CondFormats_HGCalObjects_HGCalTriggerConfigurationTemplateConditions_h

#include <string>
#include "CondFormats/Serialization/interface/Serializable.h"

class HGCalTriggerConfigurationTemplateConditions {
public:
  HGCalTriggerConfigurationTemplateConditions() = default;

  std::string fedJson;
  std::string modJson;

  COND_SERIALIZABLE;
};

#endif
