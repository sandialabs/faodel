#include <iomanip>
#include <fstream>
#include <sys/stat.h>

#include "faodel-common/StringHelpers.hh"
#include "dirman/DirMan.hh"
#include "kelpie/Kelpie.hh"

#include "faodel_cli.hh"

using namespace std;
using namespace faodel;

int kelpieClientPut(const vector<string> &args);
int kelpieClientGet(bool only_meta, const vector<string> &args);
int kelpieClientInfo(const vector<string> &args);
int kelpieClientList(const vector<string> &args);


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
  -m/--meta-only             : Only display the meta data for the object

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


  bool found=false;
  found |= dumpSpecificHelp(subcommand, help_kput);
  found |= dumpSpecificHelp(subcommand, help_kget);
  found |= dumpSpecificHelp(subcommand, help_kgetm);
  found |= dumpSpecificHelp(subcommand, help_kinfo);
  found |= dumpSpecificHelp(subcommand, help_klist);
  return found;
}

int checkKelpieClientCommands(const std::string &cmd, const vector<string> &args) {

  if(     (cmd == "kelpie-put")       || (cmd == "kput"))  return kelpieClientPut(args);
  else if((cmd == "kelpie-get")       || (cmd == "kget"))  return kelpieClientGet(false, args);
  else if((cmd == "kelpie-get-meta")  || (cmd == "kgetm")) return kelpieClientGet(true, args);
  else if((cmd == "kelpie-info")      || (cmd == "kinfo")) return kelpieClientInfo(args);
  else if((cmd == "kelpie-list") || (cmd == "klist") || (cmd=="kls")) return kelpieClientList(args);

  //No command
  return ENOENT;
}

bool getArgValue(string *result, const vector<string> &args, size_t &i, const string &s1, const string &s2) {

  if((args[i] == s1) || (args[i] == s2)) {
    i++;
    if(i>=args.size()) {
      cerr<<"Error parsing "<<s1<<"/"<<s2<<": expected additional argument\n";
      exit(-1);
    }
    *result = args[i];
    return true;
  }
  return false;
}


void kelpieClientParseBasicArgs(const vector<string> &args,
                                vector<string> *remaining_args,
                                string *pool_name, string *file_name,
                                kelpie::Key *key ) {

  string key1, key2, tmp_key;

  for(size_t i = 0; i<args.size(); i++) {
    string val;
    if(       getArgValue(pool_name, args, i, "-p",  "--pool")) {
    } else if(getArgValue(&key1,     args, i, "-k1", "--key1")) {
    } else if(getArgValue(&key2,     args, i, "-k2", "--key2")) {
    } else if(getArgValue(file_name, args, i, "-f",  "--file")) {
    } else if(getArgValue(&tmp_key,  args, i, "-k",  "--key")) {

      //Split the key into two parts
      auto keys = faodel::Split(tmp_key, '|');
      if(keys.size()>2) {
        cerr << "Could not parse -k/--key argument for '" << tmp_key << "'. Must be of form 'key1' or 'key1|key2'\n";
        exit(-1);
      }
      key1 = keys[0];
      if(keys.size()>1)
        key2 = keys[1];

    } else {
      remaining_args->push_back(args[i]);
    }

  }

  //Generate a proper key
  if(key1.empty()) {
    cerr << "No key provided. Cannot publish.\n";
    exit(-1);
  }
  *key = kelpie::Key(key1, key2);

  //Pull from env var is no pool name
  if(pool_name->empty()) {
    char *env_pool = getenv("FAODEL_POOL");
    if(env_pool == nullptr) {
      cerr << "No pool provided. Use -p/--pool resource_url or set env var FAODEL_POOL\n";
      exit(-1);
    }
    *pool_name = string(env_pool);
  }
}

faodel::Configuration kelpieClientStart() {

  faodel::Configuration config;

  //Make sure we're using dirman
  string dirman_type;
  config.GetLowercaseString(&dirman_type, "dirman.type");
  if(dirman_type == "") config.Append("dirman.type", "centralized");

  //Modify for debugging settings
  modifyConfigLogging(&config,
                      {"kelpie", "whookie"},
                      {"opbox", "dirman"});

  config.AppendFromReferences();

  faodel::bootstrap::Start(config, kelpie::bootstrap);

  return config;
}

int kelpieClientPut(const vector<string> &args) {

  string pool_name, file_name, meta;
  kelpie::Key key;
  vector<string> custom_args;

  //Pull out basic args
  kelpieClientParseBasicArgs(args, &custom_args, &pool_name, &file_name, &key);

  //Pull out put-specific args
  for(size_t i=0; i<custom_args.size(); i++) {
    string val;
    if( getArgValue(&meta,      custom_args, i, "-m",  "--meta")) {
    } else {
      cerr <<"Unknown argument: "<<custom_args[i]<<endl;
      exit(-1);
    }
  }

  auto config = kelpieClientStart();
  auto pool = kelpie::Connect(pool_name);

  uint64_t MAX_CAPACITY;
  config.GetUInt(&MAX_CAPACITY, "kelpie.chunk_size", "512M");
  dbg("Chunk size is "+std::to_string(MAX_CAPACITY));

  if(meta.size()>=64*1024){
    cerr <<"Meta sectionn must be less than 16KB\n";
    exit(-1);
  }
  uint16_t meta_capacity = meta.size() & 0x0FFFF;


  if(!file_name.empty()) {

    ifstream f;
    f.open(file_name, ios::in|ios::binary);
    if(!f.is_open()) {
      cerr <<"Could not open file "<<file_name<<endl;
      faodel::bootstrap::Finish();
      return -1;
    }

    struct stat results;
    int rc = stat(file_name.c_str(), &results);
    if((rc!=0) || (results.st_size >= 4*1024*1024*1024LL)){
      cerr <<"File "<<file_name<<" was larger than a single Kelpie object can store\n";
      faodel::bootstrap::Finish();
      return -1;
    }
    lunasa::DataObject ldo(meta_capacity, results.st_size,lunasa::DataObject::AllocatorType::eager);
    if(meta_capacity){
      memcpy(ldo.GetMetaPtr(), meta.c_str(), meta_capacity);
    }
    f.read(ldo.GetDataPtr<char *>(), ldo.GetDataSize());

    kelpie::kv_row_info_t row;
    kelpie::kv_col_info_t col;

    pool.Publish(key, ldo, &row, &col);
    //cout <<"Publish row col info is "<<row.str() << "\n"<< col.str()<<endl;

  } else {

    lunasa::DataObject ldo( meta_capacity, MAX_CAPACITY, lunasa::DataObject::AllocatorType::eager );

    if(meta_capacity){
      memcpy(ldo.GetMetaPtr(), meta.c_str(), meta_capacity);
    }

    //cout <<"About to read from stdin\n";
    try {
      uint32_t offset=0;
      auto *dptr=ldo.GetDataPtr<char *>();
      size_t len;
      while((len = fread( &dptr[offset], 1, MAX_CAPACITY - offset, stdin )) > 0) {
        //cout <<"Pulled in chunk of len "<<len<<endl;
        if(std::ferror(stdin) && !std::feof(stdin))
          throw std::runtime_error(std::strerror(errno));
        offset += len;
        if(offset==MAX_CAPACITY) break;
      }
      //Trim the data section to the actual length
      ldo.ModifyUserSizes(meta_capacity, offset);

      kelpie::kv_row_info_t row;
      kelpie::kv_col_info_t col;

      //cout <<"About to send key "<<key.str()<<" or k1="<<key.K1()<<" k2="<<key.K2()<<endl;
      pool.Publish(key, ldo, &row, &col);

      //cout <<"Publish row col info is "<<row.str() << "\n"<< col.str()<<endl;

    } catch(std::exception const &e) {
      cerr <<"STDIO error "<<e.what()<<endl;
    }
  }

  faodel::bootstrap::Finish();

  return 0;
}

int kelpieClientGet(bool only_meta, const vector<string> &args) {

  string pool_name, file_name, meta;
  kelpie::Key key;
  vector<string> custom_args;

  //Pull out basic args
  kelpieClientParseBasicArgs(args, &custom_args, &pool_name, &file_name, &key);

  //Pull out put-specific args
  for(size_t i=0; i<custom_args.size(); i++) {
    string val;
    if( (custom_args[i]=="-m") || (custom_args[i]=="--meta-only")) {
      only_meta=true;
    } else {
      cerr <<"Unknown argument: "<<custom_args[i]<<endl;
      exit(-1);
    }
  }

  auto config = kelpieClientStart();
  auto pool = kelpie::Connect(pool_name);

  lunasa::DataObject ldo;
  pool.Need(key, &ldo);

  char *ptr; uint32_t len;
  ptr = (only_meta) ? ldo.GetMetaPtr<char *>() : ldo.GetDataPtr<char *>();
  len = (only_meta) ? ldo.GetMetaSize()        : ldo.GetDataSize();

  if(!file_name.empty()) {
    ofstream f;
    f.open(file_name, ios::out | ios::app | ios::binary);
    if(!f.is_open()) {
      cerr <<"Problem opening file "<<file_name<<" for writing\n";
      faodel::bootstrap::Finish();
      return -1;
    }
    f.write(ptr, len);
    f.close();

  } else {
    std::cout.write(ptr,len);
  }

  faodel::bootstrap::Finish();
  return 0;

}

int kelpieClientInfo(const vector<string> &args) {


  string tmp_key;
  vector<kelpie::Key> keys;
  string pool_name;

  int max_key_len;
  for(size_t i = 0; i<args.size(); i++) {
    string val;
    if(       getArgValue(&pool_name, args, i, "-p", "--pool")) {
    } else {

      //Must be passing a key, either with "-k keyname" or just "keyname"
      if(getArgValue(&tmp_key,   args, i, "-k", "--key")) { //parsed to tmp_key
      } else {
        tmp_key = args[i];
      }

      //Convert strings to actual keys
      auto key_parts = faodel::Split(tmp_key, '|');
      if(key_parts.size()>2) {
        cerr << "Could not parse -k/--key argument for '" << tmp_key << "'. Must be of form 'key1' or 'key1|key2'\n";
        exit(-1);
      }
      string k1, k2;
      k1=key_parts[0];
      k2=(key_parts.size()>1) ? key_parts[1] : "";
      kelpie::Key key(k1,k2);
      int len = key.str().size();
      if(len>max_key_len) max_key_len = len;
      keys.push_back(key);
    }
  }

  //Pull from env var is no pool name
  if(pool_name.empty()) {
    char *env_pool = getenv("FAODEL_POOL");
    if(env_pool == nullptr) {
      cerr << "No pool provided. Use -p/--pool resource_url or set env var FAODEL_POOL\n";
      exit(-1);
    }
    pool_name = string(env_pool);
  }

  auto pp=ResourceURL(pool_name);

  auto config = kelpieClientStart();
  auto pool = kelpie::Connect(pool_name);

  for(auto key : keys) {
    kelpie::kv_col_info_t col_info;
    rc_t rc = pool.Info(key, &col_info);
    cout <<setw(max_key_len)<<std::left<< key.str() <<" : ";
    if(rc!=0) {
      cout << "Not found\n";
    }else {
      cout << col_info.num_bytes <<endl; //TODO Add other info like local/disk
    }
  }

  faodel::bootstrap::Finish();
  return 0;

}

int kelpieClientList(const vector<string> &args) {


  string tmp_key;
  vector<kelpie::Key> keys;
  string pool_name;

  for(size_t i = 0; i<args.size(); i++) {
    string val;
    if(       getArgValue(&pool_name, args, i, "-p", "--pool")) {
    } else {

      //Must be passing a key, either with "-k keyname" or just "keyname"
      if(getArgValue(&tmp_key,   args, i, "-k", "--key")) { //parsed to tmp_key
      } else {
        tmp_key = args[i];
      }

      //Convert strings to actual keys
      auto key_parts = faodel::Split(tmp_key, '|');
      if(key_parts.size()>2) {
        cerr << "Could not parse -k/--key argument for '" << tmp_key << "'. Must be of form 'key1' or 'key1|key2'\n";
        exit(-1);
      }
      string k1, k2;
      k1=key_parts[0];
      k2=(key_parts.size()>1) ? key_parts[1] : "";
      kelpie::Key key(k1,k2);
      keys.push_back(key);
    }
  }

  //Pull from env var is no pool name
  if(pool_name.empty()) {
    char *env_pool = getenv("FAODEL_POOL");
    if(env_pool == nullptr) {
      cerr << "No pool provided. Use -p/--pool resource_url or set env var FAODEL_POOL\n";
      exit(-1);
    }
    pool_name = string(env_pool);
  }

  auto pp=ResourceURL(pool_name);

  auto config = kelpieClientStart();
  auto pool = kelpie::Connect(pool_name);

  //Get all kets and append to one OC
  kelpie::ObjectCapacities oc;
  for(auto key : keys) {
    pool.List(key, &oc);
  }

  int max_key_len=0;
  for(size_t i=0; i<oc.keys.size(); i++) {
    int len = oc.keys[i].str().size();
    if(len>max_key_len)
      max_key_len = len;
  }

  for(size_t i=0; i<oc.keys.size(); i++) {
    cout << setw(max_key_len) << std::left << oc.keys[i].str() << " " << oc.capacities[i] << endl;
  }

  faodel::bootstrap::Finish();
  return 0;



}
