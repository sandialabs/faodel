// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#ifndef FAODEL_COMMON_STRINGHELPERS_HH
#define FAODEL_COMMON_STRINGHELPERS_HH

#include <vector>
#include <string>

#include "common/FaodelTypes.hh"
#include "common/Bucket.hh"

namespace faodel {

std::string MakePunycode(std::string const &s);
std::string ExpandPunycode(std::string const &s);

bool IsValidIPString(const std::string &hostname);
int StringToInt64(int64_t *val, const std::string &name);
int StringToUInt64(uint64_t *val, const std::string &name);
int StringToPtr(void **val, const std::string &sval);

bool StringBeginsWith(const std::string &s, const std::string &search_prefix);
bool StringEndsWith(const std::string &s, const std::string &search_suffix);

std::vector<std::string> Split(const std::string &text, char sep, bool remove_empty=true);
void Split(std::vector<std::string> &tokens, const std::string &text, char sep, bool remove_empty=true);
std::string Join(const std::vector<std::string> &tokens, char sep);

std::string ToLowercase(std::string const &s);
void ToLowercaseInPlace(std::string &s);


std::vector<std::string> SplitPath(std::string const &s);
std::string              JoinPath(std::vector<std::string> const &vv, int num_items);

void ConvertToHexDump(const char *x, ssize_t len, int chars_per_line, int grouping_size,
                      std::string even_prefix, std::string even_suffix,
                      std::string odd_prefix,  std::string odd_suffix,
                      std::vector<std::string> *byte_offsets,
                      std::vector<std::string> *hex_lines,
                      std::vector<std::string> *txt_lines);
void ConvertToHexDump(const char *x, ssize_t len,  int chars_per_line, std::string *hex_part, std::string *txt_part);
void ConvertToHexDump(const std::string s, int chars_per_line, std::string *hex_part, std::string *txt_part);

uint32_t hash_dbj2(const std::string &s);
uint32_t hash_dbj2(const bucket_t &bucket, const std::string &s);
uint32_t hash32(const std::string &s); //Maps to hash_dbj2



} // namespace faodel

#endif // FAODEL_COMMON_STRINGHELPERS_HH
