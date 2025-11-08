// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef FAODEL_WORKERSERDES_HH
#define FAODEL_WORKERSERDES_HH

#include <iostream>
#include <functional>
#include <vector>
#include <thread>
#include <random>

#include "faodel-common/SerializationHelpersBoost.hh"
#include "faodel-common/SerializationHelpersCereal.hh"
#include "lunasa/DataObject.hh"

#include <boost/serialization/vector.hpp>

#include <cereal/cereal.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

#include "../Worker.hh"



// Boilerplate code to pack/unpack a bunch of objects. The T class should
// be one of the Serdes objects, which should have (1) a simple ctor,
// (2) a templated serialize function for boost serialization, and
// (3) pup functions for manually packing/unpacking using lunasa helpers.


template<class T>
class WorkerSerdes
        : public Worker {

public:
  WorkerSerdes() = default;

  WorkerSerdes(int id, JobSerdes::params_t params)
          : Worker(id, params.num_iters, params.item_len_min, params.item_len_max),
            params(params),
            pack_objects(params.pack),
            unpack_objects(params.unpack) {

    if(params.method==1) { //Boost Serialization
      f_pack = [](const T &obj) {
        //Have boost make a string that we then copy into an LDO
        std::string s=faodel::BoostPack<T>(obj);
        lunasa::DataObject ldo(s.length());
        std::memcpy(ldo.GetDataPtr(), s.c_str(), s.length());
        return ldo;
      };
      f_unpack = [](const lunasa::DataObject &ldo) {
        //Pull the string out and let boost do the hard work
        std::string s(ldo.GetDataPtr<char *>(), ldo.GetDataSize());
        return faodel::BoostUnpack<T>(s);
      };
    } else if (params.method==2) { //Cereal Serialization
      f_pack = [](const T &obj) {
          //Have Cereal make a string that we then copy into an LDO
          std::string s=faodel::CerealPack<T>(obj);
          lunasa::DataObject ldo(s.length());
          std::memcpy(ldo.GetDataPtr(), s.c_str(), s.length());
          return ldo;
      };
      f_unpack = [](const lunasa::DataObject &ldo) {
          //Pull the string out and let boost do the hard work
          std::string s(ldo.GetDataPtr<char *>(), ldo.GetDataSize());
          return faodel::CerealUnpack<T>(s);
      };
    } else if (params.method==3) { //LDO Serialization
      //Use the pack/unpack function built into each class to serialize
      f_pack = [](const T &obj) { return obj.pup(); };
      f_unpack = [](const lunasa::DataObject &ldo) {
        T obj;
        obj.pup(ldo);
        return obj;
      };
    } else {
      F_ASSERT(0, "Unknown method passed to WorkerSerdes?");
    }

    //Create initial objects and their encoded values. Lets us do pack/unpack/pack+unpack
    objs.resize(batch_size);
    packed_objs.resize(batch_size);
    for(int i=0; i<batch_size; i++) {
      objs.at(i) =  T(params, [this]() { return prngGetRangedInteger(); });
      packed_objs.at(i) = f_pack(objs.at(i));
    }

  }
  ~WorkerSerdes() = default;

  void server() {

    do {
      if(pack_objects) {
        for(int i = 0; i<batch_size; i++) {
          packed_objs[i] = f_pack(objs[i]);
        }
      }
      if(unpack_objects) {
        for(int i=0; i<batch_size; i++) {
          objs[i] = f_unpack(packed_objs[i]);
        }
      }
      ops_completed += batch_size;
    } while(!kill_server);

  }
private:

  std::function<lunasa::DataObject (const T &)> f_pack;
  std::function<T (const lunasa::DataObject &)> f_unpack;

  bool pack_objects;
  bool unpack_objects;

  std::vector<T> objs;
  std::vector<lunasa::DataObject> packed_objs;
  JobSerdes::params_t params;
};


#endif //FAODEL_WORKERSERDES_HH
