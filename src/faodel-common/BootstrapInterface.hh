// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef FAODEL_COMMON_BOOTSTRAPINTERFACE_HH
#define FAODEL_COMMON_BOOTSTRAPINTERFACE_HH

#include <string>
#include <vector>

//Forward references
namespace faodel { class Configuration; }


namespace faodel {
namespace bootstrap {

/**
 * @brief Interface for auto starting/stopping FAODEL components with dependencies
 *
 * Bootstrap is used to control the order in which different FAODEL components
 * are started/stopped. Add this interface to a component that you want to
 * include in the boot process.
 */
class BootstrapInterface {
public:

  virtual void Init(const Configuration &config) = 0;
  virtual void InitAndModifyConfiguration(Configuration *config) { Init(*config); };

  virtual void Start() = 0;
  virtual void Finish() = 0;
  virtual void GetBootstrapDependencies(std::string &name,
                       std::vector<std::string> &requires,
                       std::vector<std::string> &optional) const = 0;

  virtual ~BootstrapInterface() = default;

};

} // namespace bootstrap
} // namespace faodel

#endif // FAODEL_COMMON_BOOTSTRAPINTERFACE_HH
