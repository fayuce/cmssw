#ifndef CondFormats_HGCalObjects_HGCalConfigurationTemplateConditions_h
#define CondFormats_HGCalObjects_HGCalConfigurationTemplateConditions_h

#include "CondFormats/Serialization/interface/Serializable.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

struct HGCalROCConfigTemplate {
  int32_t charMode = 0;
  int32_t muxMode = -1;

  COND_SERIALIZABLE;
};

struct HGCalECONDConfigTemplate {
  std::string headerMarker;
  std::vector<int32_t> calibrationSC;
  std::vector<int32_t> multiPlex;
  bool hasMultiPlex = false;
  int32_t enabledErx = -1;
  bool hasEnabledErx = false;

  COND_SERIALIZABLE;
};

struct HGCalFedConfigTemplate {
  uint32_t fedId = 0;
  bool isWildcard = false;

  int32_t mismatchPassthroughMode = 0;
  std::string cbHeaderMarker;
  std::string slinkHeaderMarker;

  COND_SERIALIZABLE;
};

class HGCalConfigurationTemplateConditions {
public:
  HGCalConfigurationTemplateConditions() = default;

  std::vector<HGCalFedConfigTemplate> feds;
  std::map<std::string, HGCalECONDConfigTemplate> modules;

  COND_SERIALIZABLE;
};

#endif
