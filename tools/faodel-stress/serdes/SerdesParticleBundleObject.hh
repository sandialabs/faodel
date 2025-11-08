// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef FAODEL_SERDESPARTICLEBUNDLEOBJECT_HH
#define FAODEL_SERDESPARTICLEBUNDLEOBJECT_HH


#include <vector>
#include <string>
#include <random>
#include <functional>

#include "lunasa/common/DataObjectPacker.hh"

#include "../JobSerdes.hh"

// A mock up bundle of generic particles, organized as a struct of arrays.

class SerdesParticleBundleObject {
public:
  SerdesParticleBundleObject() = default;
  SerdesParticleBundleObject(JobSerdes::params_t params, std::function<int ()> f_prng);

  //Serialization hook
  template <typename Archive>
  void serialize(Archive &ar, const unsigned int version){
    ar & px; ar & py; ar & pz;
    ar & vx; ar & vy; ar & vz;
    ar & val1; ar & val2;
  }

  lunasa::DataObject pup() const;
  void pup(const lunasa::DataObject &ldo);


  //Actual data
  std::vector<double> px;
  std::vector<double> py;
  std::vector<double> pz;

  std::vector<double> vx;
  std::vector<double> vy;
  std::vector<double> vz;

  std::vector<uint32_t> val1;
  std::vector<uint32_t> val2;


};

#endif //FAODEL_SERDESPARTICLEBUNDLEOBJECT_HH
