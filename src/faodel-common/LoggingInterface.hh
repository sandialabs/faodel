// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#ifndef FAODEL_COMMON_LOGGINGITERFACE_HH
#define FAODEL_COMMON_LOGGINGITERFACE_HH

#include <string>

#include <faodelConfig.h>



//Forward references
namespace sbl { class logger; }
namespace faodel { class Configuration; }


namespace faodel {

/**
 * @brief A standard logging interface for FAODEL components
 *
 * This interface provides us with a way to simplify how logging is performed
 * by different FAODEL components. Inherit this from your class and specify
 * the name of this component. The owner of this class must then pass in
 * the runtime Configuration when bootstrap inits components.
 */
class LoggingInterface {

public:
  LoggingInterface(std::string component_name);
  LoggingInterface(std::string component_name, std::string subcomponent_name);
  virtual ~LoggingInterface() {}

  //Allow externals to change what we log
  void ConfigureLoggingDebug(bool enable_debug);
  void ConfigureLoggingInfo(bool enable_info);
  void ConfigureLoggingWarn(bool enable_warn);

  void SetLoggingLevel(int log_level);
  static int GetLoggingLevelFromConfiguration(const Configuration &config, const std::string &component_name);

  bool GetDebug() const { return debug_enabled; }
  std::string GetFullName() const { if (subcomponent_name.empty()) return component_name; else return component_name+"."+subcomponent_name;}
  std::string GetComponentName() const { return component_name; }
  std::string GetSubcomponentName() const { return subcomponent_name; }

protected:

  //Only allow component to set its configuration and subcomponent name
  void ConfigureLogging(const Configuration &config);
  void SetSubcomponentName(std::string new_name) { subcomponent_name=new_name; }



#if Faodel_LOGGINGINTERFACE_DISABLED==1
  void dbg(std::string s) const {}
  void info(std::string s) const {}
  void warn(std::string s) {}
  void error(std::string s) const {}
  void fatal(std::string s) const { exit(-1); }
#else
  void dbg(std::string s) const;
  void info(std::string s) const;
  void warn(std::string s);
  void error(std::string s) const;
  void fatal(std::string s) const;
#endif

private:
  std::string component_name;
  std::string subcomponent_name;
  bool debug_enabled;
  bool info_enabled;
  bool warn_enabled;

#if Faodel_LOGGINGINTERFACE_DISABLED==0 && Faodel_LOGGINGINTERFACE_USE_SBL==1
  static sbl::logger *sbl_logger;
#endif


};


} // namespace faodel



#endif // FAODEL_COMMON_LOGGINGITERFACE_HH
