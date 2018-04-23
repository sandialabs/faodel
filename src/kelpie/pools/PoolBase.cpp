// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include <iostream>

#include "common/Common.hh"

#include "kelpie/Kelpie.hh"
#include "kelpie/common/KelpieInternal.hh"
#include "kelpie/pools/PoolBase.hh"

using namespace std;
using namespace faodel;

namespace kelpie {

PoolBase::PoolBase(const faodel::ResourceURL &pool_url)
  : pool_url(pool_url), lkv(nullptr), iom(nullptr), iom_hash(0) {

  //Unconfigure pool is empty
  if(pool_url.resource_type == "unconfigured") return;

  kelpie::internal::getLKV(&lkv);

  default_bucket = pool_url.bucket;
  my_nodeid = net::GetMyID();
}

string PoolBase::GetIomName(bool use_web_formatting) {
  stringstream ss;
  if(iom!=nullptr) {
    //Local version
    string name=iom->Name();
    if(!use_web_formatting) {
      ss<<"local:"<<name;
    } else {
      ss<<"<a href=/kelpie/iom_registry&iom_name="<<
        faodel::MakePunycode(name)<<">local:"<<name<<"</a>";
    }
  } else if(iom_hash!=0) {
    ss<<"remote:[0x"<<std::hex<<iom_hash<<"]";
  } else {
    ss<<"none";
  }
  return ss.str();
}


}  // namespace kelpie
