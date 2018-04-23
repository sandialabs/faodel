// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#ifndef LUNASA_ALLOCATORBASE_HH
#define LUNASA_ALLOCATORBASE_HH


#include <iostream>
#include <sstream>
#include <atomic>

#include <pthread.h>

#include "common/Common.hh"

#include "webhook/WebHook.hh"
#include "webhook/Server.hh"
#include "webhook/common/ReplyStream.hh"

#include "lunasa/Lunasa.hh"
#include "lunasa/DataObject.hh"

namespace lunasa {

//Note: noopPin must set pinned to something, to approximate malloc/free
void noopPin(void *base_addr, size_t length, void *&pinned);
void noopUnpin(void *&pinned);
void fakePin(void *base_addr, size_t length, void *&pinned);
void fakeUnpin(void *&pinned);

class LunasaException: public std::exception
{
  public:
    LunasaException(std::string msg)
    {
      std::string prefix("[LUNASA] ");
      mMsg = prefix + msg;
    }

    virtual const char* what() const throw()
    {
      return mMsg.c_str();
    }

  private:
    std::string mMsg;
};

class AllocatorBase : 
    public faodel::InfoInterface,
    public faodel::LoggingInterface {

public:
  AllocatorBase(const faodel::Configuration &config, std::string subcomponent_name, bool eagerPinning);
 
  void IncrRef();
  int DecrRef();  
  int RefCount();

  void RegisterPinUnpin(net_pin_fn pin, net_unpin_fn unpin);
  void RegisterMemory(void *base_addr, size_t length, void *&pinned);
  bool UsingEagerRegistration();
  bool UsingLazyRegistration();

  virtual allocation_t *Allocate(uint32_t capacity) = 0;
  virtual void Free(allocation_t* allocation) = 0;

  virtual bool SanityCheck() = 0;
  virtual void PrintState(std::ostream& stream) = 0;

  /* REPORTS whether this allocator has any allocations that are still in use */
  virtual bool HasActiveAllocations() const = 0;

  /* REPORTS the total number of bytes allocated to the user (excludes overhead such as 
     memory used to store allocation_t structure). */
  virtual size_t TotalAllocated() const = 0;

  /* REPORTS the total number of bytes managed by the allocator */
  virtual size_t TotalManaged() const = 0;

  /* REPORTS the total number of bytes that are in use (i.e., memory allocated to users
     plus overhead). */
  virtual size_t TotalUsed() const = 0;

  /* REPORTS the total number of bytes that are not currently in use. */
  virtual size_t TotalFree() const = 0;
 
  virtual std::string AllocatorType() const = 0;

  //Webhook helpers
  virtual void webhookStatus(webhook::ReplyStream &rs, const std::string &allocator_name);
  virtual void webhookMemoryAllocations(webhook::ReplyStream &rs, const std::string &allocator_name);

  //InfoInterface
  void sstr(std::stringstream &ss, int depth=0, int indent=0) const override;
  
protected:
  virtual ~AllocatorBase () {
    delete mutex;
  }
  
  std::atomic_int mRefCount; // Counts the number of instances for this allocator (not used by LDOs)
  faodel::MutexWrapper *mutex; // Lock for manipulating allocation list
  bool allocator_has_been_shutdown; // Set to true when decrRef'd to zero, but allocations still in use

  size_t mTotalManaged;
  size_t mTotalAllocated;
  size_t mTotalUsed;
  size_t mTotalFree;

  // application supplied pinning functions
  net_pin_fn   mPinFunc;
  net_unpin_fn mUnpinFunc;

  // Designates whether we pin when memory is created, or when RDMA handles are requested
  bool mEagerPinning;
};

} //namespace lunasa

#endif // ALLOCATORBASE_HH
