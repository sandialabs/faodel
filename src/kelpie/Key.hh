// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef KELPIE_KEY_HH
#define KELPIE_KEY_HH

#include <string>
#include <sstream>


namespace kelpie {

/**
 * @brief A simple data structure for labeling data objects in Kelpie
 *
 * Kelpie uses a Key to label all data objects that are passed around at
 * runtime. A key contains two strings (often called the "row" and 
 * "column" components), though the second string is optional. Multiple
 * constructors are provided for users to create different types of 
 * keys.
 *
 * This class provides multiple hooks for serializing Keys in order to 
 * make the key class usable in different environments. Specifically:
 *
 * - **Boost/Cereal**: The serialize() function is provided to do both
 *   packing and unpacking with libraries like Boost.
 * - **DIY**: If you're manually serdesing data, the pup functions 
 *   provide a simple way to convert the two strings into a single
 *   string that can be unpacked into a key.
 *   
 *
 * @note Kelpie allows users to pass binary data in as key values. However,
 *       users should be aware that keys with binary data will likely break
 *       printing functions in various places (eg whookie)
 */
class Key {
public:
  Key() {}

  Key(const std::string &k1_) : k1(k1_) {}
  Key(const std::string &k1_, const std::string &k2_) : k1(k1_), k2(k2_) {}

  // Make it easy to pass in binary values for keys
  Key(const void *k1_ptr, int k1_len) :
    k1(std::string(static_cast<const char *>(k1_ptr), static_cast<const char *>(k1_ptr)+k1_len))
  {}

  Key(const void *k1_ptr, int k1_len,
      const void *k2_ptr, int k2_len) :
    k1(std::string(static_cast<const char *>(k1_ptr), static_cast<const char *>(k1_ptr)+k1_len)),
    k2(std::string(static_cast<const char *>(k2_ptr), static_cast<const char *>(k2_ptr)+k2_len))
  {}


  // make dtor virtual to allow inherited classes to be properly dtor'ed when use through
  // a Key pointer
  virtual ~Key() {}

  // The row and col keys are mutable
  std::string K1()                  const { return k1; }     //!< Get the row label
  std::string K2()                  const { return k2; }     //!< Get the column label
  void        K1(std::string value)       { k1 = value; }    //!< Set the row label
  void        K2(std::string value)       { k2 = value; }    //!< Set the column label

  //Wildcard checks
  bool IsRowWildcard() const;
  bool IsColWildcard() const;
  bool IsWildcard() const;

  //Prefix check
  bool matchesPrefixString(bool row_is_prefix, const std::string &row_match,
                           bool col_is_prefix, const std::string &col_match) const;

  //Wildcard check
  bool Matches(const std::string &row_wildcard, const std::string &col_wildcard) const;
  bool Matches(const Key &pattern_key) const { return Matches(pattern_key.K1(), pattern_key.K2()); }

  //Simple pack/unpack to binary string
  std::string pup() const;
  void pup(const std::string &packed_string);
  
  /**
   * @brief Use a template to set the row portion of the key
   * @param[in] value The value to use for the row portion of a key
   * @note The conversion is performed via stringstream
   */
  template<typename KeyType>
  void TemplatedK1(const KeyType & value) {
    std::stringstream ss; ss << value;
    k1 = ss.str();
  }

  /**
   * @brief Use a template to set the column portion of the key
   * @param[in] value The value to use for the column portion of a key
   * @note The conversion is performed via stringstream
   */
  template<typename KeyType>
  void TemplatedK2(const KeyType & value) {
    std::stringstream ss; ss << value;
    k2 = ss.str();
  }

  /*
   * @brief Return this key as a simple string (for printing/debugging)
   * @retval String in the form of "k1|k2"
   */
  std::string str() const { return k1+"|"+k2; }          //!< Join the row/column labels into one string (for debugging)

  std::string str_as_args() const;                      //!< Return in argument string form (eg '-k1 key1 -k2 key2')

  bool operator== (const Key &other) const { return   (k1 == other.K1()) && (k2 == other.K2());  }
  bool operator!= (const Key &other) const { return !((k1 == other.K1()) && (k2 == other.K2())); }
  bool operator<  (const Key &other) const { return (k1 < other.K1()) || (k1 == other.K1() && k2 < other.K2()); }

  /** @brief A template needed to make a Key serializable by Boost and Cereal */
  template <typename Archive>
  void serialize(Archive &ar, const unsigned int version) {
    ar & k1;
    ar & k2;
  }
  uint16_t k1_size() const { return k1.size(); }         //!< Length of the row name
  uint16_t k2_size() const { return k2.size(); }         //!< Length of column name
  size_t size() const { return k1.size() + k2.size(); }  //!< Total length of the row and column names

  bool valid() const { return (k1.size() > 0); }         //!< A valid key has to at least have a row name

  static Key Random(size_t k1_length, size_t k2_length=0);           //!< Generate a random string key of specified length
  static Key Random(const std::string &k1_name, size_t k2_length=0); //!< Generate a key with a fixed row name and random column name
  static Key Random(size_t k1_length, const std::string &k2_name);   //!< Generate a key with a fixed column name and random row name

private:
  std::string k1;
  std::string k2;
};  // key



/**
 * @brief Use a template to generate a 1D key (row-only)
 * @param[in] val The label to use as the row name of the key
 * @retval 1D Key
 */
template<typename KeyType>
Key KeyGen(const KeyType &val) {
  Key key;
  key.TemplatedK1<KeyType>(val);
  return key;
}

/**
 * @brief Use a template to generate a 2D key
 * @param[in] val The labels to use as the row and column names of the key
 * @retval 2D Key
 */
template<typename KeyType1, typename KeyType2>
Key KeyGen(const KeyType1 &val1, const KeyType2 &val2) {
  Key key;
  key.TemplatedK1<KeyType1>(val1);
  key.TemplatedK2<KeyType2>(val2);
  return key;
}

}  // namespace kelpie

#endif  // KELPIE_KEY_HH
