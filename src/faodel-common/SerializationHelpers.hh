// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#ifndef FAODEL_COMMON_SERIALIZATION_HELPERS_HH
#define FAODEL_COMMON_SERIALIZATION_HELPERS_HH

#include <sstream>

//For boostPack()/boostUnpack()
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>


namespace faodel {

template<typename T>
std::string BoostPack(const T &t1) {
  std::ostringstream astream;
  { //<--Important, put archive in {} to finalize last write at end
    boost::archive::binary_oarchive archive(astream); //Binary archive
    archive & t1;
  }
  return astream.str();
}

template<typename T>
T BoostUnpack(const std::string &packed) {
  T t1;
  std::istringstream astream(packed);
  {
    boost::archive::binary_iarchive archive(astream);
    archive & t1;
  }
  return t1;
}

} // namespace faodel

#endif // FAODEL_COMMON_SERIALIZATION_HELPERS_HH
