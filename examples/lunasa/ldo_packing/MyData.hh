// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef FAODEL_EXAMPLES_MYDATA_HH
#define FAODEL_EXAMPLES_MYDATA_HH

#include <string>
#include <vector>

/**
 * @brief Example class that contains different types of data
 * @note Anything more sophisticated than this should use a real serializer
 */

class MyData {

public:

  MyData() : field_length(0), field1(nullptr), field2(nullptr) {}
  MyData(int field_length)
    : field_length(field_length) {

    prop1 = "My big property";
    prop2 = 42.0;
    field1 = new float[field_length];
    field2 = new int[field_length];
    for(int i=0; i<field_length; i++) {
      field1[i] = float(i);
      field2[i] = int(i);
      field3.push_back(double(i));
    }
  }

  ~MyData() {
    if(field1) delete[] field1;
    if(field2) delete[] field2;
  }


  int    field_length; //How many values are in the fields

  //Singular values
  std::string prop1;
  double prop2;

  //Arrays of data
  float *field1;
  int   *field2;
  std::vector<double> field3;

};


#endif //FAODEL_EXAMPLES_MYDATA_HH
