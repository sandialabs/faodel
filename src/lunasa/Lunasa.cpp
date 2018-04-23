// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include "Lunasa.hh"
#include <iostream>
#include <sstream>
#include <assert.h>

#include <string.h> //memcpy
// TODO does malloc.h exist?
// #include <malloc.h>
#include <pthread.h>


#include "webhook/WebHook.hh"
#include "webhook/Server.hh"

#include "lunasa/Lunasa.hh"

#include "lunasa/core/LunasaCoreBase.hh"
#include "lunasa/core/Singleton.hh"


using namespace std;

namespace lunasa {

Lunasa GetInstance() {
  return internal::Singleton::impl.core->GetLunasaInstance();
}

//The Singleton holds all the init/start/finish code
void Init(const faodel::Configuration &config)
{
  internal::Singleton::impl.Init(config);  
}
void Start()
{
  internal::Singleton::impl.Start();
}
void Finish()
{
  internal::Singleton::impl.Finish();
}

void RegisterPinUnpin(net_pin_fn pin, net_unpin_fn unpin) 
{
  internal::Singleton::impl.core->RegisterPinUnpin(pin, unpin);
}

Lunasa::Lunasa()
{}

Lunasa::Lunasa(faodel::internal_use_only_t iuo, AllocatorBase *lazy_allocator, AllocatorBase *eager_allocator)
{
  this->lazyImpl = lazy_allocator;
  this->eagerImpl = eager_allocator;
}

Lunasa::Lunasa(const Lunasa& copy) 
{
  //note: either core is valid, or the others
  lazyImpl = copy.lazyImpl;
  eagerImpl = copy.eagerImpl;
}

Lunasa::~Lunasa() 
{
  //Everything is a reference.
}

Lunasa& Lunasa::operator=(const Lunasa& copy) 
{
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

} // namespace lunasa
