// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#ifndef FAODEL_COMMON_HH
#define FAODEL_COMMON_HH

#include "faodel-common/FaodelTypes.hh"
#include "faodel-common/Bucket.hh"
#include "faodel-common/Configuration.hh"
#include "faodel-common/Debug.hh"
#include "faodel-common/InfoInterface.hh"
#include "faodel-common/LoggingInterface.hh"
#include "faodel-common/MutexWrapper.hh"
#include "faodel-common/StringHelpers.hh"
#include "faodel-common/QuickHTML.hh"
#include "faodel-common/ReplyStream.hh"
#include "faodel-common/ResourceURL.hh"
#include "faodel-common/NodeID.hh"
#include "faodel-common/DirectoryInfo.hh"
#include "faodel-common/Bootstrap.hh"
#include "faodel-common/BootstrapInterface.hh"

//Exclude Serialization by default, as it has boost-specific templates that
//make the build go longer. (eg, 17s vs 14s to build common on CDU's laptop)
//
//#include "faodel-common/SerializationHelpers.hh"


#endif // FAODEL_COMMON_HH
