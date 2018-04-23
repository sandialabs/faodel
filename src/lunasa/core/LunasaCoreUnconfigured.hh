// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#ifndef LUNASA_LUNASACOREUNCONFIGURED_HH
#define LUNASA_LUNASACOREUNCONFIGURED_HH

#include "common/Common.hh"
#include "lunasa/core/LunasaCoreBase.hh"

namespace lunasa {
namespace internal {

class LunasaCoreUnconfigured : 
    public LunasaCoreBase {
  
public:
  LunasaCoreUnconfigured();
  ~LunasaCoreUnconfigured();
  
  void init(std::string lmm_name, std::string emm_name, bool use_webhook,
            const faodel::Configuration &config);
  void start(){}
  void finish(){}

  void RegisterPinUnpin(net_pin_fn pin, net_unpin_fn unpin);

  allocation_t *AllocateEager(uint32_t metaCapacity, uint32_t dataCapacity);
  allocation_t *AllocateLazy(uint32_t metaCapacity, uint32_t dataCapacity);
  
  size_t TotalAllocated() const;
  size_t TotalManaged() const;
  size_t TotalUsed() const;
  size_t TotalFree() const;

  bool SanityCheck();
  void PrintState(std::ostream& stream);

  Lunasa GetLunasaInstance();

  std::string GetType() const { return "unconfigured"; };

  //InfoInterface
  void sstr(std::stringstream &ss, int depth=0, int indent=0) const;

private:
  void Panic(std::string fname) const;
  
};
} // namespace internal
} // namespace lunasa

#endif // LUNASA_LUNASACOREUNCONFIGURED_HH
