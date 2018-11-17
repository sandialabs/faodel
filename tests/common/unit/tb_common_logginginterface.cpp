// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

// Note: This is only a sanity check for debugging. It will not catch any 
//       errors. It's here to make sure filters for logs work the way
//       we expect them.

#include <atomic>
#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
#include "gtest/gtest.h"


#include "faodel-common/Common.hh"


using namespace std;
using namespace faodel;




class Base 
  : public LoggingInterface {

public:
  
  Base(Configuration config, string child_name) 
    : LoggingInterface("base", child_name) {
    ConfigureLogging(config);
  }
  void dump(string msg){
    dbg("Debug message "+msg);
    info("Info message "+msg);
    warn("Warn message "+msg);
    error("Error message "+msg);   
  }

};

class Child
  : public Base {

public:
  Child(Configuration config, string name)
    : Base(config, name) {
  }
};



class FaodelLoggingInterface : public testing::Test {
protected:
  void SetUp() override {
    conf_none  = Configuration("mything.debug     false");
    conf_debug = Configuration("mything.log.debug true");
    conf_info  = Configuration("mything.log.info  true");
    conf_warn  = Configuration("mything.log.warn  true");
  }
  virtual void TearDown() { 
  }
  
  Configuration conf_none;
  Configuration conf_debug;
  Configuration conf_info;
  Configuration conf_warn;
  Configuration conf_error;
};

TEST_F(FaodelLoggingInterface, baseNone) {
  Base b_none(conf_none, "none");
  Base b_debug(conf_none, "debug");
  Base b_info(conf_none, "info");
  Base b_warn(conf_none, "warn");
  Base b_error(conf_none, "error");
  b_none.dump("none");
  b_debug.dump("debug");
  b_info.dump("info");
  b_warn.dump("warn");
  b_error.dump("error");
}


TEST_F(FaodelLoggingInterface, childNone) {
  Child b_none(conf_none, "Child-none");
  Child b_debug(conf_none, "Child-debug");
  Child b_info(conf_none, "Child-info");
  Child b_warn(conf_none, "Child-warn");
  Child b_error(conf_none, "Child-error");
  b_none.dump("none");
  b_debug.dump("debug");
  b_info.dump("info");
  b_warn.dump("warn");
  b_error.dump("error");
}


