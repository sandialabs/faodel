// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include <iostream>
#include <sstream>
#include <stdexcept>

#include "common/Configuration.hh"
#include "common/LoggingInterface.hh"


#if FAODEL_LOGGINGINTERFACE_DISABLED==0 && FAODEL_LOGGINGINTERFACE_USE_SBL==1
#include <sbl/sbl_logger.hpp>
#endif


#if FAODEL_LOGGINGINTERFACE_DISABLED==1

#define LI_LOG_DEBUG(s)
#define LI_LOG_INFO(s)
#define LI_LOG_WARN(s)
#define LI_LOG_ERROR(s)
#define LI_LOG_FATAL(s)

#else

#if FAODEL_LOGGINGINTERFACE_USE_SBL==1

#include <sbl/sbl_logger.hpp>

sbl::logger *faodel::LoggingInterface::sbl_logger=nullptr;

#define LI_LOG_DEBUG(s)  SBL_LOG_STREAM(sbl_logger->debug_source(), component_name) << s;
#define LI_LOG_INFO(s)   SBL_LOG_STREAM(sbl_logger->info_source(), component_name) << s;
#define LI_LOG_WARN(s)   SBL_LOG_STREAM(sbl_logger->warning_source(), component_name) << s;
#define LI_LOG_ERROR(s)  SBL_LOG_STREAM(sbl_logger->error_source(), component_name) << s;
#define LI_LOG_FATAL(s)  SBL_LOG_STREAM(sbl_logger->fatal_source(), component_name) << s;

#else

#define LI_LOG_DEBUG(s)  std::cout << "\033[1;31mD " << component_name << subcomponent_name << ":\033[0m " << (s) << std::endl;
#define LI_LOG_INFO(s)   std::cout << "\033[1;31mI " << component_name << subcomponent_name << ":\033[0m " << (s) << std::endl;
#define LI_LOG_WARN(s)   std::cout << "\033[1;31mW " << component_name << subcomponent_name << ":\033[0m " << (s) << std::endl;
#define LI_LOG_ERROR(s)  std::cerr << "E " << component_name << subcomponent_name << ": " << (s) << std::endl;
#define LI_LOG_FATAL(s)  std::cerr << ss.str() << std::endl;

#endif

#endif


using namespace std;

namespace faodel {


LoggingInterface::LoggingInterface(string component_name)
  : component_name(component_name),
    subcomponent_name(""),
    debug_enabled(false),
    info_enabled(false),
    warn_enabled(true) {

#if FAODEL_LOGGINGINTERFACE_DISABLED==0 && FAODEL_LOGGINGINTERFACE_USE_SBL==1
  // Create a default logger in case ConfigureLogging isn't called.
  sbl_logger = new sbl::logger(sbl::severity_level::debug);
#endif
}


LoggingInterface::LoggingInterface(string component_name, string subcomponent_name)
  : component_name(std::move(component_name)),
    subcomponent_name(std::move(subcomponent_name)),
    debug_enabled(false),
    info_enabled(false),
    warn_enabled(true) {

#if FAODEL_LOGGINGINTERFACE_DISABLED==0 && FAODEL_LOGGINGINTERFACE_USE_SBL==1
  // Create a default logger in case ConfigureLogging isn't called.
  sbl_logger = new sbl::logger(sbl::severity_level::debug);
#endif
}

void LoggingInterface::ConfigureLogging(const Configuration &config) {

  //Usually we want to be explicit about all our names, like x.log.info.
  //However, we also want to be able to be lazy and just tag a
  //component as being in debug mode.

  //Allow user to do "component.debug" instead of "component.log.debug"
  config.GetBool(&debug_enabled, component_name+".debug",     "false");
  string default_setting=(debug_enabled) ? "true" : "false";

  //But still trust log.debug as an override.
  config.GetBool(&debug_enabled, component_name+".log.debug", default_setting);
  config.GetBool(&info_enabled,  component_name+".log.info",  default_setting);
  config.GetBool(&warn_enabled,  component_name+".log.warn",  default_setting);

#if FAODEL_LOGGINGINTERFACE_DISABLED==0 && FAODEL_LOGGINGINTERFACE_USE_SBL==1
  string logfile;
  if (0 == config.GetLowercaseString(&logfile, component_name+".log.filename")) {
    // They want to log to a file, so delete the default logger and create a new one.
    delete sbl_logger;
    sbl_logger = new sbl::logger(logfile, sbl::severity_level::debug);
  }
#endif
}

void LoggingInterface::ConfigureLoggingDebug(bool enable_debug) {
  debug_enabled=enable_debug;
}
void LoggingInterface::ConfigureLoggingInfo(bool enable_info) {
  info_enabled=enable_info;
}
void LoggingInterface::ConfigureLoggingWarn(bool enable_warn) {
  warn_enabled=enable_warn;
}


//TODO: Put some compile-time stuff to route these to SBL

#if FAODEL_LOGGINGINTERFACE_DISABLED==0
void LoggingInterface::dbg(string s) const {
  if(debug_enabled) {
    LI_LOG_DEBUG(s);
  }
}
void LoggingInterface::info(string s) const {
  if(info_enabled) {
    LI_LOG_INFO(s);
  }
}
void LoggingInterface::warn(string s) {
  if(warn_enabled) {
    LI_LOG_WARN(s);
  }
}
void LoggingInterface::error(string s) const {
  LI_LOG_ERROR(s);
}
void LoggingInterface::fatal(string s) const {
  stringstream ss;
  ss<<"F "<<component_name <<": "<<s;
  LI_LOG_FATAL(s);
  throw std::runtime_error(ss.str());
}

#endif


} // namespace faodel
