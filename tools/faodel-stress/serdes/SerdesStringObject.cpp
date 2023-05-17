// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <iostream>
#include "lunasa/common/GenericSequentialDataBundle.hh"

#include "SerdesStringObject.hh"


using namespace std;

SerdesStringObject::SerdesStringObject(JobSerdes::params_t params,
                                       std::function<int ()> f_prng) {

  //Populate our string list
  for(int i=0; i<params.num_items; i++) {
    strings.emplace_back(faodel::RandomString( f_prng() ));
  }

}

typedef lunasa::GenericSequentialBundle<uint64_t> bundle_t;

lunasa::DataObject SerdesStringObject::pup() const {

  // Since this is a series of random length strings, the easiest thing to
  // do is just pack them into an LDO using the GenericSequentialBundler
  // class. You allocate space for all the strings, and then use a
  // bundle_offsets_t to keep track of where you are in the LDO.


  //Figure out how much space our strings need. Note: each item as a 32b length
  uint32_t payload_size=0;
  for(auto &s: strings) {
    payload_size += s.size() + sizeof(uint32_t);
  }

  //Allocate an LDO, overlay our bundle structure on it, and wipe the header
  lunasa::DataObject ldo(sizeof(bundle_t), payload_size,lunasa::DataObject::AllocatorType::eager);
  auto *msg = ldo.GetMetaPtr<bundle_t *>();
  msg->Init();

  //Use the offsets to track where we are so we don't overflow
  lunasa::bundle_offsets_t counters(&ldo);
  for(auto &s : strings) {
    bool ok=msg->AppendBack(counters, s);
    if(!ok) std::cerr<<"Serialization problems in SerdesStringObject\n";
  }

  return ldo;
}
void SerdesStringObject::pup(const lunasa::DataObject &ldo) {

  auto *msg = ldo.GetMetaPtr<bundle_t *>();
  lunasa::bundle_offsets_t counters(&ldo);

  strings.resize(0);
  string s;
  while(msg->GetNext(counters, &s)) {
    strings.emplace_back(s);
  }
}