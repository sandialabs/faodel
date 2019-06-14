// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include <iostream>

#include "faodel-common/Common.hh"

#include "kelpie/Kelpie.hh"
#include "kelpie/common/KelpieInternal.hh"
#include "kelpie/pools/PoolBase.hh"

using namespace std;
using namespace faodel;

namespace kelpie {

PoolBase::PoolBase(const faodel::ResourceURL &pool_url)
  : LoggingInterface("kelpie.pool"),
    pool_url(pool_url), lkv(nullptr), iom(nullptr), iom_hash(0),
    behavior_flags(PoolBehavior::DefaultBaseClass) {

  //Unconfigure pool is empty
  if(pool_url.Type() == "unconfigured") return;

  kelpie::internal::getLKV(&lkv);

  default_bucket = pool_url.bucket;
  my_nodeid = net::GetMyID();

  auto iom_name = pool_url.GetOption("iom");
  if(iom_name!="") {

    if(iom_name.compare(0,2, "0x")==0) {
      unsigned long val;
      stringstream ss;
      ss << std::hex<<iom_name.substr(2);  //0x not supported
      ss >> val;
      iom_hash = static_cast<iom_hash_t>(val);
    } else {
      iom_hash = faodel::hash32(iom_name);
    }

    behavior_flags = PoolBehavior::DefaultIOM;

    //NOTE: Local is responsible for finding actual iom, since it's the only one where *iom is used.
  }
  auto behavior = pool_url.GetOption("behavior");
  if(!behavior.empty()) {
    behavior_flags = PoolBehavior::ParseString(behavior);
  }

}

string PoolBase::GetIomName(bool use_web_formatting, bool add_detail) {
  stringstream ss;
  if(iom!=nullptr) {
    //Local version
    string name=iom->Name();
    if(!use_web_formatting) {
      ss<<"local:"<<name;
    } else {
      ss<<"<a href=/kelpie/iom_registry&iom_name="<<
        faodel::MakePunycode(name);
      if(add_detail) ss<<"&details=true>details</a>";
      else           ss<<">local:"<<name<<"</a>";
    }
  } else if(iom_hash!=0) {
    ss<<"remote:[0x"<<std::hex<<iom_hash<<"]";
  } else {
    ss<<"none";
  }
  return ss.str();
}


}  // namespace kelpie
