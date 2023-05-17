// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef FAODEL_COMMON_DEBUG_HH
#define FAODEL_COMMON_DEBUG_HH

#include <iostream>
#include <string>

#include "faodelConfig.h"


// In some debugging scenarios it helps to have some alternate ways of
// dealing with assertions. The motivation here is:
//
// cassert (default):  just handle with standard assert macro (drops the message)
// debugHalt:          Dump the message and halt. Useful for whookie postmortem
// debugExit:          Dump the message and exit.
// debugWarn:          Dump the message, but continue on. See what else breaks.
#if Faodel_ASSERT_METHOD_NONE
#define F_ASSERT(a,msg) {}
#elif (Faodel_ASSERT_METHOD_DEBUG_WARN || Faodel_ASSERT_METHOD_DEBUG_HALT || Faodel_ASSERT_METHOD_DEBUG_EXIT)
#define F_ASSERT(a,msg) { faodel::_f_assert(a, msg, __FILE__, __LINE__); }
#else
// default to using plain assert
#include <cassert>
#define F_ASSERT(a,msg) { assert(a); }
#endif



namespace faodel {

void _f_assert(bool true_or_die, const std::string &message, const char *file, int line);
void _f_halt(const std::string &message, const char *file, int line);

const std::string TXT_RED = "\033[1;31m";
const std::string TXT_WARN = "\033[1;97;44m";
const std::string TXT_NORMAL = "\033[0m";



// Debug: make halt spots easier to identify
#define F_HALT(msg) { faodel::_f_halt(msg, __FILE__, __LINE__); }
#define F_FAIL(rc)  { printf("Fail at %s line %d\n",__FILE__, __LINE__); exit(-1); }
#define F_WARN(a)   { static bool _warned=false; if(!_warned) { std::cout << faodel::TXT_WARN<<"WARNING:"<< faodel::TXT_NORMAL <<" "<<(a)<<std::endl; _warned=true; }}
#define F_TODO(a)   { std::cout <<"TODO hit at "<<__FILE__<<" line "<<__LINE__<<": "<<(a)<<std::endl; exit(-1); }
#define F_DELAY()   { sleep(1); }




void fatal_fn(const std::string &component, const std::string &msg);

} // namespace faodel

#endif // FAODEL_COMMON_DEBUG_HH
