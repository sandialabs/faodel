// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef FAODEL_COMMON_BUCKET_HH
#define FAODEL_COMMON_BUCKET_HH

#include <sstream>
#include <stdexcept>

#include "faodel-common/FaodelTypes.hh"


namespace faodel {

/**
 * @brief POD struct for holding a hash of a string the user provides
 */
struct Bucket {
  uint32_t bid; //!< The hashed value of the string

  Bucket():bid(0) {} //Empty when given nothing
  Bucket(uint32_t b, internal_use_only_t iuo) {bid=b;} //!< Reserved for testing
  explicit Bucket(const std::string &bucket_string);
  bool operator== (const Bucket &other) const { return (bid == other.bid); }
  bool operator!= (const Bucket &other) const { return (bid != other.bid); }
  bool operator<  (const Bucket &other) const { return (bid < other.bid); }

  bool Valid() const { return (bid!=0); } //!< True if bucket is not BUCKET_UNSPECIFIED
  bool Unspecified() const { return (bid==0); } //!< True if bucket is BUCKET_UNSPECIFIED

  std::string GetHex() const;
  uint32_t    GetID() const { return bid; } //!< Returns the internal 32b hash value

  //Serialization hook
  template <typename Archive>
  void serialize(Archive &ar, const unsigned int version) {
    ar & bid;
  }
};

using bucket_t = Bucket;


const bucket_t BUCKET_UNSPECIFIED(0, internal_use_only);

/**
 * @brief An exception used to deal with bad bucket string parsing
 */
class BucketParseError : public std::runtime_error {
public:
  BucketParseError()
    : std::runtime_error( "Format problem while parsing Bucket string" ) {}
  explicit BucketParseError( const std::string& s )
    : std::runtime_error( "Format problem while parsing Bucket string: " + s ) {}
};

} // namespace faodel

#endif // FAODEL_COMMON_BUCKET_HH
