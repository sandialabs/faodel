// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include <iostream>
#include <sstream>

#include <cinttypes>

#include "common/Bucket.hh"
#include "common/StringHelpers.hh"

using namespace std;

namespace faodel {

namespace internal {
const uint32_t BUCKET_NULL_VAL = 5381;
} // namespace internal

static BucketParseError b_parseError;


/**
 * @brief Convert input string to a binary hash value
 *
 * @param[in] bucket_string Either the string to hash ("mybucket") or a hex
 *         string of the hash value ("0x0abcd")
 * @return Bucket
 *
 * @note Users typically pass in a string they want to hash. Passing in a hex
 *        string is less common (eg, when components need to exchange a
 *        compact representation of a namespace for a resource).
 * @note Bucket is currently a simple hash to a 32b value. It does not
 *        attempt to deal with collisions
 */
Bucket::Bucket(const string &bucket_string) {

  if(bucket_string.compare(0, 2, "0x")==0) {

    //Make sure this is less than 32b ("0x"+8hex digits)
    if(bucket_string.size() > 10) {
      throw BucketParseError("Hex string exceeds uint32_t capacity");
    }
    //Make sure all parts are valid hex
    for(size_t i=2; i<bucket_string.size(); i++) {
      if(!isxdigit(bucket_string.at(i))) {
        throw BucketParseError("Hex string contained non-hex value");
      }
    }

    unsigned long val;
    stringstream ss;
    ss << std::hex << bucket_string.substr(2); //0x not supported
    ss >> val;
    //Note c++ has std::stoul("0xaaaaa", nullptr, 16)
    bid = static_cast<uint32_t>(val);

  } else {
    //We were given some kind of string. Hash it down in order to make
    //things fit into a bucket id

    //NOTE: If you change this, make sure you update BUCKET_NULL value

    //Taken from: djb2 dan bernstein: http://www.cse.yorku.ca/~oz/hash.html
    const char *str = bucket_string.c_str();
    unsigned long hash = internal::BUCKET_NULL_VAL; //5381;
    int c;
    while((c = *str++)!=0) {
      hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }
    bid = static_cast<uint32_t>(hash);
  }

}


/**
 * @brief Generate a human-readable hex string for a bucket
 *
 * @return hex string (eg 0xabcd1234)
 */
string Bucket::GetHex() const {
  stringstream ss;
  ss << "0x"<<std::hex << bid;
  return ss.str();
}

} // namespace faodel
