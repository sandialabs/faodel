// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <iostream>
#include <stdexcept>
#include "faodel-common/StringHelpers.hh"
#include "kelpie/Key.hh"

using namespace std;

namespace kelpie {


/**
 * @brief Determine if Key has a wildcard in its row (ie, ends with '*')
 * @retval true It's a wildcard
 * @retval false Not a wildcard
 */
bool Key::IsRowWildcard() const {
  return faodel::StringEndsWith(k1, "*");
}

/**
 * @brief Determine if Key has a wildcard in its column (ie, ends with '*')
 * @retval true It's a wildcard
 * @retval false Not a wildcard
 */
bool Key::IsColWildcard() const {
  return faodel::StringEndsWith(k2, "*");
}

/**
 * @brief Determine if Key has a wildcard in either its row or column (ie, ends with '*')
 * @retval true It's a wildcard
 * @retval false Not a wildcard
 */
bool Key::IsWildcard() const {
  return IsRowWildcard() || IsColWildcard();
}

/**
 * @brief Determine whether this key matches row/col search parameters
 * @param row_is_prefix true=Allow any key that matches this prefix, false=Row name must be exact
 * @param row_match     The row name to match against (does NOT permit wildcards)
 * @param col_is_prefix true=Allow any key that matches this prefix, false=Col name must be exact
 * @param col_match     The col name to match against (does NOT permit wildcards)
 * @retval true The key matches
 * @retval false The key does NOT match
 *
 * This is a power user function, as it expects that you've looked at some key, determined
 * it was a wildcard or not, and removed the trailing '*' because you're searching a lot
 * of keys. The Matches() function below illustrates that kind of behavior.
 */
bool Key::matchesPrefixString(bool row_is_prefix, const std::string &row_match,
                              bool col_is_prefix, const std::string &col_match) const {

  //Check the row first
  if(row_is_prefix) {
    //User originally asked for "my_row_prefix*" or "*", so row_match is "my_row_prefix" or ""
    if( (!row_match.empty()) &&  //Empty in this case means match everything
        (!faodel::StringBeginsWith(k1, row_match))) {
      return false;
    }

  } else {
    //User is expecting an exact match
    if(k1 != row_match) return false;
  }

  //Check the column
  if(col_is_prefix) {
    //User originally asked for "my_col_prefix*" or "*", so col_match is "my_col_prefix" or ""
    if( (!col_match.empty()) &&  //Empty in this case means match everything
        (!faodel::StringBeginsWith(k2, col_match))) {
      return false;
    }

  } else {
    //User is expecting an exact match
    if(k2 != col_match) return false;
  }

  return true;
}

/**
 * @brief Determine if this key matches a row/col wildcards
 * @param row_wildcard The exact row name to match, or a wildcard (eg my_row*)
 * @param col_wildcard The exact col name to match, or a wildcard (eg my_col*)
 * @retval true This key matches the inputs
 * @retval false This key does NOT match the inputs
 */
bool Key::Matches(const std::string &row_wildcard, const std::string &col_wildcard) const {
  bool is_row_wildcard = faodel::StringEndsWith(row_wildcard, "*");
  bool is_col_wildcard = faodel::StringEndsWith(col_wildcard, "*");
  string row_prefix = (is_row_wildcard) ? row_wildcard.substr(0, row_wildcard.size()-1) : row_wildcard;
  string col_prefix = (is_col_wildcard) ? col_wildcard.substr(0, col_wildcard.size()-1) : col_wildcard;
  return matchesPrefixString(is_row_wildcard, row_prefix, is_col_wildcard, col_prefix);
}

/**
 * @brief Append (or replace) a numerical tag at the end of the key
 * @param new_tag An integer used by the tag folding table pool
 * @note This appends '{0x1234}' to the end of the row key. This is only used by the tag-folding-table pool for controlling where cpmtemy ;amds
 */
void Key::SetK1Tag(uint32_t new_tag) {

   //Remove the previous tag
   auto f = k1.find_last_of('{');
   if((f!=string::npos) && (!k1.empty()) && (k1.back()=='}')) {
      k1 = k1.substr(0, f);
   }

   stringstream ss;
   ss << k1 <<"{0x"<<std::hex<<new_tag<<"}";

   k1 = ss.str();
}

/**
 * @brief Extract an integer tag that has been placed at the end of the row part of the key
 * @param tag The integer value found in the tag, or zero if it doesn't exist
 * @retval KELPIE_OK found a tag
 * @retval KELPIE_ENOENT no tag was found (tag set to zero)
 * @note This extracts the int found at and end of a string, in the '{0x1234}' format
 * @note Tag MUST be hex string for a 32b number.
 */
faodel::rc_t Key::GetK1Tag(uint32_t *tag) const {

   auto f = k1.find_last_of('{');
   if( k1.empty() || (f==string::npos) || (k1.back() != '}') ) {
      if(tag) *tag=0;
      return KELPIE_ENOENT;
   }

   uint32_t val;
   auto s_num = k1.substr(f+1, k1.size()-f-2);
   stringstream ss(s_num);
   ss >>std::hex>>val;

   if(tag) *tag=val;
   return KELPIE_OK;
}

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

  size_t i=0;
  for(size_t j=0; j<k1.size(); i++, j++) s[i] = k1[j];
  for(size_t j=0; j<k2.size(); i++, j++) s[i] = k2[j];
  s[i++] = static_cast<uint8_t>(k1.size() & 0x0ff);
  s[i++] = static_cast<uint8_t>(k2.size() & 0x0ff);
  
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
    int i = packed_string.size() - 1;
    // WORKING backward from the end of the string
    s1=static_cast<uint8_t>(packed_string[i--]);
    s0=static_cast<uint8_t>(packed_string[i--]);
  }
  if(s0+s1+2>static_cast<int>(packed_string.size())){
    throw std::runtime_error("Error unpacking key");
  }
  k1 = (s0==0) ? "" : packed_string.substr(0,   s0); 
  k2 = (s1==0) ? "" : packed_string.substr(0+s0,s1); 
}


string Key::str_as_args() const {
  //Todo: would be nice to handle spaces via quotes
  stringstream ss;
  if(!k1.empty())                    ss<<"-k1 "<<k1;
  if((!k1.empty()) && (!k2.empty())) ss<<" ";
  if(!k2.empty())                    ss<<"-k2 "<<k2;
  return ss.str();
}

/**
 * @brief Generate a key with random alpha-numeric labels
 * @param k1_length The number of characters for the row part of the key
 * @param k2_length The number of characters for the col part of the key
 * @retval Key that has random printable characters for its row/column components
 */
Key Key::Random(size_t k1_length, size_t k2_length) {
  string s1 = faodel::RandomString(k1_length);
  if(k2_length==0) {
    return Key(s1);
  }
  string s2 = faodel::RandomString(k2_length);
  return Key(s1, s2);
}

/**
 * @brief Generate a key with a fixed row name and random alpha-numeric column name
 * @param k1_name The name to use for the row part of the key
 * @param k2_length The number of characters for the col part of the key
 * @retval Key that has random printable characters for its row/column components
 */
Key Key::Random(const string &k1_name, size_t k2_length) {
  if(k2_length==0) {
    return Key(k1_name);
  }
  string s2 = faodel::RandomString(k2_length);
  return Key(k1_name, s2);
}

/**
 * @brief Generate a key with a random alpha-numeric row name and a fixed column name
 * @param k1_length The number of characters for the row part of the key
 * @param k2_name The name to use for the column part of the key
 * @retval Key that has random printable characters for its row/column components
 */
Key Key::Random(size_t k1_length, const string &k2_name) {
  string s1 = faodel::RandomString(k1_length);
  return Key(s1, k2_name);
}




} // namespace kelpie
