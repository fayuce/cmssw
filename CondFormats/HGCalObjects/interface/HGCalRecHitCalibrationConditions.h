#ifndef CondFormats_HGCalObjects_HGCalRecHitCalibrationConditions_h
#define CondFormats_HGCalObjects_HGCalRecHitCalibrationConditions_h

#include <cstdint>
#include <string>
#include <vector>

#include <boost/serialization/access.hpp>
#include <boost/serialization/nvp.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/vector.hpp>

class HGCalRecHitCalibrationConditions {
public:
  HGCalRecHitCalibrationConditions() = default;
  ~HGCalRecHitCalibrationConditions() = default;

  struct ModuleData {
    std::string typeCode;

    std::vector<int32_t> channel;
    std::vector<int32_t> valid;

    std::vector<float> ADC_ped;
    std::vector<float> Noise;
    std::vector<float> CM_slope;
    std::vector<float> CM_ped;
    std::vector<float> BXm1_slope;
    std::vector<float> BXm1_ped;

    std::vector<float> TOTtoADC;
    std::vector<float> TOT_ped;
    std::vector<float> TOT_lin;
    std::vector<float> TOT_P0;
    std::vector<float> TOT_P1;
    std::vector<float> TOT_P2;

    std::vector<std::vector<float>> TOA_CTDC;
    std::vector<std::vector<float>> TOA_FTDC;
    std::vector<std::vector<float>> TOA_TW;

    std::vector<float> TOAtops;

    std::vector<float> MIPS_scale;

    int nChannels() const { return static_cast<int>(ADC_ped.size()); }

  private:
    friend class boost::serialization::access;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
      ar & BOOST_SERIALIZATION_NVP(typeCode);
      ar & BOOST_SERIALIZATION_NVP(channel);
      ar & BOOST_SERIALIZATION_NVP(valid);

      ar & BOOST_SERIALIZATION_NVP(ADC_ped);
      ar & BOOST_SERIALIZATION_NVP(Noise);
      ar & BOOST_SERIALIZATION_NVP(CM_slope);
      ar & BOOST_SERIALIZATION_NVP(CM_ped);
      ar & BOOST_SERIALIZATION_NVP(BXm1_slope);
      ar & BOOST_SERIALIZATION_NVP(BXm1_ped);

      ar & BOOST_SERIALIZATION_NVP(TOTtoADC);
      ar & BOOST_SERIALIZATION_NVP(TOT_ped);
      ar & BOOST_SERIALIZATION_NVP(TOT_lin);
      ar & BOOST_SERIALIZATION_NVP(TOT_P0);
      ar & BOOST_SERIALIZATION_NVP(TOT_P1);
      ar & BOOST_SERIALIZATION_NVP(TOT_P2);

      ar & BOOST_SERIALIZATION_NVP(TOA_CTDC);
      ar & BOOST_SERIALIZATION_NVP(TOA_FTDC);
      ar & BOOST_SERIALIZATION_NVP(TOA_TW);
      ar & BOOST_SERIALIZATION_NVP(TOAtops);

      ar & BOOST_SERIALIZATION_NVP(MIPS_scale);
    }
  };

  std::vector<ModuleData> modules;

  int nModules() const { return static_cast<int>(modules.size()); }

  const ModuleData* getModule(const std::string& typeCode) const {
    for (const auto& module : modules) {
      if (module.typeCode == typeCode)
        return &module;
    }
    return nullptr;
  }

private:
  friend class boost::serialization::access;

  template <class Archive>
  void serialize(Archive& ar, const unsigned int) {
    ar & BOOST_SERIALIZATION_NVP(modules);
  }
};

#endif
