// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <sstream>
#include <stdexcept>

#include "faodel-common/FaodelTypes.hh"
#include "faodel-common/MutexWrapper.hh"
#include "faodel-common/StringHelpers.hh"
#include "faodel-common/LoggingInterface.hh"
#include "faodel-common/ResourceURL.hh"
#include "faodel-common/StringHelpers.hh"
#include "whookie/Server.hh"

#include "kelpie/ioms/IomRegistry.hh"

#include "kelpie/ioms/IomPosixIndividualObjects.hh"
#ifdef FAODEL_HAVE_LEVELDB
#include "kelpie/ioms/IomLevelDB.hh"
#endif
#ifdef FAODEL_HAVE_HDF5
#include "kelpie/ioms/IomHDF5.hh"
#endif
#ifdef FAODEL_HAVE_CASSANDRA
#include "kelpie/ioms/IomCassandra.hh"
#endif

using namespace std;

namespace kelpie {
namespace internal {

IomRegistry::IomRegistry()
  : LoggingInterface("kelpie.iom_registry"),
    finalized(false) {
  mutex = faodel::GenerateMutex();
}

IomRegistry::~IomRegistry() {
  if(mutex) delete mutex;
}

/**
 * @brief Create a new IOM instance based on settings
 * @param type The IOM driver type to use
 * @param name The name of the IOM instance
 * @param settings Settings to pass into the IOM
 * @throw runtime_error if name already used, driver is not known, or race problems detected
 */
void IomRegistry::RegisterIom(string type, string name, const map<string,string> &settings) {

  faodel::ToLowercaseInPlace(type);

  dbg("Register iom "+name+" type "+type+ "("+to_string(settings.size())+" settings)");

  //Don't let user register an iom multiple times (could have config mismatches)
  IomBase *iom = Find(name);
  if(iom!=nullptr) {
    throw std::runtime_error("Attempted to register Iom '"+name+"', which already exists");
  }

  //Make sure ctor exists
  auto name_fn = iom_ctors.find(type);
  if(name_fn == iom_ctors.end()) {
    throw std::runtime_error("Driver '"+type+"' has not been registered for Iom '"+name+"'");
  }

  //Construct the iom 
  iom = name_fn->second(name, settings);
  if(iom==nullptr){
    throw std::runtime_error("Driver creation problem for Iom '"+name+"' with driver '"+type+"'"); 
  }
  iom->SetLoggingLevel(default_logging_level);

  //Store it in the list
  iom_hash_t hid = faodel::hash32(name);
  if(!finalized) {
    ioms_by_hash_pre[hid] = iom;
  } else {
    mutex->Lock();
    //Recheck in case it came in since we checked at the top of this function
    auto rhash_rptr = ioms_by_hash_post.find(hid);
    if(rhash_rptr!=ioms_by_hash_post.end()){
      mutex->Unlock();
      throw std::runtime_error("IOM Registration race detected for '"+name+"'");
    }
    ioms_by_hash_post[hid] = iom;
    mutex->Unlock();
  }
  
}

/**
 * @brief Register an IOM based on settings encoded in a resource url
 * @param url The resource url that contains iom info in its options
 * @retval 0 IOM was successfully registered
 * @retval -1 IOM could not be registered
 *
 * This is intended to be used by tools like the cli that allow users to
 * define resources that are later instantiated. When the cli tool launches a
 * kelpie server, the server retrieves a dirman entry that contains a resource
 * url for the server. That url may include all of the info the server needs
 * to setup the iom. A valid iom inside a url will have the following optins:
 *
 * iom_name : The name of the iom
 * iom_type : What kind of iom driver to use. eg PosixIndividualObjects
 *
 * Additional driver-specific options may include:
 * path : The path PIO should use to write objects
 */
int IomRegistry::RegisterIomFromURL(const faodel::ResourceURL &url) {

  //Convert all options
  auto options_vector = url.GetOptions();
  map<string,string> options_map;
  string iom_name;
  string iom_type;
  for(auto &k_v : options_vector) {
    string k = k_v.first;
    faodel::ToLowercaseInPlace(k);
    if(k=="iom") {               iom_name = k_v.second;
    } else if (k=="iom_type") {  iom_type = k_v.second;
    } else if (faodel::StringBeginsWith(k, "iom_")) {
      k.erase(0,4); //Remove prefix to be compatible with default iom syntax
      if(!k.empty())             options_map[k] = k_v.second;
    }
  }
  if(iom_name.empty() || iom_type.empty()){
    dbg("Error: attempted register iom from url, but didn't have iom_name/iom_type defined");
    return -1;
  }
  try {
    RegisterIom(iom_type, iom_name, options_map);
  } catch(exception &e) {
    dbg("Error: could not register iom due to '"+string(e.what())+"' Note: all settings must have 'iom_' prefix, which is stripped off during registration");
    return -1;
  }

  return 0;
}


/**
 * @brief Register a new iom driver with kelpie
 * @param[in] type The name of this driver
 * @param[in] constructor_function A constructor for building an instance of the driver
 * @param[in] valid_settings_function A function for validating params
 */
void IomRegistry::RegisterIomConstructor(string type, fn_IomConstructor_t constructor_function, fn_IomGetValidSetting_t valid_settings_function) {

  dbg("Registering iom driver for type "+type);

  faodel::ToLowercaseInPlace(type);
  if(finalized){
    throw std::runtime_error("Attempted to register IomConstructor after started");
  }
  
  auto name_fn = iom_ctors.find(type);
  if(name_fn != iom_ctors.end()){
    F_WARN("Overwriting iom constructor "+type);
  }
  iom_ctors[type] = constructor_function;
  iom_valid_setting_fns[type] = valid_settings_function;
}


/**
 * @brief Initialize the registry with a config and register default drivers
 * @param config
 */
void IomRegistry::init(const faodel::Configuration &config) {

  ConfigureLogging(config); //Set registry's logging level
  default_logging_level = faodel::LoggingInterface::GetLoggingLevelFromConfiguration(config,"kelpie.iom"); //For setting ioms


  //Register the drivers

  //Driver: Posix Individual Objects
  fn_IomConstructor_t fn_pio = [] (string name, const map<string,string> &settings) -> IomBase * {
      return new IomPosixIndividualObjects(name, settings);
  };
  RegisterIomConstructor("posixindividualobjects", fn_pio, IomPosixIndividualObjects::ValidSettingNamesAndDescriptions);


  //
  // Insert other built-in drivers here
  //
#ifdef FAODEL_HAVE_LEVELDB
  fn_IomConstructor_t fn_ldb = [] (string name, const map< string, string > &settings) -> IomBase * {
				 return new IomLevelDB( name, settings );
			       };
  RegisterIomConstructor( "leveldb", fn_ldb, IomLevelDB::ValidSettingNamesAndDescriptions );
#endif

#ifdef FAODEL_HAVE_HDF5
  fn_IomConstructor_t fn_hdf5 = [] (string name, const map< string, string > &settings) -> IomBase * {
				  return new IomHDF5( name, settings );
			       };
  RegisterIomConstructor( "hdf5", fn_hdf5, IomHDF5::ValidSettingNamesAndDescriptions );
#endif

#ifdef FAODEL_HAVE_CASSANDRA
  fn_IomConstructor_t fn_cassandra = [] (string name, const map< string, string > &settings) -> IomBase * {
				       return new IomCassandra( name, settings );
			       };
  RegisterIomConstructor( "cassandra", fn_cassandra, IomCassandra::ValidSettingNamesAndDescriptions );
#endif

  
  //Get the list of Ioms this Configuration wants to use
  string s,role;
  config.GetString(&s, "kelpie.ioms");
  role = config.GetRole();

  
  if(!s.empty()) {
    dbg("Registering "+s);
    vector<string> names = faodel::Split(s,';',true);
    for(auto &name : names) {

      //Get all settings for this iom. Do hierarchy of default, kelpie.iom.name, then role.kelpie.iom.name
      map<string, string> settings;
      config.GetComponentSettings(&settings, "default.kelpie.iom");
      config.GetComponentSettings(&settings, "kelpie.iom."+name);
      config.GetComponentSettings(&settings, role+".kelpie.iom."+name);

      string emsg = "";
      string type = faodel::ToLowercase(settings["type"]);
      
      
      //Check for errors
      if(type == "") {
        emsg="Iom '"+name+"' does not have a type specified in Configuration";
      } else if (Find(name)!=nullptr) {
        emsg="Iom '"+name+"' defined multiple times in Configuration iom_names";
      } else if( iom_ctors.find(type)==iom_ctors.end()) {
        emsg="Iom type '"+type+"' is unknown. Deferred iom types not currently supported";
        //TODO add ability to defer unknown types until start
      }      
      if(!emsg.empty()) {
        dbg("IOM Configuration error: "+emsg);
        throw std::runtime_error("IOM Configuration error. "+emsg);
      }

      //Do the actual creation
      RegisterIom(type, name, settings);
    }
  }

  whookie::Server::updateHook("/kelpie/iom_registry", [this] (const map<string,string> &args, stringstream &results) {
      return HandleWhookieStatus(args, results);
    });

}

/**
 * @brief Shutdown all ioms and remove all references to instances/drivers
 */
void IomRegistry::finish() {

  dbg("Finishing");
  whookie::Server::deregisterHook("kelpie/iom_registry");
  
  //Tell all ioms to shutdown (may trigger some close operations)
  for(auto &name_iomptr : ioms_by_hash_pre) {
    dbg("Removing (pre) iom "+name_iomptr.second->Name());
    name_iomptr.second->finish();
    delete name_iomptr.second;
  }
  for(auto &name_iomptr : ioms_by_hash_post) {
    dbg("Removing (post) iom "+name_iomptr.second->Name());
    name_iomptr.second->finish();
    delete name_iomptr.second;
  }
  //Get rid of all references
  ioms_by_hash_pre.clear();
  ioms_by_hash_post.clear();
  iom_ctors.clear();
  
}

/**
 * @brief Use a hash to locate a particular IOM instance. (usually for remote ops)
 * @param iom_hash The hash of the iom instance name
 * @retval ptr A pointer to the instance
 * @retval nullptr Instance was not found
 */
IomBase * IomRegistry::Find(iom_hash_t iom_hash) {

  //Start with pre-init items
  auto rhash_rptr = ioms_by_hash_pre.find(iom_hash);
  if(rhash_rptr != ioms_by_hash_pre.end()) {
    return rhash_rptr->second;
  }

  //Continue into finalized section
  if(finalized) {
    mutex->Lock();
    rhash_rptr = ioms_by_hash_post.find(iom_hash);
    if(rhash_rptr != ioms_by_hash_post.end()) {
      mutex->Unlock();
      return rhash_rptr->second;
    }
    mutex->Unlock();
  }
  //Not found
  return nullptr;
}

/**
 * @brief Whookie for dumping info about known IOMs
 * @param args Incoming whookie args
 * @param results Results handed back
 */
void IomRegistry::HandleWhookieStatus(const std::map<std::string,std::string> &args, std::stringstream &results) {

  auto ii=args.find("iom_name");
  
  if(ii!=args.end()) {
    string iom_name = faodel::ExpandPunycode(ii->second);
  
    faodel::ReplyStream rs(args, "Kelpie IOM "+iom_name, &results);
    rs.mkSection("IOM Info");
    mutex->Lock();
    IomBase *iom = Find(iom_name);
    if(iom==nullptr) {
      rs.mkText("Error: Iom '"+iom_name+"' was not found in registry");
    } else {
      iom->AppendWebInfo(rs, "/kelpie/iom_registry", args);
    }
    mutex->Unlock();
    rs.Finish();
    
  } else {
  
    faodel::ReplyStream rs(args, "Kelpie IOM Registry", &results);

    mutex->Lock();

    //Table for Drivers
    vector<vector<string>> driver_names;
    driver_names.push_back({{"Name"}});
    for(auto &name_fn : iom_ctors) {
      driver_names.push_back({{name_fn.first}});
    }
    rs.mkTable(driver_names, "IOM Constructor Functions");

    //Table for ioms
    vector<vector<string>> existing_ioms;
    existing_ioms.push_back({{"Iom Name"},{"Info"},{"Hash(Iom)"},{"Iom Type"},{"Registered At"}});
    for(auto &h_iomptr : ioms_by_hash_pre) {
      stringstream ss;
      ss<<hex<<h_iomptr.first;
      string name=h_iomptr.second->Name();
      string pname=faodel::MakePunycode(name);
      string name_link="<a href=\"/kelpie/iom_registry&iom_name="+pname+"\">"+name+"</a>";
      string detail_link="<a href=\"/kelpie/iom_registry&details=true&iom_name="+pname+"\">details</a>";
      existing_ioms.push_back( { {name_link},
                                 {detail_link},
                                 {ss.str()},
                                 {h_iomptr.second->Type()},
                                 {"Pre-Init"} });
    }
    for(auto &h_iomptr : ioms_by_hash_post) {
      stringstream ss;
      ss<<hex<<h_iomptr.first;
      string name=h_iomptr.second->Name();
      string pname=faodel::MakePunycode(name);
      string name_link="<a href=\"/kelpie/iom_registry&iom_name="+pname+"\">"+name+"</a>";
      string detail_link="<a href=\"/kelpie/iom_registry&detail=true&iom_name="+pname+"\">details</a>";
      existing_ioms.push_back( { {name_link},
                                 {detail_link},
                                 {ss.str()},
                                 {h_iomptr.second->Type()},
                                 {"Post-Init"} });
    }
    rs.mkTable(existing_ioms, "Known IOMs");
    mutex->Unlock();
    rs.Finish();
  }
}

/**
 * @brief Get a list of all the ioms that are available
 * @return Vector of names
 */
vector<string> IomRegistry::GetIomNames() const {
  vector<string> names;
  mutex->ReaderLock();
  for(auto &hash_iom : ioms_by_hash_pre) {
    names.push_back( hash_iom.second->Name() );
  }
  for(auto &hash_iom : ioms_by_hash_post) {
    names.push_back( hash_iom.second->Name() );
  }
  mutex->Unlock();
  return names;
}

/**
 * @brief Get a list of all the registered iom types
 * @return List of type names
 */
vector<string> IomRegistry::RegisteredTypes() const {
  vector<string> names;
  for(auto &name_fn : iom_ctors)
    names.push_back(name_fn.first);
  return names;
}

/**
 * @brief Get a list of parameters this IOM accepts when it is constructed
 * @param type The IOM type to lookup
 * @return Vector with the name and description of each parameter
 */
vector<pair<string,string>> IomRegistry::RegisteredTypeParameters(const string &type) const {
  auto name_fn = iom_valid_setting_fns.find(type);
  if( (name_fn== iom_valid_setting_fns.end()) || (name_fn->second == nullptr)) {
    return {}; //No entry
  }
  return name_fn->second();
}

void IomRegistry::sstr(stringstream &ss, int depth, int indent) const {
  if(depth<0) return;

  mutex->Lock();

  ss << string(indent,' ') << "[IomRegistry] State: "<<((finalized) ? "Started" : "NotStarted")
     << " Ioms: "<<(ioms_by_hash_pre.size()+ioms_by_hash_post.size())
     << " Drivers: "<<(iom_ctors.size())
     <<endl;
 
  if(depth>1) {
    indent+=2;
    ss << string(indent,' ') << "[Drivers]\n";
    for(auto &name_fn : iom_ctors) {
      ss << string(indent+2,' ')
         << name_fn.first<<endl;
    }
    
    ss << string(indent,' ') << "[Ioms]\n";
    for(auto &h_iomptr : ioms_by_hash_pre) {
      ss << string(indent+2,' ')
         << hex << h_iomptr.first << "  "
         << h_iomptr.second->Name() << " type: "
         << h_iomptr.second->Type() << " (Pre)"<<endl;
    }
    for(auto &h_iomptr : ioms_by_hash_post) {
      ss << string(indent+2,' ')
         << hex << h_iomptr.first << "  "
         << h_iomptr.second->Name() << " type: "
         << h_iomptr.second->Type() << " (Post)"<<endl;
    }
  }
  mutex->Unlock();
}


} // namespace internal
} // namespace kelpie
