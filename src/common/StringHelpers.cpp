// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include <climits>
#include <string>
#include <vector>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>

#include "common/Debug.hh"
#include "common/StringHelpers.hh"

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

        unsigned char tmp=0;
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

  for(auto s : octets) {
    if(s=="") return false;
    char *p;
    long val = strtol(s.c_str(), &p,10);
    if(*p) {
      //Couldn't parse.. not a digit
      all_digits=false;
    } else if((val>=0) || (val<=255)) {
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
 * @brief Convert a numerical string (eg "100" "4K") into an int64 value (eg 100, 4096)
 * @param[out] val The output variable to set
 * @param[in] name Input string to parse
 * @retval 0 If input could be parsed
 * @retval EINVAL If input string couldn't be parsed
 */
int StringToInt64(int64_t *val, string const &name) {
  kassert(val,"Null val ptr handed to StringToInt64");
  string sname = name;
  int64_t multiplier=1;
  char last_char = *sname.rbegin();
  if(!isdigit(last_char)) {
    switch(tolower(last_char)) {
    case'k': multiplier = 1024; break;
    case'm': multiplier = 1024*1024; break;
    case'g': multiplier = 1024*1024*1024; break;
    default:
      cerr << "Parsing problem decyphering " << name << " as an integer string\n";
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
 * @param[in] name Input string to parse
 * @retval 0 If input could be parsed
 * @retval EINVAL If input string couldn't be parsed
 */
int StringToUInt64(uint64_t *val, string const &name) {
  kassert(val,"Null val ptr handed to StringToUInt64");
  string sname = name;
  int64_t multiplier=1;
  char last_char = *sname.rbegin();
  if(!isdigit(last_char)) {
    switch(tolower(last_char)) {
    case'k': multiplier = 1024; break;
    case'm': multiplier = 1024*1024; break;
    case'g': multiplier = 1024*1024*1024; break;
    default:
      cerr << "Parsing problem decyphering " << name << " as an unsigned integer string\n";
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
 * @brief Convert a numerical string into an pointer value
 * @param[out] val The output variable to set
 * @param[in] sval Input string to parse
 * @retval 0 If input could be parsed
 * @retval EINVAL If input string couldn't be parsed
 * @note This function is **not** commonly used and can be dangerous
 */
int StringToPtr(void **val, string const &sval) {
  kassert(val,"Null val ptr handed to StringToPtr");

  //Pass back to the user
  *val = (void*)strtoull(sval.c_str(), nullptr, 16);

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
 * @brief Split a path into a vector of strings
 */
vector<string> SplitPath(string const &s) {
  vector<string> tokens;
  stringstream ss(s);
  string item;
  while(getline(ss,item,'/')) {
    if(item!="") //For root case (otherwise /root would be "" "root") and ending slash
      tokens.push_back(item);
  }
  return tokens;
}

/**
 * @brief Join a vector to make a path: vv=(a,b,c,d,e), num_items=3, gives /a/b/c
 */
string JoinPath(vector<string> const &vv, int num_items) {

  kassert(num_items>=0, "JoinPath");
  kassert(num_items<=(int)vv.size(), "JoinPath");

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

void ConvertToHexDump(const char *x, ssize_t len, int chars_per_line,
                      int grouping_size,
                      string even_prefix, string even_suffix,
                      string odd_prefix,  string odd_suffix,
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

void ConvertToHexDump(const string s, int chars_per_line,
                      string *hex_part, string *txt_part) {

  ConvertToHexDump(s.c_str(),s.size(), chars_per_line, hex_part, txt_part);
}


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

} // namespace faodel
