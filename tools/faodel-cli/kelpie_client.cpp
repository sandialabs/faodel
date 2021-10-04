// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <iomanip>
#include <fstream>
#include <atomic>
#include <sys/stat.h>
#include <dirent.h>

#include "faodel-common/StringHelpers.hh"
#include "dirman/DirMan.hh"
#include "kelpie/Kelpie.hh"

#include "faodel_cli.hh"
#include "kelpie_client.hh"

using namespace std;
using namespace faodel;




/**
 * @brief Provide help info for all the kelpie client commands
 * @param[in] subcommand The command we're looking for
 * @return FOUND whether we found this command or not
 */
bool dumpHelpKelpieClient(string subcommand) {

  string help_kput[5] = {
          "kelpie-put", "kput", "<args>", "Publish data to kelpie",
          R"(
kelpie-put arguments:
  -p/pool pool_url           : The pool to publish to (resolves w/ DirMan)
                               (pool may be specified in $FAODEL_POOL)

  -k1/--key1 rowname         : Row part of key
  -k2/--key2 colname         : Optional column part of key
     or
  -k/--key "rowname|colname" : Specify both parts of key, separated by '|'

  -f/--file filename         : Read data from file instead of stdin
  -m/--meta "string"         : Add optional meta data to the object

The kelpie-put command provides a simple way to publish a data object into
a pool. A user must specify a pool and a key name for the object. If no
file argument is provided, kelpie-put will read from stdin until it
gets an EOF. This version of the command is intended for publishing a
single, contiguous object and will truncate the data if it exceeds
the kelpie.chunk_size specified in Configuration (default = 512MB).

Examples:

  # Populate from the command line
  faodel kput --pool ref:/my/dht --key bob -m "My Stuff"
     type text on cmd line
     here, then hit con-d con-d to end

  # Use another tool to unpack a file and pipe into an object
  xzcat myfile.xz | faodel kput --pool ref:/my/dht --key1 myfile

  # Load from a file and store in row stuff, column file.txt
  faodel kput --pool ref:/my/dht --file file.txt --key "stuff|file.txt"
)"
  };
  string help_kget[5] = {
          "kelpie-get", "kget", "<args>", "Retrieve an item",
          R"(
kelpie-get arguments:
  -p/pool pool_url           : The pool to retrieve from (resolves w/ DirMan)
                               (pool may be specified in $FAODEL_POOL)

  -k1/--key1 rowname         : Row part of key
  -k2/--key2 colname         : Optional column part of key
     or
  -k/--key "rowname|colname" : Specify both parts of key, separated by '|'

  -f/--file filename         : Read data from file instead of stdin
  -i/--meta-only             : Only display the meta data for the object

The kelpie-get command provides a simple way to retrieve an object from a
pool. A user must specify the pool and key name for an object. If no file
argument is provided, the data will be dumped to stdout. A user may also
select the meta-only option to display only the meta data section of the
object.

Examples:

  # Dump an object to stdout and use standard unix tools
  faodel kget --pool ref:/my/dht --key mything | wc -l

  # Dump an object to file
  faodel kget --pool ref:/my/dht --key "stuff|file.txt" --file file2.txt
)"
  };

  string help_kgetm[5] = {
          "kelpie-get-meta", "kgetm", "<args>", "Retrieve metadata for item",
          R"(
The kelpie-get-meta command is an alias for "kelpie-get --meta-only". It
uses the same arguments as kelpie-get.
)"
  };

  string help_kinfo[5] = {
          "kelpie-info", "kinfo", "<keys>", "Retrieve info for different keys",
          R"(
kelpie-info arguments:
  -p/pool pool_url           : The pool to retrieve from (resolves w/ DirMan)
                               (pool may be specified in $FAODEL_POOL)

The kelpie-info command provides users with a way to get information about
specific keys in a pool.

Example:

  # Get sizes of different objects
  faodel kinfo --pool ref:/my/dht mykey1 mykey2 "mykey3|version9"

)"
  };

  string help_klist[5] = {
          "kelpie-list", "klist", "<key>", "Retrieve key names/sizes",
          R"(
kelpie-list arguments:
  -p/pool pool_url           : The pool to retrieve from (resolves w/ DirMan)
                               (pool may be specified in $FAODEL_POOL)

The kelpie-list command provides users with a way to learn what keys are
stored in a pool. A simple wildcard can be used to find keys that
match a specific prefix. A wildcard can be on the row, column, both,
or neither. eg
  "myrow1"             : show only the key named myrow1
  "myrow1|mycol1"      : show only the key named myrow1|mycol1
  "myrow1|*"           : show all the keys in myrow1
  "myrow*|mycol3"      : show mycol3 for all myrows

The output is a list of keys and their corresponding user lengths

Example:

  # Get sizes of different objects
  faodel klist --pool ref:/my/dht mykey1 "rowname1|col*" "row*|col*"

)"
  };
    string help_ksave[5] = {
            "kelpie-save", "ksave", "<keys>", "Save objects from a pool to a local dir",
            R"(
kelpie-save arguments:
  -p/pool pool_url           : The pool to retrieve from (resolves w/ DirMan)
                               (pool may be specified in $FAODEL_POOL)
  -d/dir directory           : The directory to store objects

The kelpie-save command provides users with a way to retrieve that
objects that are in a pool and save them to a local directory. Similar
to the list command, the user must provide a list of keys or wildcards
to retrieve (if all items are desired, use '*').

Note: The bucket for the bool is not saved in the directory structure

Example:

  # Save all items to the directory "mystruff/"
  faodel ksave --pool ref:/my/dht --dir mystuff "*"

)"
    };
    string help_kload[5] = {
            "kelpie-load", "kload", "", "Load objects from disk and store to a pool",
            R"(
kelpie-load arguments:
  -p/pool pool_url           : The pool to retrieve from (resolves w/ DirMan)
                               (pool may be specified in $FAODEL_POOL)
  -d/dir directory           : The directory to load objects from

The kelpie-load command allows you load objects from disk and push them into
pool. Objects must be in Lunasa's native disk format and be named as packed
key names).

Example:

  # Load objects that were previously ksave'd to "mystuff/"
  faodel kload --pool ref:/my/dht --dir mystuff

)"
    };


  bool found=false;
  found |= dumpSpecificHelp(subcommand, help_kput);
  found |= dumpSpecificHelp(subcommand, help_kget);
  found |= dumpSpecificHelp(subcommand, help_kgetm);
  found |= dumpSpecificHelp(subcommand, help_kinfo);
  found |= dumpSpecificHelp(subcommand, help_klist);
  found |= dumpSpecificHelp(subcommand, help_ksave);
  found |= dumpSpecificHelp(subcommand, help_kload);
  return found;
}


/**
 * @brief Read a normal file and publish its raw data into a pool using the info specified in KelpieClientAction
 * @param pool The pool to write into
 * @param action The info about what needs to be published
 * @retval -1 Could not find file, or data was too big
 * @retval 0 Success
 * @note This only uses the first key provided in args
 * @note Does not shutdown bootstrap on errors
 */
int kelpieClient_PutFromFile(kelpie::Pool &pool, const KelpieClientAction &action) {

  dbg("Putting file "+action.file_name+" to key "+action.keys[0].str());

  uint16_t meta_capacity = action.meta.size() & 0x0FFFF; //capacity checked in parser

  ifstream f;
  f.open(action.file_name, ios::in|ios::binary);
  if(!f.is_open()) {
    cerr <<"Could not open file "<<action.file_name<<endl;
    return -1;
  }

  struct stat results;
  int rc = stat(action.file_name.c_str(), &results);
  if((rc!=0) || (results.st_size >= 4*1024*1024*1024LL)){
    cerr <<"File "<<action.file_name<<" was larger than a single Kelpie object can store\n";
    return -1;
  }
  lunasa::DataObject ldo(meta_capacity, results.st_size,lunasa::DataObject::AllocatorType::eager);
  if(meta_capacity){
    memcpy(ldo.GetMetaPtr(), action.meta.c_str(), meta_capacity);
  }
  f.read(ldo.GetDataPtr<char *>(), ldo.GetDataSize());

  kelpie::object_info_t info;

  pool.Publish(action.keys[0], ldo, &info);
  return 0;
}

/**
 * @brief Generate a block of data and publish it to a pool
 * @param pool The pool to write into
 * @param action The info about what needs to be published
 * @retval -1 Could not find file, or data was too big
 * @retval 0 Success
 * @note This only uses the first key provided in args
 * @note Does not shutdown bootstrap on errors
 */
int kelpieClient_PutFromGeneratedData(kelpie::Pool &pool, const KelpieClientAction &action) {

  dbg("Putting generated data to key "+action.keys[0].str());


  lunasa::DataObject ldo(action.getMetaCapacity(), action.generate_data_size,lunasa::DataObject::AllocatorType::eager);
  if(!action.meta.empty()){
    memcpy(ldo.GetMetaPtr(), action.meta.c_str(), action.meta.size());
  }

  kelpie::object_info_t info;
  pool.Publish(action.keys[0], ldo, &info);
  return 0;
}

/**
 * @brief Take data from stdio and publish it to a pool
 * @param pool The pool to write into
 * @param action The info about what needs to be published
 * @param MAX_CAPACITY The maximum amount of data that can be sent
 * @retval 0 Success
 * @note This only uses the first key provided in args
 * @note Does not shutdown bootstrap on errors
 */
int kelpieClient_PutFromStdio(kelpie::Pool &pool, const KelpieClientAction &action, uint64_t MAX_CAPACITY) {

  uint16_t meta_capacity = action.meta.size() & 0x0FFFF; //meta is validated to be <64KB in parse

  lunasa::DataObject ldo( meta_capacity, MAX_CAPACITY, lunasa::DataObject::AllocatorType::eager );

  if(meta_capacity){
    memcpy(ldo.GetMetaPtr(), action.meta.c_str(), meta_capacity);
  }

  try {
    uint32_t offset=0;
    auto *dptr=ldo.GetDataPtr<char *>();
    size_t len;
    while((len = fread( &dptr[offset], 1, MAX_CAPACITY - offset, stdin )) > 0) {
      if(std::ferror(stdin) && !std::feof(stdin))
        throw std::runtime_error(std::strerror(errno));
      offset += len;
      if(offset==MAX_CAPACITY) break;
    }
    //Trim the data section to the actual length
    ldo.ModifyUserSizes(meta_capacity, offset);

    kelpie::object_info_t info;
    pool.Publish(action.keys[0], ldo, &info);

  } catch(std::exception const &e) {
    cerr <<"STDIO error "<<e.what()<<endl;
  }

  return 0;
}

/**
 * @brief Publish data into a pool. Input is either from a file, stdio, or generated
 * @param pool The pool to write into
 * @param config The current configurations (used to provide limits)
 * @param action The info about what needs to be published
 * @retval -1 Could not find file, or data was too big
 * @retval 0 success
 */
int kelpieClient_Put(kelpie::Pool &pool, const faodel::Configuration &config, const KelpieClientAction &action) {

  int rc;
  if(!action.file_name.empty()) {
    //Handle case (1): Read from file
    rc = kelpieClient_PutFromFile(pool, action);

  } else if(action.generate_data_size!=0) {
    //Handle case (2): generating data
    rc = kelpieClient_PutFromGeneratedData(pool, action);

  } else {
    //Handle case (3): Take data from stdin and put it into an object. Get our max chunk size
    uint64_t MAX_CAPACITY;
    config.GetUInt(&MAX_CAPACITY, "kelpie.chunk_size", "512M");
    dbg("Chunk size is "+std::to_string(MAX_CAPACITY));
    rc = kelpieClient_PutFromStdio(pool, action, MAX_CAPACITY);
  }
  return rc;
}

/**
 * @brief Request an object and write it to stdout or a file
 * @param pool The pool to read from
 * @param action The info about what needs to be retrieved
 * @retval EIO Could not write to output
 * @retval 0 Success
 * @note This only uses the first key provided in args
 * @note Does not shutdown bootstrap on errors
 */
int kelpieClient_Get(kelpie::Pool &pool, const KelpieClientAction &action) {

  lunasa::DataObject ldo;
  pool.Need(action.keys[0], &ldo);

  char *ptr; uint32_t len;
  ptr = (action.kget_meta_only) ? ldo.GetMetaPtr<char *>() : ldo.GetDataPtr<char *>();
  len = (action.kget_meta_only) ? ldo.GetMetaSize()        : ldo.GetDataSize();

  if(!action.file_name.empty()) {
    ofstream f;
    f.open(action.file_name, ios::out | ios::app | ios::binary);
    if(!f.is_open()) {
      cerr <<"Problem opening file "<<action.file_name<<" for writing\n";
      return EIO;
    }
    f.write(ptr, len);
    f.close();

  } else {
    std::cout.write(ptr,len);
  }
  return 0;
}


int kelpieClient_Info(kelpie::Pool &pool, const KelpieClientAction &action) {

  size_t max_key_len=0;
  for(auto key : action.keys) {
    size_t key_len = key.str().size();
    if(key_len>max_key_len)
      max_key_len = key_len;
  }

  for(auto key : action.keys) {
    kelpie::object_info_t info;
    rc_t rc = pool.Info(key, &info);
    cout <<setw(max_key_len)<<std::left<< key.str() <<" ";
    if(rc!=0) {
      cout << "Not found\n";
    } else {
      cout << info.str()
           << endl; //TODO Add other info like local/disk
    }
  }
  return 0;
}


int kelpieClient_List(kelpie::Pool &pool, const KelpieClientAction &action) {
  //Get all keys and append to one OC
  kelpie::ObjectCapacities oc;
  for(auto key : action.keys) {
    pool.List(key, &oc);
  }

  int max_key_len = 0;
  for(size_t i = 0; i<oc.keys.size(); i++) {
    int len = oc.keys[i].str().size();
    if(len>max_key_len)
      max_key_len = len;
  }

  for(size_t i=0; i<oc.keys.size(); i++) {
    cout << setw(max_key_len) << std::left << oc.keys[i].str() << " " << oc.capacities[i] << endl;
  }
  return 0;
}


/**
 * @brief Save a list of keys from a pool to a local directory
 * @param[in] args Command line args
 * @retval 0 (always success or immediate exit)
 */
int kelpieClient_Save(kelpie::Pool &pool, const KelpieClientAction &action) {

  //Note: dir_name vetted inside parse

  //Get all keys and append to one OC
  kelpie::ObjectCapacities oc;
  for(auto key : action.keys) {
    pool.List(key, &oc);
  }

  //Download all objects
  for(size_t i = 0; i<oc.keys.size(); i++) {
    cout << "Retrieving " << oc.keys[i].str() << " (" << oc.capacities[i] << ")\n";
    lunasa::DataObject ldo;
    int rc = pool.Need(oc.keys[i], oc.capacities[i], &ldo);
    if(rc != 0) cerr << "Could not retrieve " << oc.keys[i].str() << endl;

    //Store to a directory
    string fname = action.dir_name + "/" + faodel::MakePunycode(oc.keys[i].pup());
    ldo.writeToFile(fname.c_str());
    //TODO: save to other targets, like hdf5
  }

  return 0;
}


/**
 * @brief Read kelpie objects from a raw directory and push them to the pool
 * @param pool The pool to use for this operation
 * @param action The kelpie action/arguments for this command
 * @return
 */

int kelpieClient_Load(kelpie::Pool &pool, const KelpieClientAction &action) {

  //Note:  dir_name vetted inside parse

  struct file_info_t {
    string full_fname;
    kelpie::Key key;
    size_t disk_size;
  };
  vector<file_info_t> files;

  //Scan directory and see if there are any files that we should read
  DIR *dp;
  struct dirent *ep;
  dp = opendir(action.dir_name.c_str());
  if(dp != NULL) {
    while((ep = readdir(dp)) != NULL) {
      string name(ep->d_name);
      if(!((name == ".") || (name == ".."))) {
        string pname = action.dir_name + "/" + name;
        struct stat sb;
        if((stat(pname.c_str(), &sb) == 0) && (S_ISREG(sb.st_mode))) {
          file_info_t finfo;
          finfo.full_fname = pname;
          finfo.key.pup(faodel::ExpandPunycode(name));
          finfo.disk_size = sb.st_size;
          files.push_back(finfo);
        }
      }
    }
  }
  closedir(dp);

  if(files.size() == 0) return 0; //Done


  //Read in each file and publish it
  kelpie::ResultCollector results(files.size());
  for(auto finfo : files) {
    cout << "Reading key " << finfo.key.str() << " (" << finfo.disk_size << ")\n";
    lunasa::DataObject ldo = lunasa::LoadDataObjectFromFile(finfo.full_fname);
    pool.Publish(finfo.key, ldo, results);
  }
  results.Sync();

  return 0;
}

/**
 * @brief Check env var to see if the default FAODEL_POOL is set
 * @return A string with the FAODEL_POOL setting of empty
 */
string kelpieGetPoolFromEnv() {
  char *env_pool = getenv("FAODEL_POOL");
  if(env_pool == nullptr)  return "";
  return string(env_pool);
}

/**
 * @brief Launch a kelpie client. Converts some cli settings to config settings
 * @return Configuration the configuration that was used at launch time
 */
faodel::Configuration kelpieClientStart() {

  faodel::Configuration config;
  config.AppendFromReferences();

  //Make sure we're using dirman
  string dirman_type;
  config.GetLowercaseString(&dirman_type, "dirman.type");
  if(dirman_type == "") config.Append("dirman.type", "centralized");

  //Modify for debugging settings
  modifyConfigLogging(&config,
                      {"kelpie", "whookie"},
                      {"opbox", "dirman"});


  faodel::bootstrap::Start(config, kelpie::bootstrap);

  return config;
}

/**
 * @brief Launch one of the kelpie client commands
 * @param pool The pool to use for this operation
 * @param config The configuration (for pulling out minor constants)
 * @param action The kelpie action/arguments for this command
 * @return Results of the operation or EINVAL for command not found
 * @note Requires kelpie to be started
 */
int kelpieClient_Dispatch(kelpie::Pool &pool, faodel::Configuration &config, const KelpieClientAction &action) {

  string cmd2 = action.cmd;
  int rc=EINVAL;
  if       (cmd2=="kput")  { rc = kelpieClient_Put(pool, config, action);
  } else if(cmd2=="kget")  { rc = kelpieClient_Get(pool, action);
  } else if(cmd2=="kinfo") { rc = kelpieClient_Info(pool, action);
  } else if(cmd2=="klist") { rc = kelpieClient_List(pool, action);
  } else if(cmd2=="ksave") { rc = kelpieClient_Save(pool, action);
  } else if(cmd2=="kload") { rc = kelpieClient_Load(pool, action);
  }
  return rc;
}

/**
 * @brief One-shot kelpieClient function. Parses args, starts kelpie command, and shutsdown
 * @param[in] cmd The command that the user supplied (eg kput)
 * @param[in] args The other arguments passed in on the cli to parse
 * @retval 0 Found the command
 * @retval ENOENT Didn't find the command
 * @note This Starts and Stops kelpie
 */
int checkKelpieClientCommands(const std::string &cmd, const vector<string> &args) {

  //Figure out what command this is. Bail out if it's not a kelpie command
  KelpieClientAction action(cmd);
  if(action.HasError()) return ENOENT; //Didn't match


  //Parse this command's arguments
  string default_pool = kelpieGetPoolFromEnv();
  action.ParseArgs(args, default_pool);
  action.exitOnError();     //Parse errors get marked but not handled until here
  action.exitOnExtraArgs(); //Bail out if we have unrecognized options

  //Start up
  auto config = kelpieClientStart();
  auto pool = kelpie::Connect(action.pool_name);
  pool.ValidOrDie();

  int rc = kelpieClient_Dispatch(pool, config, action);

  //Shut down
  if(faodel::bootstrap::IsStarted()) {
    faodel::bootstrap::Finish();
  }
  return rc;
}
