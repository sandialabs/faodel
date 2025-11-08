// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef FAODEL_EXAMPLES_MYHELPERS_HH
#define FAODEL_EXAMPLES_MYHELPERS_HH

#include <vector>
#include <string>

/**
 * @brief Our custom type specification. User is free to encode any layout they want
 */
enum MyTypes : uint8_t {
  STRING = 1,
  INT    = 2,
  FLOAT  = 3,
  DOUBLE = 4
};


/**
 * @brief Helper class to make it easier to gather up variables for packing
 *
 * This is an example helper class that shows how to use templates to gather
 * up variable information so you can pack it easier.
 */
struct MyVariableGatherer {
  std::vector<std::string> names;
  std::vector<const void *> ptrs;
  std::vector<size_t> bytes;
  std::vector<uint8_t> types;

  template <typename T>
  void append(const std::string &name, const T *ptr, int num) {
    names.push_back(name);
    ptrs.push_back((void *) ptr);
    bytes.push_back( sizeof(T)*num);
    uint8_t type=0;
    if(      std::is_same<T,int>::value)    type=MyTypes::INT;
    else if (std::is_same<T,float>::value)  type=MyTypes::FLOAT;
    else if (std::is_same<T,double>::value) type=MyTypes::DOUBLE;
    types.push_back(type);
  }

  //Make a special handler for strings
  void append(const std::string &name, const std::string *ptr) {
    names.push_back(name);
    ptrs.push_back((void *) ptr->c_str());
    bytes.push_back(ptr->length());
    types.push_back(MyTypes::STRING);
  }

};

class MyVariableAccess {
public:
  MyVariableAccess(lunasa::DataObjectPacker *dop) : dop(dop) {}

  std::string ExpectString(const std::string name) {
    char *ptr;
    size_t bytes;
    uint8_t type;
    int rc = dop->GetVarPointer(name,(void **)&ptr, &bytes, &type);
    if((rc!=0) || (type!=MyTypes::STRING))
      throw std::runtime_error("Could not retrieve variable named "+name);
    return std::string(ptr, bytes);
  }

  template <typename T>
  T * ExpectArray(const std::string name, size_t *num_words) {
    T *ptr;
    size_t bytes;
    uint8_t type;
    int rc = dop->GetVarPointer(name, (void **)&ptr, &bytes, &type);

    size_t tmp_num_words=0;
    if( ((std::is_same<T,int>::value)    && (type==MyTypes::INT))    ||
        ((std::is_same<T,float>::value)  && (type==MyTypes::FLOAT))  ||
        ((std::is_same<T,double>::value) && (type==MyTypes::DOUBLE))    ){
      tmp_num_words = bytes/sizeof(T);
    }

    if ( (rc)  || (!tmp_num_words) ){
      ptr = nullptr;
    }
    if(num_words) *num_words = tmp_num_words;
    return ptr;
  }

private:
  lunasa::DataObjectPacker *dop;
};

#endif //FAODEL_EXAMPLES_MYHELPERS_HH
