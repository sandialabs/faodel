// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#ifndef FAODEL_COMMON_HH
#define FAODEL_COMMON_HH

#include "common/FaodelTypes.hh"
#include "common/Bucket.hh"
#include "common/Configuration.hh"
#include "common/Debug.hh"
#include "common/InfoInterface.hh"
#include "common/LoggingInterface.hh"
#include "common/MutexWrapper.hh"
#include "common/StringHelpers.hh"
#include "common/ResourceURL.hh"
#include "common/NodeID.hh"
#include "common/BackBurner.hh"
#include "common/Bootstrap.hh"
#include "common/BootstrapInterface.hh"

//Exclude Serialization by default, as it has boost-specific templates that
//make the build go longer. (eg, 17s vs 14s to build common on CDU's laptop)
//
//#include "common/SerializationHelpers.hh"

#endif // FAODEL_COMMON_HH
