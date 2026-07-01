#ifndef CondFormats_HGCalObjects_HGCalConfigurationTemplateConditions_h
#define CondFormats_HGCalObjects_HGCalConfigurationTemplateConditions_h

#include <string>
#include "CondFormats/Serialization/interface/Serializable.h"

class HGCalConfigurationTemplateConditions {
public:
  HGCalConfigurationTemplateConditions() = default;

  std::string fedJson;
  std::string modJson;

  COND_SERIALIZABLE;
};

#endif
