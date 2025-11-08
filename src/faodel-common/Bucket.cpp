// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <iostream>

#include <cinttypes>

#include "faodel-common/Bucket.hh"
#include "faodel-common/StringHelpers.hh"

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
  bid = faodel::UnpackHash32(bucket_string);
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
