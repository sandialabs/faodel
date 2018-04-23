// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#ifndef KELPIE_IOMREGISTRY_HH
#define KELPIE_IOMREGISTRY_HH

#include <map>
#include <string>

#include "common/InfoInterface.hh"
#include "common/StringHelpers.hh"
#include "common/ResourceURL.hh"

#include "kelpie/common/Types.hh"
#include "kelpie/ioms/IomBase.hh"

namespace kelpie {
namespace internal {

/**
 * @brief An internal structure for managing IOM drivers and instantiated IOMs
 *
 * Kelpie uses this module to keep track of two types of IOM registrations.
 * First, users may register new IOM drivers for talking to different storage
 * technologies by calling the RegisterIomConstructor() class before Kelpie
 * starts. Second, a user may register a new instance of an IOM via RegisterIom(),
 * or find and existing instance by name via Find().
 *
 * @note: it is expected that users will access these mechanisms via shortcuts
 *        in the Kelpie.hh file.
 */
class IomRegistry
  : public faodel::InfoInterface {

public:
  IomRegistry();
  ~IomRegistry();

  void init(const faodel::Configuration &config);
  void start() { finalized=true; }
  void finish();

  void RegisterIom(std::string type, std::string name,  const std::map<std::string,std::string> &settings);
  void RegisterIomConstructor(std::string type, fn_IomConstructor_t ctor_function);

  IomBase * Find(std::string iom_name) { return Find(faodel::hash32(iom_name)); }
  IomBase * Find(uint32_t iom_hash);
  
  //InfoInterface function
  void sstr(std::stringstream &ss, int depth=0, int indent=0) const;
  
private:
  faodel::MutexWrapper *mutex;
  bool finalized;
  std::map<uint32_t, IomBase *> ioms_by_hash_pre;
  std::map<uint32_t, IomBase *> ioms_by_hash_post;
  
  std::map<std::string, fn_IomConstructor_t> iom_ctors;

  void registerIom(IomBase *iom);
  
  void HandleWebhookStatus(const std::map<std::string, std::string> &args, std::stringstream &results);
};

}  // namespace internal
}  // namespace kelpie

#endif  // KELPIE_IOMREGISTRY_HH
