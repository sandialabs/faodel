// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#ifndef FAODEL_COMMON_DEBUG_HH
#define FAODEL_COMMON_DEBUG_HH

#include <iostream>
#include <string>


namespace faodel {

void _kassert(bool true_or_die, std::string message, const char *file, int line);
void _khalt(std::string message, const char *file, int line);


// Debug: make halt spots easier to identify
#define KHALT(msg) { faodel::_khalt(msg, __FILE__, __LINE__); }
#define KFAIL(rc)  { printf("Fail at %s line %d\n",__FILE__, __LINE__); exit(-1); }
#define KWARN(a)   { static bool _warned=false; if(!_warned) { std::cout <<"WARNING: "<<(a)<<std::endl; _warned=true; }}
#define KTODO(a)   { std::cout <<"TODO hit at "<<__FILE__<<" line "<<__LINE__<<": "<<(a)<<std::endl; exit(-1); }
#define KDELAY()   { sleep(1); }



#define kassert(a,msg) { faodel::_kassert(a, msg, __FILE__, __LINE__); }


void fatal_fn(std::string component, std::string msg);

} // namespace faodel

#endif // FAODEL_COMMON_DEBUG_HH
