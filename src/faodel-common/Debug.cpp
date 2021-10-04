// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.


#include <iostream>
#include <iomanip>
#include <unistd.h>
#include <stdexcept>
#include <sstream>

#include "faodel-common/Debug.hh"

using namespace std;

namespace faodel {


void _f_assert(bool true_or_die, const std::string &message, const char *file, int line) {

  if(true_or_die) return;

  static int  _f_fail_count=0;
  _f_fail_count++;

  cout <<TXT_RED<<"Faodel Assert #("<<_f_fail_count<<"): "<<TXT_NORMAL<<message<<" in "<<file<<":"<<line<<endl;
  if(Faodel_ASSERT_METHOD_DEBUG_WARN) return;
  if(Faodel_ASSERT_METHOD_DEBUG_HALT) _f_halt("Assertion Halt", file, line);
  sleep(1);
  exit(-1);
}

void _f_halt(const std::string &message, const char *file, int line) {
  std::cout << std::dec
            << "\033[1;41m  Halt  \033[1;33m["
            << message
            << "]  \033[0m at "
            << file <<":"<<line
            << endl;
  while(1) { sleep(10); }
}


/**
 * @brief Throw an exception with a message when not inside a LoggingInterface
 *
 * @param[in] component The component that is failing
 * @param[in] msg The message to display
 */
void fatal_fn(const string &component, const string &msg) {
  stringstream ss;
  ss <<"F "<<component<<" ERROR: "<<msg<<endl;
  throw std::runtime_error(ss.str());
}

} // namespace kelpie
