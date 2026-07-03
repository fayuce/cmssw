#ifndef CondFormats_HGCalObjects_HGCalTriggerConfigurationTemplateConditions_h
#define CondFormats_HGCalObjects_HGCalTriggerConfigurationTemplateConditions_h

#include "CondFormats/Serialization/interface/Serializable.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

struct HGCalTriggerECONTTemplate {
  uint8_t density = 0;
  uint8_t dropLSB = 0;
  uint8_t select = 0;
  uint8_t stcType = 0;
  uint8_t eportTxNumen = 0;
  uint8_t sumType = 0;

  std::vector<uint16_t> calv;
  std::vector<uint8_t> mux;

  COND_SERIALIZABLE;
};

struct HGCalTriggerModuleTemplate {
  std::vector<HGCalTriggerECONTTemplate> econts;

  COND_SERIALIZABLE;
};

struct HGCalTriggerFedTemplate {
  uint32_t fedId = 0;
  bool isWildcard = false;
  std::string tdaqHeaderMarker;
  std::vector<uint32_t> neconts;
  std::vector<int32_t> econtSwapOffset;
  std::map<uint8_t, std::vector<uint8_t>> elinksMap;

  COND_SERIALIZABLE;
};

class HGCalTriggerConfigurationTemplateConditions {
public:
  HGCalTriggerConfigurationTemplateConditions() = default;

  std::vector<HGCalTriggerFedTemplate> feds;
  std::map<std::string, HGCalTriggerModuleTemplate> modules;

  COND_SERIALIZABLE;
};

#endif
