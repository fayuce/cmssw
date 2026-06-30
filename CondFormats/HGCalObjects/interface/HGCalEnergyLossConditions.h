#ifndef CondFormats_HGCalObjects_HGCalEnergyLossConditions_h
#define CondFormats_HGCalObjects_HGCalEnergyLossConditions_h

#include <cstddef>
#include <vector>

#include <boost/serialization/access.hpp>
#include <boost/serialization/nvp.hpp>
#include <boost/serialization/vector.hpp>

class HGCalEnergyLossConditions {
public:
  HGCalEnergyLossConditions() = default;
  ~HGCalEnergyLossConditions() = default;

  std::vector<float> dEdx;
  std::vector<float> SF_thickness_Si;
  std::vector<float> SF_thickness_SiPM;

  int nLayers() const { return static_cast<int>(dEdx.size()); }

private:
  friend class boost::serialization::access;

  template <class Archive>
  void serialize(Archive& ar, const unsigned int) {
    ar & BOOST_SERIALIZATION_NVP(dEdx);
    ar & BOOST_SERIALIZATION_NVP(SF_thickness_Si);
    ar & BOOST_SERIALIZATION_NVP(SF_thickness_SiPM);
  }
};

#endif
