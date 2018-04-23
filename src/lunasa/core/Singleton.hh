// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#ifndef LUNASA_SINGLETON_HH
#define LUNASA_SINGLETON_HH

#include "common/Common.hh"
#include "common/LoggingInterface.hh"

#include "lunasa/core/LunasaCoreBase.hh"
#include "lunasa/core/LunasaCoreUnconfigured.hh"


namespace lunasa {

//Forward ref
namespace dirman {
namespace internal {
class SingletonImpl;
}
}

namespace internal {


class SingletonImpl 
  : public faodel::bootstrap::BootstrapInterface,
    public faodel::LoggingInterface {
  
public:

  SingletonImpl();
  ~SingletonImpl();
  
  bool IsUnconfigured();
  
  //Bootstrap API
  void Init(const faodel::Configuration &config);
  void Start();
  void Finish();
  void GetBootstrapDependencies(std::string &name, 
                       std::vector<std::string> &requires,
                       std::vector<std::string> &optional) const;

  void RegisterPinUnpin(net_pin_fn pin, net_unpin_fn unpin);
  
  LunasaCoreBase *core; //!< Pointer to the current, active core

  
private:
  LunasaCoreUnconfigured unconfigured; //!< Core to use when not started

  bool pin_functions_registered;
  net_pin_fn registered_pin_function;
  net_unpin_fn registered_unpin_function;

  bool used_tcmalloc_before; //!< tcmalloc code doesn't allow restarts
};


//A place to plug one or more static implementations for library. Lunasa
//only has one bootstrapped service.
class Singleton {
public:
  static lunasa::internal::SingletonImpl         impl;
};


} // namespace internal
} // namespace lunasa

#endif // LUNASA_SINGLETON_HH
