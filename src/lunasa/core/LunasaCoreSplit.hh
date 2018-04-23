// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#ifndef LUNASA_LUNASACORESPLIT_HH
#define LUNASA_LUNASACORESPLIT_HH

#include "lunasa/core/LunasaCoreBase.hh"
#include "lunasa/common/Allocation.hh"

namespace lunasa {
namespace internal {

class LunasaCoreSplit : public LunasaCoreBase {
public:
  LunasaCoreSplit();
  ~LunasaCoreSplit();
  
  void init(std::string lmm_name, std::string emm_name, bool use_webhook,
            const faodel::Configuration &config);
  void start() {}
  void finish();

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

  std::string GetType() const { return "split"; };

  //WebHook
  void HandleWebhookStatus(const std::map<std::string,std::string> &args, std::stringstream &results);
  void HandleWebhookEagerDetails(const std::map<std::string,std::string> &args, std::stringstream &results);
  void HandleWebhookLazyDetails(const std::map<std::string,std::string> &args, std::stringstream &results);
  
  //InfoInterface
  void sstr(std::stringstream &ss, int depth=0, int indent=0) const;
  
private:
  AllocatorBase *lazy_allocator;
  AllocatorBase *eager_allocator;
};

} // namespace internal
} // namespace lunasa

#endif // LUNASA_LUNASACORESPLIT_HH
