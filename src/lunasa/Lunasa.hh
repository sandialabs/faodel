// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

/**
 * @file Lunasa.hh
 *
 * @brief Lunasa.hh
 * 
 */

#ifndef LUNASA_LUNASA_HH
#define LUNASA_LUNASA_HH


#include <iostream>

#include "common/Common.hh"
#include "lunasa/common/Types.hh"

namespace lunasa {

std::string bootstrap();

typedef std::function<void(void *base_addr, size_t length, void *&pinned)> net_pin_fn;
typedef std::function<void(void *&pinned)> net_unpin_fn; 

class LunasaConfigurationException: public std::exception
{
  public:
    LunasaConfigurationException(std::string msg)
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

//Forward references that are defined in private files
namespace internal {
  class LunasaCoreBase;
}

class AllocatorBase;

class Lunasa : public faodel::InfoInterface {
public:

  Lunasa();
  Lunasa(faodel::internal_use_only_t iuo, AllocatorBase *lazy_allocator, AllocatorBase *eager_allocator);

  Lunasa(const Lunasa&);
  ~Lunasa();

  Lunasa& operator=(const Lunasa&);

  static size_t TotalAllocated();
  static size_t TotalManaged();
  static size_t TotalUsed();
  static size_t TotalFree();

  static bool SanityCheck ();

  void PrintState (std::ostream& stream) ;

  //InfoInterface
  void sstr(std::stringstream &ss, int depth=0, int indent=0) const;

private:
  AllocatorBase *lazyImpl;
  AllocatorBase *eagerImpl;
};

void Init(const faodel::Configuration &config);
void Finish();

void RegisterPinUnpin(net_pin_fn pin, net_unpin_fn unpin);

} // namespace lunasa

#endif // LUNASA_LUNASA_HH
