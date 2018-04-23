// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

// Boostrap Example
//
// Bootstrap is a way to establish the order in which different components
// are initialized and shutdown. Each component must register itself using
// the RegisterComponent function. This registration has six fields:
//
//  1. The name of the component
//  2. A list of all the components that must come before this one
//  3. A list of any optional components that must come before this one
//  4. The configuration function
//  5. The start function
//  6. The finish function
//

#include <iostream>

#include "common/Common.hh"
#include "common/Bootstrap.hh"

using namespace std;
using namespace faodel;

int global_num_tested=0;

class A {
public:
  A() {
    cout <<"A()\n";
    bootstrap::RegisterComponent("a", {}, {},
                                 [this] (const Configuration &config) { this->increaseCount(); },
                                 [this] () { this->increaseCount(); },
                                 [this] () { this->decreaseCount(); this->decreaseCount(); }
                                 );
  }
  ~A() { cout << "~A()\n"; }

private:
  void increaseCount() { global_num_tested++;}
  void decreaseCount() { global_num_tested--;}

};

class B {
public:
  B() {
    cout <<"B()\n";
    bootstrap::RegisterComponent("b", {"a"}, {},
                                 [this] (const Configuration &config) { this->increaseCount(); },
                                 [this] () { this->increaseCount(); },
                                 [this] () { this->decreaseCount(); this->decreaseCount(); }
                                 );
  }
  ~B() { cout << "~B()\n"; }

private:
  void increaseCount() { global_num_tested++;}
  void decreaseCount() { global_num_tested--;}
};

std::string fn_no_components() {
  return "";
}

//These are the actual instances
B b;
A a;




int main() {

  if(global_num_tested!=0) { cerr <<"Didn't have right pre-init value\n"; return -1; }

  bootstrap::Init(Configuration(""), fn_no_components);
  if(global_num_tested!=2) { cerr <<"Didn't have right post-init value\n"; return -1; }

  bootstrap::Start();
  if(global_num_tested!=4) { cerr <<"Didn't have right post-start value\n"; return -1; }


  bootstrap::Finish();
  if(global_num_tested!=0) { cerr <<"Didn't have right post-final value\n"; return -1; }

  return 0;
}
