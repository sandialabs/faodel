// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef FAODEL_COMMON_SERIALIZATION_HELPERS_HH
#define FAODEL_COMMON_SERIALIZATION_HELPERS_HH

#include <sstream>

// These are helper functions to make it easier to use Boost or Cereal to
// serialize a class to string that can be shipped around. Boost requires
// and external library. Cereal is a header-only library included in
// faodel's tpls. Of the two, Cereal has been faster in nearly all of our
// tests. Both require the user to provide a serialize template in their
// class definition that defines the order in which items are serialized.
//
//  An example class might look like this:
//
// class foo {
// public:
//   vector<string> stuff1;
//   int x2;
//
//   template <class Archive>
//   void serialize(Archive &ar, const unsigned int version) {
//     ar & stuff1;
//     ar & x2;
//   }
// };
//
//
// Note: Make sure to include the proper boost/cereal data type header
//       files for your data structure. If you don't, you'll get a lot of
//       hard to figure out template errors.
//
//   eg:
//       boost:  #include <boost/serialization/vector.hpp>
//       cereal: #include <cereal/types/string.hpp>
//       cereal: #include <cereal/types/vector.hpp>
//


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
