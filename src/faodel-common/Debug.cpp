// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 


#include <iostream>
#include <iomanip>
#include <unistd.h>
#include <stdexcept>
#include <sstream>

#include "faodel-common/Debug.hh"

using namespace std;

namespace faodel {

bool _kassert_dont_die=false;  //For use in testing
bool _kassert_quiet=false;     //For use in testing
int  _kfail_count=0;           //For use in testing
void _kassert(bool true_or_die, std::string message, const char *file, int line) {
  if(true_or_die) return;
  _kfail_count++;
  if(!_kassert_quiet)    cout <<"KAssert: "<<message<<" in "<<file<<":"<<line<<endl;
  if(!_kassert_dont_die) { sleep(1); exit(-1); }
}

void _khalt(std::string message, const char *file, int line) {
  std::cout << std::dec
            << "\033[1;41m  Halt  \033[1;33m["
            << message
            << "]  \033[0m at "
            << file <<":"<<line
            << endl;
  while(1) {}
}


/**
 * @brief Throw an exception with a message when not inside a LoggingInterface
 *
 * @param[in] component The component that is failing
 * @param[in] msg The message to display
 */
void fatal_fn(string component, string msg) {
  stringstream ss;
  ss <<"F "<<component<<" ERROR: "<<msg<<endl;
  throw std::runtime_error(ss.str());
}

} // namespace kelpie
