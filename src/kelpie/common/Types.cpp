// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 


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



} // namespace kelpie
