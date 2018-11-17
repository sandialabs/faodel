// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#ifndef FAODEL_COMMON_CONFIGURATION_HH
#define FAODEL_COMMON_CONFIGURATION_HH

#include <string>
#include <vector>
#include <map>

#include "common/FaodelTypes.hh"
#include "common/Bucket.hh"
#include "common/InfoInterface.hh"
#include "common/MutexWrapper.hh"


namespace faodel {

namespace configlog {
//Retrieve a log of all the Configuration params our application has requested
std::map<std::string, std::array<std::string,2>> GetConfigOptions();
std::string str();
}

/**
 * @brief Provides an object for storing configuration information used by different components
 *
 * Configuration is a general class that is used to pass configuration
 * information to different components in the faodel. It stores
 * all information as strings in a key/value map. Users typically pass
 * a multiline block of text as a string into Configuration to define
 * common settings, and then append the Configuration with a few
 * machine-specific settings. Some fundamental details:
 *
 * - **Lowercase Names**: Each variable stored in Configuration
 *   (ie the key) is converted to lowercase.
 *
 * - **Appends Overwrite**: New additions to the Configuration overwrite
 *   previous values. See Multiple Items on Multiple Lines below for
 *   info on how to modify a list of items.
 *
 * - **Numerical Modifiers**: Configuration supports basic numerical
 *   modifiers (k=1024, m=1024*1024).
 *
 * - **Node Role**: Often a node in a deployment serves a particular
 *   function (eg, dht_server, client, io_handler). Most faodel
 *   applications specify different settings for each role (eg,
 *   "dht_server.debug true", "client.debug false"). When a node boots,
 *   appending the "node_role" variable to the configuration allows
 *   the node to pick settings that were intended for it.
 *
 * - **Default Role**: If no default node_role is supplied, "default"
 *   will be used.
 *
 * - **Roles and Reading Variables**: When a user reads a variable,
 *   Configuration automatically tries customizing the name. eg, if
 *   the node_role is "server" and a request for variable "debug" is
 *   made, Configuration will check "server.debug", "default.debug",
 *   and "debug", and pick the first result that is present.
 *
 * - **Multiple Items on Single Line**: Some options allow you to
 *   specify multiple items on line (eg filenames). Just provide a
 *   list separated by ";"
 *
 * - **Multiple Items on Multiple Lines**: Sometimes it is more legible
 *   to specify a list of items using multiple lines. Appending "<>"
 *   to the name indicates that the entry should be appended to a list
 *   of items instead of replacing the value. All lists of values are
 *   returned as a string with ";" delimiters.
 *
 * - **Appending from Other Sources**: Additional configuration info can be
 *   retrieved by specifying environment references. These references
 *   are specified by "config.additional_files" or
 *   "config.additional_files.env_name". See AppendFromReferences for
 *   details.
 *
 */
class Configuration : public InfoInterface {

public:
  Configuration();
  Configuration(const std::string &config_str);
  ~Configuration();

  //Pass in additional configuration data
  rc_t Append(const std::string &config_str);
  rc_t Append(const std::string &name, const std::string &val);
  rc_t AppendFromFile(const std::string &file_name);
  rc_t AppendFromReferences();

  //Extras: update the config after it's been generated
  rc_t Set(const std::string &name, const std::string &val);
  rc_t Set(const std::string &name, const char *val);
  rc_t Set(const std::string &name, int val);
  rc_t Set(const std::string &name, bool val);
  rc_t Set(const std::string &name, void *val);
  rc_t Set(const std::string& name, unsigned int val);

  //Get values out of the config. Returns 0 if ok, -1 if not found
  rc_t GetString(std::string *val, const std::string &name, const std::string &default_value="") const;
  rc_t GetLowercaseString(std::string *val, const std::string &name, const std::string &default_value="") const;
  rc_t GetInt(int64_t *val, const std::string &name, const std::string &default_value="0") const;
  rc_t GetUInt(uint64_t *uval, const std::string &name, const std::string &default_value="0") const;
  rc_t GetBool(bool *val, const std::string &name, const std::string &default_value="") const;
  rc_t GetPtr(void **val, const std::string &name, void *default_value=nullptr) const;
  rc_t GetFilename(std::string *fname, const std::string &name, const std::string &default_value) const;

  //Get subset of configuration info for a particular component
  rc_t GetComponentSettings(std::map<std::string,std::string> *results, const std::string &component_name) const;
  std::map<std::string,std::string> GetComponentSettings(const std::string &component_name) const;

  //Get all entries
  rc_t GetAllSettings(std::vector<std::pair<std::string,std::string>> *results) const;

  //Get our defined role
  std::string GetRole() const;

  //Lookup the default bucket for this node
  rc_t GetDefaultSecurityBucket(std::string *bucket_name) const;
  rc_t GetDefaultSecurityBucket(bucket_t *bucket) const;

  //Generate a mutex for a component
  rc_t GetDefaultThreadingModel(std::string *threading_model) const;
  faodel::MutexWrapperTypeID GetComponentMutexTypeID(std::string component_name="", std::string default_mutex_type="default") const;
  faodel::MutexWrapper * GenerateComponentMutex(std::string component_name="", std::string default_mutex_type="default") const;

  //InfoInterface function
  void sstr(std::stringstream &ss, int depth=0, int indent=0) const;

private:
  std::string node_role;

  std::vector<std::string> tokenizeLine(const std::string &str); //Parse a line
  int addConfigToMap(std::string config); //Parse all the lines in a string

  std::map<std::string, std::string> config_map; //Holds all the config k/vs

};

} // namespace faodel

#endif // FAODEL_COMMON_CONFIGURATION_HH
