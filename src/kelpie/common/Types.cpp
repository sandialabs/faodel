// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 


#include "faodel-common/StringHelpers.hh"

#include "kelpie/common/Types.hh"

using namespace std;
using namespace kelpie;

namespace kelpie {

string availability_to_string(const Availability &a){
  switch(a) {
  case Availability::Unavailable:    return "Unavailable";
  case Availability::Requested:      return "Requested";
  case Availability::InLocalMemory:  return "InLocalMemory";
  case Availability::InRemoteMemory: return "InRemoteMemory";
  case Availability::InNVM:          return "InNVM";
  case Availability::InDisk:         return "InDisk";
  default: return "Unknown?";
  }
}

string kv_row_info_t::str(){
  std::stringstream ss;
  ss<<"RowInfo: "
    <<" RowCols: "             << num_cols_in_row
    <<" RowBytes: "            << row_bytes
    <<" Availability: "        << availability_to_string(availability)
    <<" WaitingNodes: "        << num_row_receiver_nodes
    <<" WaitingFunctions: "    << num_row_dependencies;

  return ss.str();
}


void kv_row_info_t::ChangeAvailabilityFromLocalToRemote(){
  if(availability == Availability::InLocalMemory)
    availability = Availability::InRemoteMemory;
}


string kv_col_info_t::str(){
  std::stringstream ss;
  ss<<"ColInfo: "
    <<" Origin: "             << node_origin.GetHex()
    <<" NumBytes: "           << std::dec << num_bytes
    <<" WaitingNodes: "       << num_col_receiver_nodes
    <<" LocalDependencies: "  << num_col_dependencies
    <<" Availability: "       << availability_to_string(availability);
  return ss.str();
}

void kv_col_info_t::ChangeAvailabilityFromLocalToRemote(){
  if(availability == Availability::InLocalMemory)
    availability = Availability::InRemoteMemory;
}

/**
 * @brief When an op lands on a remote node, we need to switch the ActionFlags to reflect new location
 * @param f Original flags
 * @return Modified flags that have moved remote settings to local spots
 */
pool_behavior_t PoolBehavior::ChangeRemoteToLocal(pool_behavior_t f){

  //Erase the locals
  f = f & ~(PoolBehavior::WriteToLocal | PoolBehavior::ReadToLocal);

  //Convert the Remotes to Local
  if(f & PoolBehavior::WriteToRemote)  f = f | PoolBehavior::WriteToLocal;
  if(f & PoolBehavior::ReadToRemote)   f = f | PoolBehavior::ReadToLocal;

  //Erase the remotes
  f = f & ~(PoolBehavior::WriteToRemote | PoolBehavior::ReadToRemote);

  return f;
}

/**
 * @brief Parse a line of '_' separated labels for actions (eg WriteToLocal_ReadToNone)
 * @param parse_line Input line
 * @return ActionFlag
 */
pool_behavior_t PoolBehavior::ParseString(string parse_line) {

  faodel::ToLowercaseInPlace(parse_line);
  auto syms = faodel::Split(parse_line,'_', true);

  pool_behavior_t f=0;
  for(auto &s : syms) {
    if      (s=="writetolocal")  f |= PoolBehavior::WriteToLocal;
    else if (s=="writetoremote") f |= PoolBehavior::WriteToRemote;
    else if (s=="writetoiom") f |= PoolBehavior::WriteToIOM;
    else if (s=="readtolocal") f |= PoolBehavior::ReadToLocal;
    else if (s=="readtoremote") f |= PoolBehavior::ReadToRemote;
    else if (s=="writearound") f |= PoolBehavior::WriteAround;
    else if (s=="writeall") f |= PoolBehavior::WriteToAll;
    else if (s=="readtonone") f |= PoolBehavior::ReadToNone;
    else if (s=="defaultiom") f |= PoolBehavior::DefaultIOM;
    else if (s=="defaultlocaliom") f |= PoolBehavior::DefaultLocalIOM;
    else if (s=="defaultremoteiom") f |= PoolBehavior::DefaultRemoteIOM;
    else if (s=="defaultcachingiom") f |= PoolBehavior::DefaultCachingIOM;
    else {
      throw runtime_error("Unable to parse Action string token "+s+" inside "+parse_line);
    }
  }
  return f;
}
std::string PoolBehavior::GetString(pool_behavior_t f) {
  vector<string> names;

  if(f & PoolBehavior:: WriteToLocal) names.push_back("WriteToLocal");
  if(f & PoolBehavior:: WriteToRemote) names.push_back("WriteToRemote");
  if(f & PoolBehavior:: WriteToIOM) names.push_back("WriteToIOM");
  if(f & PoolBehavior:: ReadToLocal) names.push_back("ReadToLocal");
  if(f & PoolBehavior:: ReadToRemote) names.push_back("ReadToRemote");
  if(f & PoolBehavior:: ReadToLocal) names.push_back("ReadToLocal");

  string s = faodel::Join(names,' ');
  return s;
}



} // namespace kelpie
