// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <cstring>
#include <iostream>

#include "faodel-common/Debug.hh"
#include "SerdesParticleBundleObject.hh"

using namespace std;

SerdesParticleBundleObject::SerdesParticleBundleObject(
        JobSerdes::params_t params,
        std::function<int ()> f_prng) {

  px.resize(params.num_items);
  py.resize(params.num_items);
  pz.resize(params.num_items);

  vx.resize(params.num_items);
  vy.resize(params.num_items);
  vz.resize(params.num_items);

  val1.resize(params.num_items);
  val2.resize(params.num_items);

  for(int i=0; i<params.num_items; i++) {
    px[i]=(double)(i+10000); py[i]=(double)(i+20000); pz[i]=(double)(i+30000);
    vx[i]=(double)(i+40000); vy[i]=(double)(i+50000); vz[i]=(double)(i+60000);
    val1[i]=70000+i;
    val2[i]=80000+1;
  }

}

lunasa::DataObject SerdesParticleBundleObject::pup() const {
  //Name for each component
  vector<string> names = {"px","py","pz", "vx","vy","vz", "val1", "val2"};

  //Pointers to each component
  vector<const void *> ptrs = {
          px.data(), py.data(), pz.data(),
          vx.data(), vy.data(), vz.data(),
          val1.data(), val2.data()
  };

  //Bytes for each component
  size_t dvec_bytes = px.size() * sizeof(double);
  vector<size_t> bytes = {
          dvec_bytes, dvec_bytes, dvec_bytes,
          dvec_bytes, dvec_bytes, dvec_bytes,
          val1.size() * sizeof(uint32_t), val2.size() * sizeof(uint32_t)
  };

  //We can use whatever label we want here.
  const uint8_t T_CHAR = 1;
  const uint8_t T_INT =2;
  const uint8_t T_FLOAT = 3;
  const uint8_t T_DOUBLE = 4;

  //Types for each component
  vector<uint8_t> types = {
    T_DOUBLE, T_DOUBLE, T_DOUBLE,
    T_DOUBLE, T_DOUBLE, T_DOUBLE,
    T_INT, T_INT
  };

  lunasa::DataObjectPacker packer(names, ptrs, bytes, types,
                                  faodel::const_hash32("ParticleBundle"));

  return packer.GetDataObject();
}


void SerdesParticleBundleObject::pup(const lunasa::DataObject &ldo) {

  lunasa::DataObjectPacker u(ldo);
  void *ptr; size_t bytes; uint8_t type;

  if(!u.VerifyDataType(faodel::const_hash32("ParticleBundle"))) {
    F_ASSERT(0, "Packed DataObject did not match the expected hash");
  }

  int rc=0;

  //Dangerous if anything goes wrong, but should be fairly fast
  rc|=u.GetVarPointer("px", &ptr, &bytes, &type); px.resize(bytes/sizeof(double)); std::memcpy(&px[0], ptr, bytes);
  rc|=u.GetVarPointer("py", &ptr, &bytes, &type); py.resize(bytes/sizeof(double)); std::memcpy(&py[0], ptr, bytes);
  rc|=u.GetVarPointer("pz", &ptr, &bytes, &type); pz.resize(bytes/sizeof(double)); std::memcpy(&pz[0], ptr, bytes);

  rc|=u.GetVarPointer("vx", &ptr, &bytes, &type); vx.resize(bytes/sizeof(double)); std::memcpy(&vx[0], ptr, bytes);
  rc|=u.GetVarPointer("vy", &ptr, &bytes, &type); vy.resize(bytes/sizeof(double)); std::memcpy(&vy[0], ptr, bytes);
  rc|=u.GetVarPointer("vz", &ptr, &bytes, &type); vz.resize(bytes/sizeof(double)); std::memcpy(&vz[0], ptr, bytes);

  rc|=u.GetVarPointer("val1", &ptr, &bytes, &type); val1.resize(bytes/sizeof(uint32_t)); std::memcpy(&val1[0], ptr, bytes);
  rc|=u.GetVarPointer("val2", &ptr, &bytes, &type); val2.resize(bytes/sizeof(uint32_t)); std::memcpy(&val2[0], ptr, bytes);

  if(rc!=0){ std::cerr<<"SerdesParticleBundleObject::pup detected an error\n"; }
}