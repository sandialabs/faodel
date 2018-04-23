// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include <iostream>
#include <assert.h>

#include "common/Common.hh"
#include "lunasa/Lunasa.hh"

using namespace std;
using namespace faodel;

int global_num_tested=0;

class A {
public:
  A(){
    cout <<"A()\n";
    bootstrap::RegisterComponent("a", {}, {},
                                 [this] (const Configuration &config) { this->increaseCount(); },
                                 [this] () { this->increaseCount(); },
                                 [this] () { this->decreaseCount(); this->decreaseCount(); }
                                 );
  }
  ~A(){ cout << "~A()\n"; }

private:
  void increaseCount(){ global_num_tested++;}
  void decreaseCount(){ global_num_tested--;}

};

class B {
public:
  B(){
    cout <<"B()\n";
    bootstrap::RegisterComponent("b", {"a"}, {},
                                 [this] (const Configuration &config) { this->increaseCount(); },
                                 [this] () { this->increaseCount(); },
                                 [this] () { this->decreaseCount(); this->decreaseCount(); }
                                 );
  }
  ~B(){ cout << "~B()\n"; }

private:
  void increaseCount(){ global_num_tested++;}
  void decreaseCount(){ global_num_tested--;}
};

//These are the actual instances
//B b;
//A a;


//void do_xx();

int main(){

  cout <<"===============static bootstrap start==================\n";

  //do_xx();
  bootstrap::Init(Configuration("bootstrap.debug true"), lunasa::bootstrap);

  bootstrap::Start();

  lunasa::Lunasa lu;
  stringstream ss;
  //cout << lu.str(10);
  //cout << ss.str();

  {
    cout <<"TST: Start done\n";
    //cout << lu.str(10);
    lunasa::DataObject ldo = lunasa::Alloc(0, (1024*sizeof(int)), 1992);

    cout <<"TST: alloc done\n";

    //cout << lu.str(10);

    cout <<"TST: DumpDone\n";

    int *x = static_cast<int *>(ldo.dataPtr());
    assert(x!=nullptr);
    cout <<"TST: cast\n";
    for(int i = 0; i<1; i++){
      cout <<"TST: Writing "<<i<<endl;
      x[i] = i;
    }
    cout <<"TST: Work done\n";
  }


  cout <<"static bootstrap stop\n";
  return 0;
}
