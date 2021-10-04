// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <iostream>
#include <sstream>

#include <string.h> //memcpy
#include <pthread.h>
#include <sys/stat.h>

#include "faodelConfig.h"

#include "whookie/Whookie.hh"
#include "whookie/Server.hh"

#include "lunasa/Lunasa.hh"

#include "lunasa/core/LunasaCoreBase.hh"
#include "lunasa/core/Singleton.hh"


using namespace std;

namespace lunasa {

Lunasa GetInstance() {
  return internal::Singleton::impl.core->GetLunasaInstance();
}

//The Singleton holds all the init/start/finish code
void Init(const faodel::Configuration &config) {
  internal::Singleton::impl.Init(config);  
}
void Start() {
  internal::Singleton::impl.Start();
}
void Finish() {
  internal::Singleton::impl.Finish();
}

/**
 * @brief Internal hook for a network layer to register its pin/unpin functions
 * @param pin The function Lunasa calls to pinning a block of memory
 * @param unpin The function Lunasa calls to unpin a block of memory
 */
void RegisterPinUnpin(net_pin_fn pin, net_unpin_fn unpin) {
  internal::Singleton::impl.core->RegisterPinUnpin(pin, unpin);
}

/**
 * @brief Update Lunasa with information about how to display a particular DataObject type
 * @param tag The integer id for a particular user data type (usually a hash of the name)
 * @param name The name for this data type (usually hashed to make the tag)
 * @param dump_func The function for dumping this DataObject type to a whookie replystream
 */
void RegisterDataObjectType(dataobject_type_t tag, std::string name, fn_DataObjectDump_t dump_func) {
  internal::Singleton::impl.dataobject_type_registry.RegisterDataObjectType(tag, name, dump_func);
}

/**
 * @brief Remove a dumping function from the registry
 * @param tag The integer id for a particular user data type (usually a hash of the name)
 */
void DeregisterDataObjectType(dataobject_type_t tag) {
  return internal::Singleton::impl.dataobject_type_registry.DeregisterDataObjectType(tag);
}

/**
 * @brief Dump info about the DataObject to a reply stream. If unregistered, dump generic hex data
 * @param ldo The DataObject that is to be dumped
 * @param rs The whookie ReplyStream that is appended
 * @retval TRUE A user-defined dump function was found for dumping the DataObject type
 * @retval FALSE No user-defined dump function available for this DataObject. Dumped using hex output
 */
bool DumpDataObject(const DataObject &ldo, faodel::ReplyStream &rs) {
  return internal::Singleton::impl.dataobject_type_registry.DumpDataObject(ldo, rs);
}
/**
 * @brief Read a DataObject from disk and store it in a new DataObject
 * @param[in] filename The name of the DataObject to load from disk
 * @retval DataObject The object
 * @throw runtime_error if file cannot be read
 */
DataObject LoadDataObjectFromFile(std::string filename) {

  uint32_t header_size = lunasa::DataObject::GetHeaderSize();
  struct stat results;
  if( (stat(filename.c_str(), &results) != 0) || (results.st_size < header_size)) {
    throw std::runtime_error("Could not read Lunasa DataObject '"+filename+"'");
  }
  lunasa::DataObject ldo(results.st_size - header_size);
  ldo.readFromFile(filename.c_str());
  return ldo;
}


// Note: The Lunasa class is just a wrapper that makes calls to the singleton

Lunasa::Lunasa() {
}

Lunasa::Lunasa(faodel::internal_use_only_t iuo, 
               internal::AllocatorBase *lazy_allocator, 
               internal::AllocatorBase *eager_allocator) 
        : lazyImpl(lazy_allocator),
          eagerImpl(eager_allocator) {
}

Lunasa::Lunasa(const Lunasa& copy) {
  //note: either core is valid, or the others
  lazyImpl = copy.lazyImpl;
  eagerImpl = copy.eagerImpl;
}

Lunasa::~Lunasa() {
  //Everything is a reference.
}

Lunasa& Lunasa::operator=(const Lunasa& copy) {
  //note: either core is valid, or the others
  lazyImpl = copy.lazyImpl;
  eagerImpl = copy.eagerImpl;
  return *this;
}

size_t Lunasa::TotalAllocated() { return  internal::Singleton::impl.core->TotalAllocated(); }
size_t Lunasa::TotalManaged() {   return  internal::Singleton::impl.core->TotalManaged(); }
size_t Lunasa::TotalUsed() {      return  internal::Singleton::impl.core->TotalUsed(); }
size_t Lunasa::TotalFree() {      return  internal::Singleton::impl.core->TotalFree(); }

bool   Lunasa::SanityCheck()                { return  internal::Singleton::impl.core->SanityCheck();        }
void   Lunasa::PrintState(ostream& stream)  {         internal::Singleton::impl.core->PrintState(stream);   }

void Lunasa::sstr(std::stringstream &ss, int depth, int indent) const {
  return internal::Singleton::impl.core->sstr(ss, depth, indent);
}

/**
 * @brief Get a list of the allocators that are available in this installation
 * @return Vector of allocator names (excluding Unconfigured)
 */
vector<string> AvailableAllocators() {
  vector<string> allocators;
  allocators.push_back("malloc");
  #ifdef Faodel_ENABLE_TCMALLOC
    allocators.push_back("tcmalloc");
  #endif
  return allocators;
}

/**
 * @brief Get a list of the Lunasa cores that are available in this installation
 * @return Vector of core names (excluding Unconfigured)
 */
vector<string> AvailableCores() {
  vector<string> cores;
  cores.push_back("split");

  return cores;
}

} // namespace lunasa
