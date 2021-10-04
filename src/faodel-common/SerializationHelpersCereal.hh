// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef FAODEL_SERIALIZATIONHELPERSCEREAL_HH
#define FAODEL_SERIALIZATIONHELPERSCEREAL_HH


// Note: this is separated out from the normal SerializationHelpers.hh
//       because faodel does not include cereal when it does its
//       installation (to avoid conflict with existing libs).


//For CerealPack()/CerealUnpack()
#include <cereal/archives/binary.hpp>

namespace faodel {

template<class T>
std::string CerealPack(const T &t1) {
  std::stringstream ss;
  { //<--Important, put archive in {} to finalize last write at end
    cereal::BinaryOutputArchive oarchive(ss); //Binary archive
    oarchive(t1);
  }
  return ss.str();
}

template<class T>
T CerealUnpack(const std::string &packed) {
  T t1;
  std::istringstream astream(packed);
  {
    cereal::BinaryInputArchive iarchive(astream);
    iarchive(t1);
  }
  return t1;
}

} // namespace faodel

#endif //FAODEL_SERIALIZATIONHELPERSCEREAL_HH
