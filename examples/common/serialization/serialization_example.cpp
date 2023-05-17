// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <iostream>

#include "faodel-common/SerializationHelpersBoost.hh"
#include <boost/serialization/vector.hpp>

#include "MyContainer.hh"

// Boost and Cereal make it easy to serialize/deserialize complex classes,
// you just add a serialize template to each class and let the library
// pack it into an archive. There are four things to remember:
//
//  1. Add a serialize() template to each class being packed
//  2. Add boost includes for STL's (eg boost/serialization/vector.hpp)
//  3. Use something like faodel's BoostPack to pack the class to bytes
//  4. Add Boost::serialization to the target_link_libraries
//
// In addition to Boost, Faodel also has support for Cereal. Cereal is
// faster and is a header-only library. Unfortunately, Faodel does not
// export cereal at installation (to avoid conflicts with other libs),
// so it is only available inside of the faodel libs.


using namespace std;

int main(int argc, char **argv) {

  //Create an object and add some data to it
  MyContainer bag1("big bag of stuff");
  bag1.append("thing1",101.0);
  bag1.append("thing2", 102.0);
  bag1.append("thing3", 103.0);

  //Use boost to pack the object into a string of bytes
  string s_bytes = faodel::BoostPack<MyContainer>(bag1);

  cout <<"Serialized size is "<<s_bytes.size()<<endl;

  //Unpack the bytes into a new object
  MyContainer bag2 = faodel::BoostUnpack<MyContainer>(s_bytes);

  //Look at both objects
  bag1.dump();
  bag2.dump();

}