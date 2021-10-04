// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef FAODEL_MYCONTAINER_HH
#define FAODEL_MYCONTAINER_HH

#include <vector>
#include <string>

// In this example we have a MyContainer class that has a vector of MyItem
// items. To serialize this to a string, we just add a serialize template
// to both both.

struct MyItem {
  MyItem() = default;
  MyItem(const std::string &item_name, double val) : item_name(item_name), x(val) {}

  std::string item_name;
  double x;

  std::string str(){
    return "Name: '"+item_name+"' Value: "+std::to_string(x);
  }
  //Serialization hook
  template <typename Archive>
  void serialize(Archive &ar, const unsigned int version){
    ar & item_name;
    ar & x;
  }
};


class MyContainer {
public:
  MyContainer() = default;
  MyContainer(const std::string &name) : name(name) {}

  void append(const std::string &item_name, double val) {
    items.emplace_back(MyItem(item_name, val));
  }
  void dump(){
    std::cout <<"Container: "<<name <<std::endl;
    for(auto &item : items)
      std::cout<<"  "<<item.str()<<std::endl;
  }

  //Serialization hook
  template <typename Archive>
  void serialize(Archive &ar, const unsigned int version){
    ar & name;
    ar & items;
  }

private:
  std::string name;
  std::vector<MyItem> items;

};


#endif //FAODEL_MYCONTAINER_HH
