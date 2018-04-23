// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include <iostream>
#include <stdexcept>
#include "kelpie/Key.hh"

using namespace std;

namespace kelpie {


/**
 * @brief A Packing function for converting a key into a binary string
 *
 * @return string A packed binary string that pup(string) can revive
 */
string Key::pup() const {

  //TODO: add something to automatically handle different lengths
  //Currently: we limit strings to 255 bytes and then pack a 2 byte
  //header
  
  const int MAX_STRING_BYTES = 256 - 1;
  
  if((k1.size()>MAX_STRING_BYTES) ||
     (k2.size()>MAX_STRING_BYTES)   ) {
    throw std::runtime_error("Cannot pack key with string larger than "+std::to_string(MAX_STRING_BYTES)+" bytes");
  }
  string s;
  size_t len = 2+k1.size()+k2.size();
  s.resize(len);

  s[0] = static_cast<uint8_t>(k1.size() & 0x0ff);
  s[1] = static_cast<uint8_t>(k2.size() & 0x0ff);
  size_t i=2;
  for(size_t j=0; j<k1.size(); i++, j++) s[i] = k1[j];
  for(size_t j=0; j<k2.size(); i++, j++) s[i] = k2[j];
  
  return s;
}


/**
 * @brief A function that unpacks a pup()'d string into an existing key
 *
 * @param[in] packed_string A string containing binary, packed data
 */
void Key::pup(const std::string &packed_string) {

  //TODO: add something to automatically handle different lengths
  
  int s0{0},s1{0};
  if(packed_string.size()>2) { //2 bytes for length fields, which can be zero
    s0=static_cast<uint8_t>(packed_string[0]);
    s1=static_cast<uint8_t>(packed_string[1]);
  }
  if(s0+s1+2>static_cast<int>(packed_string.size())){
    throw std::runtime_error("Error unpacking key");
  }
  k1 = (s0==0) ? "" : packed_string.substr(2,   s0); 
  k2 = (s1==0) ? "" : packed_string.substr(2+s0,s1); 

}

} // namespace kelpie
