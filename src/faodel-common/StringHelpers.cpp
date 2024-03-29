// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <climits>
#include <string>
#include <vector>
#include <iostream>
#include <iomanip>
#include <algorithm>

#include <stdexcept>

#include <wordexp.h>
#include <set>

#include "faodel-common/Debug.hh"
#include "faodel-common/StringHelpers.hh"

using namespace std;

namespace faodel {
/**
 * @brief Convert an input string where non-alpha-numeric vals are converted to percent-escaped hex values
 * @param s -  Input string
 * @return string - Puny code string
 */
string MakePunycode(string const &s) {
  const char *c = s.c_str();
  stringstream ss;
  size_t i=0;
  //Note: previously looked for '0' to end, but this breaks c++ things
  while(i!=s.size()) {
    if(isalnum(c[i])) ss<<c[i];
    else {
      unsigned x = ((unsigned)c[i]) & 0x0FF;
      ss << "%";
      ss <<  setw(2) << hex << setfill('0') << x ;
    }
    i++;
  }
  return ss.str();
}

/**
 * @brief Convert a punycode string into normal string
 * @param s -  Punycode-formatted string
 * @return string - Regular string
 */
string ExpandPunycode(string const &s) {
  const char *c = s.c_str();
  stringstream ss;
  int i=0;
  while(c[i]!='\0') {
    if(c[i]=='%') {
      i++;

      if( isxdigit(c[i]) && isxdigit(c[i+1]) ) {

        unsigned char tmp;
        if     (isdigit(c[i])) tmp =      c[i] - '0';
        else if(islower(c[i])) tmp = 10 + c[i] - 'a';
        else                   tmp = 10 + c[i] - 'A';
        tmp=tmp<<4;
        i++;
        if     (isdigit(c[i])) tmp |=      c[i] - '0';
        else if(islower(c[i])) tmp |= 10 + c[i] - 'a';
        else                   tmp |= 10 + c[i] - 'A';
        i++;
        ss << tmp;
      }
    } else {
      ss << c[i];
      i++;
    }
  }
  return ss.str();
}

bool IsValidIPString(const string &hostname) {
  vector<string> octets;

  octets = Split(hostname, '.', false);

  bool all_digits=true;
  bool has_digits=false;

  for(auto &s : octets) {
    if(s.empty()) return false;
    char *p;
    long val = strtol(s.c_str(), &p,10);
    if(*p) {
      //Couldn't parse.. not a digit
      all_digits=false;
    } else if((val>=0) && (val<=255)) {
      has_digits=true;
    } else {
      has_digits=true;
      all_digits=false;
    }
  }

  //see if ipv4 10.0.0.1
  if(all_digits) return (octets.size()==4);
  return (!has_digits); //ignore if out-of-range digits
}

/**
 * @brief Convert a numerical string (eg "100" "4K") into an int32 value (eg 100, 4096)
 * @param[out] val The output variable to set
 * @param[in] token Input string to parse
 * @retval 0 If input could be parsed
 * @retval EINVAL If input string couldn't be parsed
 */
int StringToInt32(int32_t *val, const std::string &token) {
  F_ASSERT(val, "Null val ptr handed to StringToInt32");
  int64_t val64;
  int rc = StringToInt64(&val64, token);
  if(rc!=0) return rc;
  *val = (val64 & 0x0FFFFFFFFL);
  return 0;
}

/**
 * @brief Convert a numerical string (eg "100" "4K") into an uint32 value (eg 100, 4096)
 * @param[out] val The output variable to set
 * @param[in] token Input string to parse
 * @retval 0 If input could be parsed
 * @retval EINVAL If input string couldn't be parsed
 */
int StringToUInt32(uint32_t *val, const std::string &token) {
  F_ASSERT(val, "Null val ptr handed to StringToUInt32");
  uint64_t val64;
  int rc = StringToUInt64(&val64, token);
  if(rc!=0) return rc;
  *val = (val64 & 0x0FFFFFFFFL);
  return 0;
}

/**
 * @brief Convert a numerical string (eg "100" "4K") into an int64 value (eg 100, 4096)
 * @param[out] val The output variable to set
 * @param[in] token Input string to parse
 * @retval 0 If input could be parsed
 * @retval EINVAL If input string couldn't be parsed
 */
int StringToInt64(int64_t *val, string const &token) {
  F_ASSERT(val, "Null val ptr handed to StringToInt64");
  string sname = token;
  int64_t multiplier=1;
  char last_char = *sname.rbegin();
  if(!isdigit(last_char)) {
    switch(tolower(last_char)) {
    case'k': multiplier = 1024; break;
    case'm': multiplier = 1024*1024; break;
    case'g': multiplier = 1024*1024*1024; break;
    default:
      cerr << "Parsing problem decyphering " << token << " as an integer string\n";
      return EINVAL;
    }
  }

  //Pass back to the user
  *val = multiplier *  atoll(sname.c_str());

  return 0;
}
/**
 * @brief Convert a numerical string (eg "100" "4K") into an uint64 value (eg 100, 4096)
 * @param[out] val The output variable to set
 * @param[in] token Input string to parse
 * @retval 0 If input could be parsed
 * @retval EINVAL If input string couldn't be parsed
 */
int StringToUInt64(uint64_t *val, string const &token) {
  F_ASSERT(val, "Null val ptr handed to StringToUInt64");
  string sname = token;
  int64_t multiplier=1;
  char last_char = *sname.rbegin();
  if(!isdigit(last_char)) {
    switch(tolower(last_char)) {
    case'k': multiplier = 1024; break;
    case'm': multiplier = 1024*1024; break;
    case'g': multiplier = 1024*1024*1024; break;
    default:
      cerr << "Parsing problem decyphering " << token << " as an unsigned integer string\n";
      return EINVAL;
    }
  }

  //Pass back to the user
  *val = multiplier *  strtoul(sname.c_str(), nullptr, 0);
  if ((*val == ULONG_MAX) && (errno == ERANGE)) {
      // sname is not a valid uint64
      return EINVAL;
  }

  return 0;
}
/**
 * @brief Convert a numerical string into a pointer value
 * @param[out] val The output variable to set
 * @param[in] token Input string to parse
 * @retval 0 If input could be parsed
 * @retval EINVAL If input string couldn't be parsed
 * @note This function is **not** commonly used and can be dangerous
 */
int StringToPtr(void **val, string const &token) {
  F_ASSERT(val, "Null val ptr handed to StringToPtr");

  //Pass back to the user
  *val = (void*)strtoull(token.c_str(), nullptr, 16);

  return 0;
}

/**
 * @brief Convert a string with a boolean flag into a value
 * @param val The output
 * @param token  Input string to parse (valid options: true,false,1,0,t,f)
 * @retval 0 If input token could be parsed
 * @retval EINVAL if input token couldn't be parsed
 */
int StringToBoolean(bool *val, const string &token) {
  if(token.empty()) { if(val) *val=false; return EINVAL; }
  string ltoken = ToLowercase(token);
  if((ltoken=="true") || (ltoken=="1") || (ltoken=="t")) {
    if(val) *val = true;
    return 0;
  }
  if((ltoken=="false") || (ltoken=="0") || (ltoken=="f")) {
    if(val) *val = false;
    return 0;
  }
  return EINVAL;
}

/**
 * @brief Convert a time string (with us,ms,minutes,hours,seconds,s suffixes) to a uint64 microsecond value
 * @param val The output in microseconds
 * @param token Input string to parse
 * @retval 0 If input could be parsed
 * @retval EINVAL if input string couldn't be parsed
 */
int StringToTimeUS(uint64_t *val, string const &token) {
  F_ASSERT(val, "Null val ptr handed to StringToTimeUS");
  if(token.empty()) { *val=0; return EINVAL; }

  vector<pair<string, uint64_t>> table = { {"us",      1},
                                           {"ms",      1000},
                                           {"minutes", 60*1000*1000ul},
                                           {"hours",   3600*1000*1000ul},
                                           {"seconds", 1000*1000ul},
                                           {"s",       1000*1000ul}  //note:make sure this is last since others end in s
                                         };
  uint64_t multiplier=1; //Assume us if not specified
  string sname = faodel::ToLowercase(token);
  for(auto &name_mult : table) {
    if(StringEndsWith(sname, name_mult.first)) {
      multiplier = name_mult.second;
      size_t last=sname.size()-name_mult.first.size();
      while((last>0) && (sname.at(last-1)==' ')) //trim spaces
        last--;
      sname = sname.substr(0, last);
      break;
    }
  }
  //Check for non-numbers
  bool is_good= !sname.empty();
  for(int i=0; (is_good) && (i<sname.size()); i++) {
    is_good = isdigit(sname.at(i));
  }

  if(is_good) {
    *val = multiplier * strtoul(sname.c_str(), nullptr, 0);
    is_good = !((*val == ULONG_MAX) && (errno == ERANGE));
  }
  if(!is_good) {
    *val = 0;
    return EINVAL;
  }

  return 0;
}

/**
 * @brief Split a string into a vector of components. Based on s/o 236129 comment
 * @param text[in] -  String to parse
 * @param sep[in] - Delimiter used to split the string
 * @param remove_empty - Allow user to specify whether empy fields are removed (eg. a:b::c gives "a","b","","c" or "a","b","c")
 * @retval vector<string> - Tokens extracted from operation
 */
vector<string> Split(const string &text, char sep, bool remove_empty) {
  vector<string> tokens;
  Split(tokens, text, sep, remove_empty);
  return tokens;
}
/**
 * @brief Split a string into a vector of components. Based on s/o 236129 comment
 * @param[out] tokens - Vector of result tokens (results are appended)
 * @param text[in] -  String to parse
 * @param sep[in] - Delimiter used to split the string
 * @param remove_empty - Allow user to specify whether empy fields are removed (eg. a:b::c gives "a","b","","c" or "a","b","c")
 * @return void
 */
void Split(vector<string> &tokens, const string &text, char sep, bool remove_empty) {

  size_t start=0, end=0;
  while((end = text.find(sep, start)) != string::npos) {
    if((!remove_empty) || (start!=end)) {
      tokens.push_back(text.substr(start, end-start));
    }
    start = end+1;
  }
  if((!remove_empty) || (start!=text.size())) {
    tokens.push_back(text.substr(start));
  }
}

/**
 * @brief Copy the input string and convert it to lowercase
 * @param s[in] -  Input string
 * @return string - A lowercase copy of the string
 */
string ToLowercase(string const &s) {
  string tmp = s;
  std::transform(tmp.begin(), tmp.end(), tmp.begin(), ::tolower);
  return tmp;
}
/**
 * @brief Converts a string to lowercase (changes original string)
 * @param s -  String to modify
 * @return void
 */
void ToLowercaseInPlace(string &s) {
  std::transform(s.begin(), s.end(), s.begin(), ::tolower);
}

//From stack overflow: s/o 440133
// Note: make sure you srand() before this since it's just the stock C rng!
string RandomString(size_t string_length) {
  auto randchar = []() -> char
  {
      const char charset[] =
              "0123456789"
              "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
              "abcdefghijklmnopqrstuvwxyz";
      const size_t max_index = (sizeof(charset) - 1);
      return charset[ rand() % max_index ];
  };
  std::string str(string_length,0);
  std::generate_n( str.begin(), string_length, randchar );
  return str;
}


/**
 * @brief Determine if string begins with a specific prefix
 *
 * @param[in] s The string to examine
 * @param[in] search_prefix The prefix we're looking for
 * @retval true The string begins with the complete prefix
 * @retval false The string does not begin with the complete prefix
 */
bool StringBeginsWith(const std::string &s, const std::string &search_prefix) {
  if(search_prefix.size() > s.size()) return false;
  auto res = std::mismatch(search_prefix.begin(), search_prefix.end(), s.begin());
  return (res.first == search_prefix.end());
}

/**
 * @brief Determine if a string ends with a specific suffix
 *
 * @param[in] s The string to examine
 * @param[in] search_suffix The suffix we're looking for
 * @retval true The string ends with the complete suffix
 * @retval false The string does not end with the complete suffix
 */
bool StringEndsWith(const std::string &s, const std::string &search_suffix) {
  if(search_suffix.size() > s.size()) return false;
  return std::equal(s.begin()+s.size() - search_suffix.size(),
                    s.end(),
                    search_suffix.begin());
}

/**
 * @brief Convert an integer into a zero-padded string of a specified number of digits
 * @param val The number to use
 * @param to_num_digits The number of string digits to return
 * @return Zero-padded string (eg, 93 zero padded to 5 digits becomes "00093")
 */
std::string StringZeroPad(int val, int to_num_digits) {
  string s=std::to_string(val);
  if(s.size()<to_num_digits)
    s = string(to_num_digits - s.size(),'0') + s;
  return s;
}

string StringCenterTitle(const std::string &s) {
  if(s.empty()) return string(80,'=');
  stringstream ss;
  string stmp = (s.size()<76) ? s : s.substr(0,76);
  size_t left = (80 - (stmp.size()+2))/2;
  size_t right = (80 - (left + 1 + stmp.size() + 1));
  ss << string(left,'=') <<" " << stmp <<" "<< string(right, '=');
  return ss.str();
}

/**
 * @brief Split a path into a vector of strings
 */
vector<string> SplitPath(string const &s) {
  vector<string> tokens;
  stringstream ss(s);
  string item;
  while(getline(ss,item,'/')) {
    if(!item.empty()) //For root case (otherwise /root would be "" "root") and ending slash
      tokens.push_back(item);
  }
  return tokens;
}

/**
 * @brief Join a vector to make a path: vv=(a,b,c,d,e), num_items=3, gives /a/b/c
 */
string JoinPath(vector<string> const &vv, int num_items) {

  F_ASSERT(num_items >= 0, "JoinPath");
  F_ASSERT(num_items <= (int)vv.size(), "JoinPath");

  stringstream ss;
  for(int i=0; i<num_items; i++)
    ss << "/" << vv[i];
  return ss.str();
}


string Join(const vector<string> &tokens, char sep) {
  stringstream ss;
  for(size_t i=0; i<tokens.size(); i++) {
    ss<<tokens[i];
    if(i+1<tokens.size())
      ss<<sep;
  }
  return ss.str();
}

/**
 * @brief Use wordexp() to perform symbol expansion on a string
 *
 * @param[in] s The path to expand
 * @param[in] flags Options to wordexp()
 * @retval string The expanded path
 * @retval string An empty string ("") if wordexp() returned an error
 */
string ExpandPath(std::string const &s, int flags) {
  wordexp_t p;
  string result;

  int rc = wordexp(s.c_str(), &p, flags);
  if ((rc==0) && (p.we_wordc==1)) {
      result = string(p.we_wordv[0]);
      wordfree(&p);
      return result;
  }
  return string("");
}
/**
 * @brief Use wordexp() to perform symbol expansion on a string allowing all substitutions
 *
 * @param[in] s The path to expand
 * @retval string The expanded path
 * @retval string An empty string ("") if wordexp() returned an error
 */
string ExpandPath(std::string const &s) {
  return ExpandPath(s, 0);
}
/**
 * @brief Use wordexp() to perform symbol expansion on a string disallowing command substitution
 *
 * @param[in] s The path to expand
 * @retval string The expanded path
 * @retval string An empty string ("") if wordexp() returned an error
 */
string ExpandPathSafely(std::string const &s) {
  return ExpandPath(s, WRDE_NOCMD);
}


/**
 * @brief Resolve an item in a component string, guessing "item" first, then "item.env_name"
 * @param item The name of the item to resolve (eg path, file)
 * @param settings The map of settings extacted from a config for a particular item
 * @retval The resolved string (possibly pulled from env var)
 * @retval Empty string when not found or env var did no exist
 *
 * Sometimes we pull out a chunk of settings from a Configuration that are related to
 * a specific entity. While most of the time we use k/v pairs, users can also append
 * the key name with ".env_name" to specify that we should get the actual value from
 * the env var that was specified. See IomPosixIndividualObjetcs for an example
 */
string GetItemFromComponentSettings(const string &item, const map<string,string> &settings) {
  string resolved_value;

  auto ii = settings.find(item);
  if(ii != settings.end()) {
    resolved_value = ii->second;
  } else {
    ii = settings.find(item + ".env_name");
    if( (ii != settings.end()) && (!ii->second.empty()) ) {
      char *c_item_var = getenv(ii->second.c_str());
      if(c_item_var != nullptr) {
        resolved_value = string(c_item_var);
      }
    }
  }
  return resolved_value;
}
/**
 * @brief Shortcut for pulling a file from a component setting (appends "/" if needed)
 * @param settings Component settings to examine
 * @retval path (ending in '/')
 * @retval empty (not found)
 */
std::string GetPathFromComponentSettings(const std::map<std::string,std::string> &settings) {
  string p = GetItemFromComponentSettings("path", settings);
  if((!p.empty()) && (p.back()!='/')) p=p+"/";
  return p;
}
std::string GetFileFromComponentSettings(const std::map<std::string,std::string> &settings) {
  return GetItemFromComponentSettings("file", settings);
}



void ConvertToHexDump(const char *x, ssize_t len, int chars_per_line,
                      int grouping_size,
                      const string &even_prefix, const string &even_suffix,
                      const string &odd_prefix,  const string &odd_suffix,
                      vector<string> *byte_offsets,
                      vector<string> *hex_lines,
                      vector<string> *txt_lines) {

  if((len<1)||(chars_per_line<1) || (grouping_size<1)) return;

  //Auto adjust line size to be a multiple of group size
  if(chars_per_line > grouping_size) {
    while((chars_per_line % grouping_size)!=0) {
      chars_per_line--;
    }
  }

  stringstream ss_hex, ss_txt;

  bool is_even=true;
  int spot=chars_per_line;
  int leftover = (len%chars_per_line);
  int padded_len= ((leftover==0) ? len : len + chars_per_line-leftover);

  for(int i=0; i<padded_len; i++) {
    spot--;

    //Start of new line
    if(spot==chars_per_line-1) {
      if(byte_offsets) byte_offsets->push_back(to_string(i));
      is_even=true; //Line always starts even
    }

    //Add prefix to beginning of word
    if((i%grouping_size)==0) {
      ss_hex << ((is_even) ? even_prefix : odd_prefix);
      ss_txt << ((is_even) ? even_prefix : odd_prefix);
    }

    //Only print when we're in valid range
    if(i<len) {

      ss_hex<< std::uppercase << setfill('0') << setw(2) << hex
            << ((unsigned int)(x[i] & 0x0FF))
            << ((spot!=0) ? " " : "");
      ss_txt<< (isprint(x[i]) ? x[i] : '.');
    }

    //Add suffix to end of word
    if(((i+1)%grouping_size)==0) {
      ss_hex << ((is_even) ? even_suffix : odd_suffix);
      ss_txt << ((is_even) ? even_suffix : odd_suffix);
      is_even = !is_even;
    }

    //End of line - should always get here because of padding
    if(spot==0) {
      if(hex_lines) hex_lines->push_back(ss_hex.str());
      if(txt_lines) txt_lines->push_back(ss_txt.str());
      //Wipe out the string streams
      ss_hex.str("");
      ss_txt.str("");
      spot=chars_per_line;
    }
  }

}


void ConvertToHexDump(const char *x, ssize_t len,  int chars_per_line,
                      string *hex_part, string *txt_part) {

  if((len<=0)||(chars_per_line<=0)) return;


  stringstream ss_hex, ss_txt;

  int spot=chars_per_line;
  for(int i=0; i<len; i++) {
    spot--;
    ss_hex<< std::uppercase << setfill('0') << setw(2) << hex
          << ((unsigned int)(x[i] & 0x0FF)) << ((spot==0) ?"\n":" ");
    ss_txt<< (isprint(x[i]) ? x[i] : '.') << ((spot==0) ? "\n" : "");
    if(spot==0) spot=chars_per_line;
  }
  if(hex_part) *hex_part = ss_hex.str();
  if(txt_part) *txt_part = ss_txt.str();
}

void ConvertToHexDump(const string &s, int chars_per_line,
                      string *hex_part, string *txt_part) {

  ConvertToHexDump(s.c_str(),s.size(), chars_per_line, hex_part, txt_part);
}

/**
 * @brief Compute a simple 32b hash via Dan Bernstein's djb2 algorithm
 * @param s String to hash
 * @return uint32_t hash value
 * @note This hash comes from http://www.cse.yorku.ca/~oz/hash.html
 */
uint32_t hash_dbj2(const string &s) {

  //Taken from: djb2 dan bernstein: http://www.cse.yorku.ca/~oz/hash.html
  const char *str = s.c_str();
  unsigned long hash = 5381;
  int c;
  while((c = *str++)!=0) {
    hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
  }
  return static_cast<uint32_t>(hash);
}

/**
 * @brief Generate a hash from a bucket and string. Bucket's hash is prepended to string
 * @param bucket The bucket for this item
 * @param s The string to hash
 * @return A 32b hash of the bucket+string
 * @note This hash comes from http://www.cse.yorku.ca/~oz/hash.html
 */
uint32_t hash_dbj2(const bucket_t &bucket, const string &s) {

  //Modified version of dbj2
  const char *str = s.c_str();
  unsigned long hash = 5381;

  //Hash the bucket's 4 bytes first
  uint32_t bval=bucket.bid;
  for(int i=0; i<4; i++) {
    hash = ((hash<<5)+hash) + (bval&0x0FF);
    bval = (bval>>8);
  }

  //hash the string
  int c;
  while((c = *str++)!=0) {
    hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
  }
  return static_cast<uint32_t>(hash);
}

uint32_t hash32(const std::string &s) {
  return hash_dbj2(s);
}
uint16_t hash16(const std::string &s) {
  uint32_t h = hash32(s);
  return (h>>16) ^ (h & 0x0FFFF);
}

/**
 * @brief Unpack a hash id from a packed string. String may be the value ("0x12345678") or a string to hash ("foo")
 * @param s The string holding the hash value
 * @return uint32_t the integer hash value
 * @throws runtime_error if parse error with string (ie, starts with 0x, but doesn't have valid hex string)
 */
uint32_t UnpackHash32(const std::string &s) {

  //Just hash it if it doesn't begin with 0x
  if(s.compare(0, 2, "0x")!=0) {
    return hash32(s);

  } else {

    //Make sure this is less than 32b ("0x"+8hex digits)
    if(s.size()>10) {
      throw std::runtime_error("UnpackHash32 given a string starting with 0x that is larger than a 32b hash");
    }

    //Make sure all parts are valid hex
    for(size_t i=2; i<s.size(); i++) {
      if(!isxdigit(s.at(i))) {
        throw std::runtime_error("UnpackHash32 hex string starting with 0x contained a non-hex symbol");
      }
    }

    //Extract out the hex value
    unsigned long val;
    stringstream ss;
    ss << std::hex << s.substr(2); //0x not supported
    ss >> val;
    //Note c++ has std::stoul("0xaaaaa", nullptr, 16)
    return static_cast<uint32_t>(val);

  }
}
/**
 * @brief Parse a token and convert it to an id within a range if valid
 * @param token The string to parse (intergers, 'end','middle','middleplus')
 * @param num_nodes Bounds the maximum value of token to (num-nodes-1)
 * @retval -1 Invalide input
 * @retval (num_nodes-1) for "end"
 * @retval (num_nodes-1)/2 for "middle"
 * @retval ((num_nodes-1)/2)+1) for "middleplus"
 * @retval [0:num_nodes-1] for valid input
 */
int parseIDInRange(const std::string &token, int num_nodes) {
  if(token.empty()) return -1;
  if(token=="first") return 0;
  if(token=="last") return num_nodes-1;
  if(token=="end") return num_nodes-1;
  if(token=="middle") return (num_nodes-1)/2;
  if(token=="middleplus") return (((num_nodes-1)/2)+1);

  //Make sure this is a real number (not " 1 12" or "123bad")
  bool previous_was_space=true;
  bool previously_hit_word=false;
  for(size_t i=0; i<token.size(); i++) {
    char x = token.at(i);
    if(x!=' ') {
      if(!isdigit(x)) return -1; //Not a number
      if((previous_was_space) && (previously_hit_word)) return -1; // " 123 1"
      previously_hit_word=true;
    }
    previous_was_space = (x==' ');
  }

  return atoi(token.c_str());
}

/**
 * @brief Given a text string of ranges, return a set of all integer values
 * @param line The input string (eg "1", "1-3", "1,3,5", "2-4,3,5-9", or "0-middle,end"
 * @param num_nodes Bounds the maximum value in the line to (num_nodes-1)
 * @return Set containing all the values
 * @thows runtime_error When invalid input
 */
std::set<int> ExtractIDs(const std::string &line, int num_nodes) {

  string s=ToLowercase(line);

  set<int> items;

  auto tokens = Split(s,',',true);
  for(auto &t : tokens) {
    if(t=="all") {
      for (int i = 0; i < num_nodes; i++) {
        items.insert(i);
      }
      return items;
    }
    auto range_val = Split(t,'-',true);
    if((range_val.size()>2) || (range_val.empty()))
      throw std::runtime_error("ExtractID Parse problem in token '"+t+"' for '"+line+"'");

    if(range_val.size()==2) {
      //This is some range value, like 2-4 or 2-end
      int a = parseIDInRange(range_val[0], num_nodes);
      int b = parseIDInRange(range_val[1], num_nodes);
      if((a>b) || (b<0) || (a>=num_nodes) || (b>=num_nodes)) // ||(a==b))
        throw std::runtime_error("ExtractID Range parse problem in token '"+t+"' for '"+line+"'");

      for(int i=a; i<=b; i++)
        items.insert(i);

    } else {

      int x = parseIDInRange(range_val[0], num_nodes);
      if((x<0)||(x>=num_nodes))
        throw std::runtime_error("ExtractID Parse problem in token '"+t+"' for '"+line+"'");
      items.insert(x);
    }
  }

  return items;
}

} // namespace faodel
