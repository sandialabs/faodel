// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef FAODEL_SERDESSTRINGOBJECT_HH
#define FAODEL_SERDESSTRINGOBJECT_HH

#include <vector>
#include <string>
#include <random>
#include <functional>

#include "lunasa/common/DataObjectPacker.hh"

#include "../JobSerdes.hh"

class SerdesStringObject {
public:
  SerdesStringObject() = default;
  SerdesStringObject(JobSerdes::params_t params, std::function<int ()> f_prng);

  //Serialization hook
  template <class Archive>
  void serialize(Archive &ar, const unsigned int version){
    ar & strings;
  }

  //Lunasa's DataObject Packer hooks
  lunasa::DataObject pup() const;
  void pup(const lunasa::DataObject &ldo);


  //Actual data
  std::vector<std::string> strings;

};


#endif //FAODEL_SERDESSTRINGOBJECT_HH
