// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include <sstream>
#include <stdexcept>

#include "faodel-common/FaodelTypes.hh"
#include "faodel-common/MutexWrapper.hh"
#include "faodel-common/StringHelpers.hh"
#include "faodel-common/LoggingInterface.hh"
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
 * @brief Register a new iom driver with kelpie
 * @param type The name of this driver
 * @param constructor_function A constructor for building an instance of the driver
 */
void IomRegistry::RegisterIomConstructor(string type, fn_IomConstructor_t constructor_function) {

  dbg("Registering iom driver for type "+type);

  faodel::ToLowercaseInPlace(type);
  if(finalized){
    throw std::runtime_error("Attempted to register IomConstructor after started");
  }
  
  auto name_fn = iom_ctors.find(type);
  if(name_fn != iom_ctors.end()){
    KWARN("Overwriting iom constroctor "+type);
  }
  iom_ctors[type] = constructor_function;
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
  RegisterIomConstructor("posixindividualobjects", fn_pio);


  //
  // Insert other built-in drivers here
  //
#ifdef FAODEL_HAVE_LEVELDB
  fn_IomConstructor_t fn_ldb = [] (string name, const map< string, string > &settings) -> IomBase * {
				 return new IomLevelDB( name, settings );
			       };
  RegisterIomConstructor( "leveldb", fn_ldb );
#endif

#ifdef FAODEL_HAVE_HDF5
  fn_IomConstructor_t fn_hdf5 = [] (string name, const map< string, string > &settings) -> IomBase * {
				  return new IomHDF5( name, settings );
			       };
  RegisterIomConstructor( "hdf5", fn_hdf5 );
#endif

#ifdef FAODEL_HAVE_CASSANDRA
  fn_IomConstructor_t fn_cassandra = [] (string name, const map< string, string > &settings) -> IomBase * {
				       return new IomCassandra( name, settings );
			       };
  RegisterIomConstructor( "cassandra", fn_cassandra );
#endif

  
  //Get the list of Ioms this Configuration wants to use
  string s,role;
  config.GetString(&s, "ioms");
  role = config.GetRole();

  
  if(s!=""){
    vector<string> names = faodel::Split(s,';',true);
    for(auto &name : names) {

      //Get all settings for this iom. Do hierarchy of default, iom.name, then role.iom.name
      map<string, string> settings;
      config.GetComponentSettings(&settings, "default.iom");
      config.GetComponentSettings(&settings, "iom."+name);
      config.GetComponentSettings(&settings, role+".iom."+name);

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
      if(emsg!="")
        throw std::runtime_error("IOM Configuration error. "+emsg);

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
