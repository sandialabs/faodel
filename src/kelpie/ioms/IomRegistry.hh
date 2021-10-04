// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef KELPIE_IOMREGISTRY_HH
#define KELPIE_IOMREGISTRY_HH

#include <map>
#include <string>

#include "faodel-common/InfoInterface.hh"
#include "faodel-common/LoggingInterface.hh"
#include "faodel-common/StringHelpers.hh"
#include "faodel-common/ResourceURL.hh"

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
  : public faodel::InfoInterface,
    public faodel::LoggingInterface {

public:
  IomRegistry();
  ~IomRegistry() override;

  void init(const faodel::Configuration &config);
  void start() { finalized=true; }
  void finish();

  void RegisterIom(std::string type, std::string name,  const std::map<std::string,std::string> &settings);
  int  RegisterIomFromURL(const faodel::ResourceURL &url);

  void RegisterIomConstructor(std::string type, fn_IomConstructor_t ctor_function, fn_IomGetValidSetting_t valid_settings_function=nullptr);

  IomBase * Find(std::string iom_name) { return Find(faodel::hash32(iom_name)); }
  IomBase * Find(iom_hash_t iom_hash);

  std::vector<std::string> GetIomNames() const;

  std::vector<std::string> RegisteredTypes() const;
  std::vector<std::pair<std::string,std::string>> RegisteredTypeParameters(const std::string &type) const;

  //InfoInterface function
  void sstr(std::stringstream &ss, int depth=0, int indent=0) const override;
  
private:
  faodel::MutexWrapper *mutex;
  int default_logging_level;
  bool finalized;
  std::map<iom_hash_t, IomBase *> ioms_by_hash_pre;
  std::map<iom_hash_t, IomBase *> ioms_by_hash_post;
  
  std::map<std::string, fn_IomConstructor_t> iom_ctors;
  std::map<std::string, fn_IomGetValidSetting_t> iom_valid_setting_fns;

  void HandleWhookieStatus(const std::map<std::string, std::string> &args, std::stringstream &results);
};

}  // namespace internal
}  // namespace kelpie

#endif  // KELPIE_IOMREGISTRY_HH
