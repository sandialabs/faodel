// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef FAODEL_COMMON_LOGGINGITERFACEMACROS_HH
#define FAODEL_COMMON_LOGGINGITERFACEMACROS_HH

//Compile-time hook for wiping out all logging
// Downside: this has conflicts with sbl

#if LOGGING_DISABLED
#define dbg(a)
#define info(a)
#define warn(a)
//#else
//#define dbg(a) dbg(a)
//#define info(a) info(a)
//#define warn(a) warn(a)
#endif

#endif // FAODEL_COMMON_LOGGINGINTERFACEMACROS_HH
