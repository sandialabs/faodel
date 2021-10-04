// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.


#include <array>
#include <vector>
#include <map>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <iomanip>

#include "faodel-common/Configuration.hh"
#include "faodel-common/StringHelpers.hh"

using namespace std;


namespace faodel {
namespace configlog {
namespace internal {

//The AppendRequestedGet captures all the config.Get options that were requested in the
//application so people can learn what options are being queries.

map<string, array<string,2>> config_values;
} //internal
void AppendRequestedGet(const string &field, const string &option_type, const string &default_value) {
  std::array<string,2> vals;
  vals[0]=option_type;
  vals[1]=default_value;
  internal::config_values[field] = vals;
}
std::map<std::string, array<std::string,2>> GetConfigOptions() {
  return internal::config_values;
}
string str() {
  stringstream ss;
  map<string, array<string,2>> vals =  configlog::GetConfigOptions();
  for(auto &name_vals : vals) {
    ss <<name_vals.first <<" "<<name_vals.second[0] <<" " <<name_vals.second[1]<<endl;
  }
  return ss.str();
}

} //configlog
} //faodel



namespace faodel {


/**
 * @brief Parse a user-supplied Configuration
 * @param[in] configuration_string A multi-line string with different config settings in it
 * @param[in] env_variable_for_extra_settings Name of an environment variable that points to a file with additional settings
 * @note While the Environment Variable is set here, the file is NOT LOADED here. You need to call
 *       AppendFromReferences, or have bootstrap::Init load it.
 * @note The default environment variable name is FAODEL_CONFIG
 */
Configuration::Configuration(const string &configuration_string,
                             const string &env_variable_for_extra_settings) {
  node_role = "default";

  //Set the default env var to load here, but do not load it so user
  //can switch or erase it.
  if(!env_variable_for_extra_settings.empty()) {
    Append("config.additional_files.env_name.if_defined", env_variable_for_extra_settings);
  }
  if(!configuration_string.empty()) {
    rc_t rc = Append(configuration_string);
    if(rc!=0) throw std::runtime_error("Configuration's initialization string had errors");
  }
}

Configuration::~Configuration() = default;

/**
 * @brief Parse a multi-line string and append configuration data
 * @param[in] config_str One or more lines of configuration data (separated by \n)
 * @retval 0 Always works
 */
rc_t Configuration::Append(const string &config_str) {
  addConfigToMap(config_str);
  return 0;
}
/**
 * @brief Set a field in the configuration to a string value
 * @param[in] name Name of the field
 * @param[in] val String value to set
 * @retval 0 Always works
 */
rc_t Configuration::Append(const string &name, const string &val) {
  //cout <<"Config Append '"<<name<<"' value '"<<val<<"'\n";
  return Set(name,val);
}

/**
 * @brief Set a field in the configuration if it is not already defined
 * @param[in] name Name of the field
 * @param[in] val String value to set
 * @return 0 Always works
 */
rc_t Configuration::AppendIfUnset(const std::string &name, const std::string &val) {
  if(Contains(name)) return 0;
  return Append(name, val);
}

/**
 * @brief Parse one or more files and load in their configuration data
 * @param[in] file_name One or more ";" separated file names to load
 * @retval 0 Always works
 */
rc_t Configuration::AppendFromFile(const string &file_name) {
  stringstream ssout;

  string segment;
  stringstream ss(file_name);
  while(getline(ss, segment, ';')) {
    string expanded = ExpandPathSafely(segment);
    if (expanded.length() > 0) {
        ifstream src(expanded.c_str());
        ssout << src.rdbuf();
    }
  }
  Append(ssout.str());
  return 0;
}

/**
 * @brief Load additional settings from files, including those defined by
 *        environment variable
 * @retval 0 Always works, unless file pointed to by env variable is not available
 *
 * This function is used to pull in additional config files specified in the
 * configuration file or by environment variables. The intent is to provide an
 * easy way to pass in platform-specific details without having to modify an
 * application. Users can supply multiple options here:
 *
 * - **config.additional_files abc;def;ghi**:
 *   Append the data from files abc, def, and ghi to Configuration
 *
 * - **config.additional_files.env_name ABC;DEF**:
 *   Read file names from environment variables ABC and DEF and append the data
 *   from the files specified by ABC and DEF to Configuration. Exception if these
 *   environment variables are not defined.
 *
 * - **config.additional_files.env_name.if_defined ABC;DEF**:
 *   Do the same as config.additional_files.env_name, but do not throw exception if the
 *   environment variables don't exist.
 *
 * @note The config.additional_files markers are removed from Configuration
 *       once AppendFromReferences() is run in order to avoid endless loops.
 *       However, Bootstrap only runs this function once. Thus, any additional
 *       file references added during this call will **not** be resolved.
 */
rc_t Configuration::AppendFromReferences() {

  //Get the additional list of configs to use
  string additional_filenames;
  string env_name1,env_name2;
  GetString(&additional_filenames, "config.additional_files","");
  GetString(&env_name1,            "config.additional_files.env_name","");
  GetString(&env_name2,            "config.additional_files.env_name.if_defined","");

  //Remove these references so we don't hit them again
  config_map.erase("config.additional_files");
  config_map.erase("config.additional_files.env_name");
  config_map.erase("config.additional_files.env_name.if_defined");


  //Check for environment var telling us of others to load
  if(!env_name1.empty()) {
    char *config_file = getenv(env_name1.c_str());
    if(config_file== nullptr) {
     throw  std::runtime_error("Configuration error: config.additional_files.env set to "+env_name1+
                               " but that environment variable is not define");
    }
    //Add env setting to back of the list
    if(!additional_filenames.empty()) {
      additional_filenames +=";"+string(config_file);
    } else {
      additional_filenames = string(config_file);
    }
  }

  //Check for optional environment var telling us of others to load
  if(!env_name2.empty()) {
    char *config_file = getenv(env_name2.c_str());
    if(config_file!= nullptr) {
      //Add env setting to back of the list
      if(!additional_filenames.empty()) {
        additional_filenames +=";"+string(config_file);
      } else {
        additional_filenames = string(config_file);
      }
    }
  }
  AppendFromFile(additional_filenames);

  return 0;
}


/**
 * @brief Set a field in the configuration to a string value
 * @param[in] name Name of the field
 * @param[in] val String value to set
 * @retval 0 Always works
 */
rc_t Configuration::Set(const string &name, const string &val) {

  string target_name = ToLowercase(name);
  string target_val  = val;

  //See if this is an item appended to our list
  if(target_name.size()>2) {
    string suffix = target_name.substr(target_name.size()-2);
    string prefix = target_name.substr(0, target_name.size()-2);

    if(suffix=="[]") {
      //Multiple instances: One variable name may have multiple instances. Use GetStringVector to access
      target_name = prefix;
      int id = GetStringVector(nullptr, target_name);
      target_name = target_name + "."+std::to_string(id);

    } else if (suffix=="<>") {
      //Multiple values: Find the one variable and then add this to the list of entries
      //TODO: Is this used anymore?
      target_name = prefix;
      auto ii = config_map.find(target_name);
      if (ii != config_map.end()) //Only have to revise if we append
        target_val = ii->second + ";" + val;
    }

  }

  //Store in map
  config_map[target_name]=target_val;
  if(name == "node_role")
    node_role = val;
  return 0;
}

/**
 * @brief Set a field in the configuration to a char array value
 * @param[in] name Name of the field
 * @param[in] val char array value to set
 * @retval 0 Always works
 */
rc_t Configuration::Set(const string &name, const char *val) {
  return Set(name, string(val));
}

/**
 * @brief Set a field in the configuration to an integer value
 * @param[in] name Name of the field
 * @param[in] val Integer value to set
 * @retval 0 Always works
 */
rc_t Configuration::Set(const string &name, int val) {
  return Set(name, std::to_string(val));
}

  /**
 * @brief Set a field in the configuration to an integer value
 * @param[in] name Name of the field
 * @param[in] val Integer value to set
 * @retval 0 Always works
 */
  rc_t Configuration::Set(const string& name, unsigned int val) {
    return Set(name, std::to_string(val));
  }

/**
 * @brief Set a field in the configuration to a pointer value
 * @param[in] name Name of the field
 * @param[in] val Pointer value to set
 * @retval 0 Always works
 */
rc_t Configuration::Set(const string &name, void *val) {
  stringstream ss;
  ss << showbase
     << std::internal
     << setfill('0');
  ss << hex << setw(18) << (uint64_t)val;
  return Set(name, ss.str());
}

/**
 * @brief Set a field in the configuration to a pointer value
 * @param[in] name Name of the field
 * @param[in] val Boolean value to set
 * @retval 0 Always works
 */
rc_t Configuration::Set(const string &name, bool val) {
  return Set(name, (val)?"true":"false");
}

/**
 * @brief Remove an entry from a configuration
 * @param name Name of the variable to remove
 * @retval 0 Always works
 */
rc_t Configuration::Unset(const string &name){
  config_map.erase(name);
  return 0;
}

/**
 * @brief Determine if a particular field is set
 * @param[in] name Keyword in configuration to look for
 * @retval TRUE If a setting is specified
 * @retval FALSE if no setting is specified
 */
bool Configuration::Contains(const std::string &name) const {
  int rc = GetString(nullptr, name);
  return (rc!=ENOENT);
}

int Configuration::findBestMatch(string *val, const string &name, const string &type_label, const string &default_value) const {

  configlog::AppendRequestedGet(name, type_label, default_value);

  string lname=ToLowercase(name);
  vector<string> search_list = { node_role+"."+lname,
                                 "default."+lname,
                                 lname};
  for(auto &s : search_list) {
    auto it=config_map.find(s);
    if(it!=config_map.end()) {
      if(val) *val = it->second;
      return 0;
    }
  }
  if(val) *val=default_value;
  return ENOENT;
}

/**
 * @brief Search configuration data and return string of value, or default if not found
 * @param[out] val Value that was found, or default value
 * @param[in] name Keyword in configuration to look for
 * @param[in] default_value Default string to return if not found
 * @retval 0 If Found value in configuration
 * @retval ENOENT Data wasn't found, result is default value
 */
rc_t Configuration::GetString(string *val, const string &name,
                              const string &default_value) const {

  return findBestMatch(val, name, "string", default_value);
}

/**
 * @brief Search configuration data and return lowercase string of value, or default if not found
 * @param[out] val Lowercase version of value that was found, or default value
 * @param[in] name Keyword in configuration to look for
 * @param[in] default_value Default string to use if not found
 * @retval 0 If Found value in configuration
 * @retval ENOENT Data wasn't found, result is default value
 */
rc_t Configuration::GetLowercaseString(string *val, const string &name, const string &default_value) const {

  string tmp;
  int rc = GetString(&tmp, name, default_value);
  if(val) *val = ToLowercase(tmp);
  return rc;
}

/**
 * @brief Search through configuration and return value if found. Otherwise
          return default value
 * @param[in] name Keyword in configuration to look for
 * @param[out] val Value that was found
 * @param[in] default_value A default value string to return if not found (parsed)
 * @retval 0 If Found value in configuration
 * @retval ENOENT Data wasn't found, using default value
 * @retval EINVAL Data or default value could not be parsed to Int
 */
rc_t Configuration::GetInt(int64_t *val, const string &name,
                           const string &default_value) const {

  std::string tmp;
  int rc = findBestMatch(&tmp, name, "int", default_value);
  int rc2 = StringToInt64(val, tmp);
  if(rc2 != 0) return rc2;
  return rc;

}

/**
 * @brief Search through configuration and return value if found. Otherwise
          return default value
 * @param[out] uval Value that was found
 * @param[in] name Keyword in configuration to look for
 * @param[in] default_value A default value string to return if not found (parsed)
 * @retval 0 If Found value in configuration
 * @retval ENOENT Data wasn't found, using default value
 * @retval EINVAL Data or default value could not be parsed to UInt
 */
rc_t Configuration::GetUInt(uint64_t *uval, const string &name,
                            const string &default_value) const {

  std::string tmp;
  int rc = findBestMatch(&tmp, name, "uint", default_value);

  //Make sure this isn't actually a negative value
  int64_t ival;
  int rc2 = StringToInt64(&ival, tmp);
  if(rc2 != 0) return rc2;
  if (ival < 0) {
    if(uval) *uval = 0;
    return EINVAL;
  }
  //Number is positive, go ahead and do uint64 conversion
  int rc3 = StringToUInt64(uval, tmp);
  if(rc3 != 0) return rc3;
  return rc;

}

/**
 * @brief Search through configuration and return value if found. Otherwise
          return default value
 * @param[out] us_val Value that was found (in microseconds)
 * @param[in] name Keyword in configuration to look for
 * @param[in] default_value A default value string to return if not found (parsed)
 * @retval 0 If Found value in configuration
 * @retval ENOENT Data wasn't found, using default value
 * @retval EINVAL Data or default value could not be parsed to UInt
 */
rc_t Configuration::GetTimeUS(uint64_t *us_val, const string &name,
                            const string &default_value) const {

  std::string tmp;
  int rc = findBestMatch(&tmp, name, "timeUS", default_value);
  int rc2 = StringToTimeUS(us_val, tmp);
  if(rc2 != 0) return rc2;
  return rc;

}


/**
 * @brief Search through configuration and return value if found. Otherwise return default value
 * @param[in] name Keyword in configuration to look for
 * @param[out] val Value that was found
 * @param[in] default_value A default value to use if not found (parsed)
 * @retval 0 If Found value in configuration
 * @retval ENOENT Data wasn't found, using default value
 * @retval EINVAL Data or default value could not be parsed to Bool
 */
rc_t Configuration::GetBool(bool *val, const string &name,
                            const string &default_value) const {

  std::string tmp;
  int rc = findBestMatch(&tmp, name, "bool", default_value);
  int rc2 = StringToBoolean(val, tmp);
  if(rc2 != 0) return rc2;
  return rc;

}

/**
 * @brief Search through configuration and return value if found. Otherwise
          return default value
 * @param[in] name Keyword in configuration to look for
 * @param[out] val Value that was found
 * @param[in] default_value A default value string to return if not found (parsed)
 * @retval 0 If Found value in configuration
 * @retval ENOENT Data wasn't found, using default value
 * @retval EINVAL Data or default value could not be parsed to Int
 * @note This function is **NOT** used by most people.
 */
rc_t Configuration::GetPtr(void **val, const string &name,
                           void *default_value) const {

  std::string tmp;
  int rc = findBestMatch(&tmp, name, "ptr", "");
  if(rc==ENOENT) {
    if(val) *val=default_value;
    return ENOENT;
  }
  int rc2 = StringToPtr(val, tmp);
  if(rc2 != 0) return rc2;
  return rc;
}

/**
 * @brief Retrieve a filename from Config (or the environment)
 * @param[out] fname The resulting file name
 * @param[in]  name The item to look up (name+".file")
 * @param[in]  default_env_var The default environment var to use if none specified
 * @param[in]  default_value The default filename to use if not found
 * @retval 0 If filename was resolved
 * @throws runtime_error if required env var is missing
 *
 * This function attempts to retrieve a filename from the config or
 * the environment. The name supplied to this function will be appended
 * with ".file" to signify this variable is for a file, and the
 * user may supply two other extensions to signify that the data should
 * be pulled from the environemnt variables. Thus, a user may speciy
 * one of three variable names in config when dealing with filenames:
 *
 * - myfile.file xyz:
 *   The user is supplying the filename of myfile
 *   in Configuration. The file is "xyz"
 *
 * - myfile.file.env_name XYZ:  The user is passing the filename
 *   through the environment variable XYZ. Exit if XYZ isn't defined.
 *
 * - myfile.file.env_name.if_defined XYZ: The user wants to check
 *   the environment variable XYZ for the filename. If that variable
 *   doesn't exist, use myfile.file from Configuration
 *
 * Order of processing:
 *   1. name.file
 *   2. name.file.env_name.if_defined
 *   3. name.file.env_name             (can fail)
 *   4. default_env_var
 *   5. default_file
 */
rc_t Configuration::GetFilename(string *fname, const string &name,
                                const string &default_env_var,
                                const string &default_file) const {

  string tmp_fname;
  string ename;
  char *penv;

  //First choice: See if file is explicitly defined. No default value yet.
  rc_t rc = GetString(&tmp_fname, name+".file");
  if(rc==0) {
    string expanded = ExpandPathSafely(tmp_fname);
    if(expanded.length() > 0) {
      if(fname) *fname = expanded;
      return 0;
    }
  }

  //Look for an optional env var
  GetString(&ename, name+".file.env_name.if_defined", "");
  if(!ename.empty()) {
    penv = getenv(ename.c_str());
    if(penv) {
      //Env var exists, use it
      string expanded = ExpandPathSafely(string(penv));
      if (expanded.length() > 0) {
        if(fname) *fname = expanded;
        return 0;
      }
    }
    //Didn't exist. Can continue because it was optional
  }

  //Look for a mandatory env var
  GetString(&ename, name+".file.env_name", "");
  if(!ename.empty()) {
    //They asked to use a mandatory env var
    penv = getenv(ename.c_str());
    if(penv) {
      //Env var exists, use it
      string expanded = ExpandPathSafely(string(penv));
      if (expanded.length() > 0) {
        if(fname) *fname = expanded;
        return 0;
      }
      throw  std::runtime_error("Configuration shell expansion failed for "+name+".file.env_name "+ename);
    }
    throw std::runtime_error("Configuration required "+name+".file.env_name "+ename+" but it was not set\n");
  }

  //See if we should check a default env var
  if(!default_env_var.empty()) {
    penv = getenv(default_env_var.c_str());
    if(penv) {
      string expanded = ExpandPathSafely(string(penv));
      if (expanded.length() > 0) {
        if(fname) *fname = expanded;
        return 0;
      }
    }
    //Not mandatory, so fall through
  }

  //Last try: see if default value set
  if(!default_file.empty()) {
    string expanded = ExpandPathSafely(default_file);
    if(fname) *fname = expanded;
    return 0;
  }

  return ENOENT;
}

/**
 * @brief Search for an name that has multiple instances (ie, name.0, name.1) and return a vector of vals
 * @param vals The vector to append the results
 * @param name The base name of the variable to use. The name is appended with ".#" until no variable is found
 * @retval number_of_items The number of items that were found and sent back
 */
int Configuration::GetStringVector(std::vector<std::string> *vals, const std::string &name) const {

  int i;
  rc_t rc=0;
  for(i=0; rc!=ENOENT; i++) {
    string n, v;
    n = name + "." + std::to_string(i);
    rc = GetString(&v, n);
    if ((rc != ENOENT) && (vals))
      vals->push_back(v);
  }

  return (i-1);
}

/**
 * @brief Pull out all key/values that begin with a particular name
 *
 * @param[out] results A map updated with k/vs for this entity
 * @param[in] component_name The prefix of the component to be found
 * @retval 0
 *
 * @note Result keys have component name removed from them
 * @note Component name are appended with '.' if not provided
 * @note This does NOT do anything with node_role (just absolute names)
 *
 * Sometimes we need a way to pull out a chunk of values for a particular
 * component (eg IOMs). This function will extract all the k/vs that have
 * a name that begins with the supplied component_name. It strips out
 * all the component_names in the returned keys (eg, my.thing1.type would
 * become type if component_name was "my_thing1").
 */
rc_t Configuration::GetComponentSettings(map<string,string> *results, const string &component_name) const {

  //Only work on lowercase and don't split tokens (ie, chop at a '.')
  string lprefix = ToLowercase(component_name);
  if( (lprefix.back() != '.') && (!lprefix.empty()))
    lprefix=lprefix+".";

  for(auto &name_val : config_map) {
    if(StringBeginsWith(name_val.first, lprefix)) {
      string new_name = name_val.first.substr(lprefix.size());
      if(results) {
        (*results)[new_name] = name_val.second;
      }
    }
  }
  return 0;
}

/**
 * @brief Pull out all key/values that begin with a particular name
 *
 * @param[in] component_name The prefix of the component to be found
 * @returns Map of just the k/vs for this component
 *
 * @note Result keys have component name removed from them
 * @note Component name are appended with '.' if not provided
 * @note This does NOT do anything with node_role (just absolute names)
 *
 * Sometimes we need a way to pull out a chunk of values for a particular
 * component (eg IOMs). This function will extract all the k/vs that have
 * a name that begins with the supplied component_name. It strips out
 * all the component_names in the returned keys (eg, my.thing1.type would
 * become type if component_name was "my_thing1").
 */
map<string,string> Configuration::GetComponentSettings(const string &component_name) const {
  map<string,string> results;
  GetComponentSettings(&results, component_name);
  return results;
}

/**
 * @brief Determin if a component's logging settings are enabled.
 * @param[out] dbg_enabled  either component.debug or component.log.debug is true
 * @param[out] info_enabled either component.debug or component.log.info is true
 * @param[out] warn_enabled either component.debug or component.log.warn is true
 * @param[in] component_name Name of component to look up in Configuration
 * @retval 0 Always succeeds
 */
rc_t Configuration::GetComponentLoggingSettings(bool *dbg_enabled, bool *info_enabled, bool *warn_enabled, const string &component_name) const {

  bool tmp_dbg_enabled=false, tmp_info_enabled = false;
  GetBool(&tmp_dbg_enabled, component_name+".debug",     "false");
  GetBool(&tmp_info_enabled, component_name+".info",     "false");

  string default_setting="false"; //Everything defaults to off.

  //If .debug is on, use it for everyone
  if(tmp_dbg_enabled) {
    default_setting="true";
    tmp_info_enabled=true; //Debug triggers info
  }
  //..but allow the log.debug to override
  GetBool(dbg_enabled, component_name+".log.debug", default_setting);

  //2. Next, do info. Force on if debug was on, or info was set to on
  if(tmp_info_enabled) {
    default_setting="true";
  }
  //..but allow log.info to override
  GetBool(info_enabled, component_name+".log.info",  default_setting);

  //3. Last do warn. Turn on if either the debug/info are on, but allow log.warn to override
  GetBool(warn_enabled, component_name+".log.warn",  default_setting);
  return 0;
}

/**
 * @brief Get entire list of configuration settings as a string table for debug
 * @param[out] results A vector of k/v pairs
 * @retval 0 Success
 * @retval EINVAL Did not provide a valid pointer for results
 */
rc_t Configuration::GetAllSettings(vector<pair<string,string>> *results) const {

  if(!results) return EINVAL;
  results->push_back(pair<string,string>("node_role", GetRole()));
  for(auto &it : config_map) {
    results->push_back(pair<string,string>(it.first, it.second));
  }
  return 0;
}

/**
 * @brief Parse a single line and generate a vector of strings, stripping
          out comments (#)
 * @param[in] str Line to parse
 * @returns vector of string tokens
 */
vector<string> Configuration::tokenizeLine(const string &str) {

  vector<string> tokens;
  size_t p0 = 0, p1;
  size_t endpos;
  endpos = str.find_first_of('#');

  while(p0 != endpos) {
    p1 = str.find_first_of(" \t", p0);
    if(p1 != p0) {
      string token = str.substr(p0, p1 - p0);
      tokens.push_back(token);
    }
    p0 = str.find_first_not_of(" \t", p1);
 }
  return tokens;

}

/**
 * @brief Convert a (multi-line) configuration string to the map of config vals
 * @param[in] config Input string to parse
 * @returns number of items found
 */
int Configuration::addConfigToMap(const string &config) {

  stringstream ss(config);
  int items_found=0;
  string item;

  while(getline(ss, item, '\n')) {
    vector<string> res = tokenizeLine(item);
    if(res.size()>1) {

      //always convert the name to lowercase
      string lname = ToLowercase(res[0]);

      //Kind of dumb, but join the string back together with uniform spacing
      stringstream sl;
      for(unsigned int i=1; i<res.size(); i++) {
        items_found++;
        sl << res[i];
        if(i+1<res.size())
          sl <<" ";
      }
      Append(lname, sl.str());
    }
  }

  return items_found;
}

/**
 * @brief Lookup the node's role in the configuration. If none specified, use "default"
 * @retval role The role name for the node
 * @retval "default" If node_role was not set
 */
string Configuration::GetRole() const {
  return node_role;
}

/**
 * @brief Look for this node's default bucket (as string).
          Checks node_role.security_bucket, then security_bucket
 * @param[out] bucket_name - The string version of the default bucket
 * @retval 0 Found the item we were looking for
 * @retval ENOENT Couldn't find the bucket
 */
rc_t Configuration::GetDefaultSecurityBucket(string *bucket_name) const {

  string bucket_string = "default-bucket-name";
  string names[] = {
    node_role+".security_bucket",
    "security_bucket",
    ""
  };

  for(int i=0; !names[i].empty(); i++) {
    map<string,string>::const_iterator jj;
    jj = config_map.find(names[i]);
    if(jj!=config_map.end()) {
      bucket_string = jj->second;
      break;
    }
  }
  if(bucket_name) *bucket_name = bucket_string;
  if(bucket_string.empty()) return ENOENT;
  return 0;
}

/**
 * @brief Look for this node's default bucket (as string). Checks
          node_role.security_bucket, then security_bucket
 * @param[out] bucket -  The numerical version of the default bucket
 * @retval 0 Found the value
 * @retval ENOENT Couldn't find the bucket
 */
rc_t Configuration::GetDefaultSecurityBucket(bucket_t *bucket) const {
  string bucket_name;

  rc_t rc = GetDefaultSecurityBucket(&bucket_name);
  if(rc!=0) return rc;

  if(bucket) *bucket = bucket_t(bucket_name);

  return 0;
}

/**
 * @brief Return a label describing what the default threading model is
 * @param[out] threading_model The value for threading_model
 * retval 0 Found the value
 **/
rc_t Configuration::GetDefaultThreadingModel(string *threading_model) const {
  if(!threading_model) return 0;
  return GetString(threading_model, "threading_model", "default");
}

/**
 * @brief Determine the mutex type to be used in a specific component
 * @param[in] component_name The component name (.mutex_type is appended)
 * @param[in] default_mutex_type The mutex type to use if the entry isn't in the Config
 * retval MutexWrapperTypeID An identifier for the threading model/mutex type
 **/
MutexWrapperTypeID Configuration::GetComponentMutexTypeID(const string &component_name,
                                                          const string &default_mutex_type) const {
  string threading_model;
  string mutex_type;
  GetDefaultThreadingModel(&threading_model);

  if(component_name.empty()) {
    mutex_type = default_mutex_type;
  } else {
    //Look this up to see if there is an override in config
    GetString(&mutex_type, component_name+".mutex_type", default_mutex_type);
  }

  return faodel::GetMutexTypeID(threading_model, mutex_type);
}

/**
 * @brief Generate an actual mutex for a component based on user's specifications
 * @param[in] component_name The item to lookup (.mutex_type is appended)
 * @param[in] default_mutex_type The mutex type to use if the entry isn't in the Config
 * @retval MutexWrapper The mutex for the named component
 */
MutexWrapper * Configuration::GenerateComponentMutex(const string &component_name, const string &default_mutex_type) const {
  MutexWrapperTypeID id = GetComponentMutexTypeID(component_name, default_mutex_type);
  return GenerateMutexByTypeID(id);
}

/**
 * @brief Write debug info into a stream stream
 * @param[in] ss String Stream to append info into
 * @param[in] depth How many more steps in hierarchy to go down (default=0)
 * @param[in] indent How many spaces to put in front of this line (default=0)
 */
void Configuration::sstr(stringstream &ss, int depth, int indent) const {
  if(depth<0) return;

  ss << string(indent,' ') << "[Configuration]" <<endl;
  for(auto &k_v : config_map)
    ss << string(indent+2,' ') << k_v.first << " " << k_v.second << endl;
}



} // namespace kelpie
