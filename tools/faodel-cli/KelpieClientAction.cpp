// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <sys/stat.h>
#include "KelpieClientAction.hh"

using namespace std;
using namespace faodel;




KelpieClientAction::KelpieClientAction(const std::string &long_or_short_cmd)
  : kget_meta_only(false),
    kload_all_dir_entries(false) {

  //Go through all of the commands and plug in the shorthand
  vector<pair<string,string>> command_list = {
          {"kelpie-put",      "kput"},
          {"kelpie-get",      "kget"},
          {"kelpie-get-meta", "kget"},
          {"kgetm",           "kget"},
          {"kelpie-info",     "kinfo"},
          {"kelpie-list",     "klist"},
          {"kls",             "klist"},
          {"kelpie-save",     "ksave"},
          {"kelpie-load",     "kload"},
          {"kelpie-load-dir", "kload"},
          {"kloadd",          "kload"}
  };

  //Convert a command to its shorthand
  for(auto &big_little : command_list) {
    if(( long_or_short_cmd == big_little.first) || (long_or_short_cmd==big_little.second)) {
      cmd=big_little.second;
      break;
    }
  }

  //Set values for commands that are aliases
  if((cmd=="kelpie-getm")  || (cmd=="kgetm"))  kget_meta_only=true;
  if((cmd=="kelpie-loadd") || (cmd=="kloadd")) kload_all_dir_entries=true;


  //Tag this parse as an error
  if(cmd.empty()) {
    error_message="Command '"+long_or_short_cmd+"' not valid";
  }

}



faodel::rc_t KelpieClientAction::appendKey(const string &k1, const string &k2) {
  keys.push_back(kelpie::Key(k1,k2));
  return 0;
}
faodel::rc_t KelpieClientAction::appendKey(const string &string_separated_by_pipe) {
  auto key_parts = faodel::Split(string_separated_by_pipe, '|');
  if(key_parts.size()>2) return setError("Could not parse -k/--key argument for '"+string_separated_by_pipe+"'. Can only have one '|'");
  string k1,k2;
  k1=key_parts[0];
  k2=(key_parts.size()>1) ? key_parts[1] : "";
  return appendKey(k1, k2);
}

uint16_t KelpieClientAction::getMetaCapacity() const {
  uint64_t mc = meta.size();
  if(mc==0)
    mc=generate_meta_size;
  return mc & 0x0FFFF;
}


/**
 * @brief Parse next option in an args list.
 * @param result The value for this option
 * @param args The list of args
 * @param iptr Pointer to current spot in args. Will be updated if we successfully extract option
 * @param s1 The simple version of this arg (eg -k)
 * @param s2 The long version of this arg (eg --key)
 * @retval TRUE The item matched and there was a followup arg to look at
 * @retval FALSE No match OR there was match but not enough args (error was set)
 */
bool KelpieClientAction::parseArgValue(string *result, const vector<string> &args, size_t *iptr, const string &s1, const string &s2) {

  size_t i = *iptr;
  if((args[i] == s1) || (args[i] == s2)) {
    i++;
    if(i>=args.size())  {
      setError("Could not parse "+s1+"/"+s2+": expected additional argument");
      return false;
    }
    if(result) *result = args[i];
    *iptr=i;
    return true;
  }
  return false;
}

/**
 * @brief Parse a list of kelpie client arguments, validate options, and store
 * @param args The list of arguments to look at
 * @param default_pool The pool to use if none specified
 * @retval 0 Success
 * @retval EINVAL Problems parsing (see error_message for problem)
 */
rc_t KelpieClientAction::ParseArgs(const std::vector<std::string> &args, const std::string &default_pool) {

  string key1, key2, tmp_key;
  string rank;
  string gen_meta_size, gen_data_size;
  int rc;

  struct TokenEntry { string *var; string short_name; string long_name;  };
  vector<TokenEntry> two_arg_tokens = {
          {&pool_name,     "-p",  "--pool"},
          {&rank,          "-r",  "--rank"},
          {&file_name,     "-f",  "--file"},
          {&dir_name,      "-d",  "--dir"},
          {&gen_meta_size, "-M",  "--generate-meta-size"},
          {&gen_data_size, "-D",  "--generate-data-size"},
          {&meta,          "-m",  "--meta"}
  };

  for(size_t i=0; i<args.size(); i++) {
    bool keep_looking = true;

    //Check for individual keys
    if(parseArgValue(&key1, args, &i, "-k1", "--key1")) {
      keep_looking = false;
    } else if(parseArgValue(&key2, args, &i, "-k2", "--key2")) {
      keep_looking = false;
    }
    if(!keep_looking) {
      if((!key1.empty()) && (!key2.empty())) {
        appendKey(key1, key2);
        key1.clear();
        key2.clear();
      }
      continue;
    }

    //Check for key
    if(parseArgValue(&tmp_key, args, &i, "-k", "--key")) {
      rc = appendKey(tmp_key);
      if(rc != 0) return rc;
      continue;
    }

    //Check all two-argument tokens
    for(auto &t : two_arg_tokens) {
      if(parseArgValue(t.var, args, &i, t.short_name, t.long_name)) {
        keep_looking = false;
        break;
      }
    }
    if(!keep_looking) continue;

    //Check boolean flags
    if((args[i] == "-i") || (args[i] == "--meta-only")) {
      kget_meta_only = true;
      continue;
    }

    //If above failed to parse, drop out now
    if(HasError()) return EINVAL;

    //Didn't find option? put it on the back
    if((cmd=="kinfo") || (cmd=="klist")) {
      rc = appendKey(args[i]);
      if(rc != 0) return rc;
    } else {
      //The rest need to be tighter
      remaining_args.push_back(args[i]);
    }
  }


  //Validate meta-only
  if((kget_meta_only) && (cmd != "kget"))
    return setError("Tried setting -i/--meta-only flag on command that wasn't 'kget'");

  //Validate meta size
  if(meta.size()>64*1024) {
    return setError("The -m/--meta option needs to be a string less than 64KB in size");
  }

  //Handle meta size
  if(!gen_meta_size.empty()) {
    if(!meta.empty())
      return setError("Both -M/--generate-meta-size and -m/--meta were defined. Only one can be specified");

    if(cmd!="kput")
      return setError("The -M/--generate-meta-size option can only be used with a kput operation");

    //Convert to a value
    rc = StringToUInt64(&generate_meta_size, gen_meta_size);
    if(rc != 0)
      return setError("Could not parse -M/--generate-meta-size option '"+gen_meta_size+"'");
    if(generate_meta_size >= 64*1024)
      return setError("Meta data size in -M/--generate-meta-size option must be less than 64k");
  }


  //Handle data generation
  if(!gen_data_size.empty()) {

    if(!file_name.empty())
      return setError("Both -f/--file and -D/--generate-data-size were defined. Only one can be specified.");

    if(cmd!="kput")
      return setError("The -D/--generate-data-size option can only be used on a kput operation");

    //Convert to a value
    rc = StringToUInt64(&generate_data_size, gen_data_size);
    if(rc != 0)
      return setError("Could not parse -D/--generate-data-size option '"+gen_data_size+"'");

  }

  //Validate we got the right number of keys
  if(keys.size()>1) {
    if(cmd!="kinfo")
      return setError("Multiple keys supplied to "+cmd+", which only accepts one key.");
  } //note: don't check for 0 keys. Some commands like list default to adding "*" key


  //Validate we got a pool, or plug the default pool in
  if(pool_name.empty()) {
    if(default_pool.empty())
      return setError("No pool provided. Use -p/--pool or set env var 'FAODEL_POOL'");
    pool_name = default_pool;
  }

  //Validate dir specified in kload/ksave
  if((cmd=="kload") || (cmd=="ksave")) {
    if(dir_name.empty())
      return setError("The -d/--dir option must be defined for the "+cmd+" command");

    //Try creating now to catch errors early
    struct stat results;
    int rc = stat(dir_name.c_str(), &results);
    if(rc != 0) {
      //Missing. try to create
      rc = mkdir(dir_name.c_str(), S_IRWXU | S_IRWXG);
      if(rc != 0)
        return setError("Could not create directory '"+dir_name+"' for "+cmd);

    } else if(!(results.st_mode & S_IFDIR)) {
      return setError("Can't use --dir '"+dir_name+"' in command "+cmd+" because it is not a directory");
    }

  } else {
    if(!dir_name.empty())
      return setError("The -d/--dir option can only be defined for the kload/ksave commands");
  }


  //Kput/kget don't allow extra options, but
  if((remaining_args.size()>0) && ((cmd=="kput")||(cmd=="kget"))) {
    return setError("Had extra arguments, starting with '"+remaining_args[0]+"'");
  }

  //All done
  return 0;
}