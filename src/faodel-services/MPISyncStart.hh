// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef FAODEL_MPISYNCHSTART_HH
#define FAODEL_MPISYNCHSTART_HH

#include <string>

#include "faodel-common/BootstrapInterface.hh"
#include "faodel-common/LoggingInterface.hh"

namespace faodel {
namespace mpisyncstart {

std::string bootstrap();


class MPISyncStart
        : public faodel::bootstrap::BootstrapInterface,
          public faodel::LoggingInterface {

public:
  MPISyncStart();
  ~MPISyncStart() override;

  //Bootstrap API
  void Init(const faodel::Configuration &config) override {}
  void InitAndModifyConfiguration(faodel::Configuration *config) override;
  void Start() override;
  void Finish() override {};
  void GetBootstrapDependencies(std::string &name,
                       std::vector<std::string> &requires,
                       std::vector<std::string> &optional) const override;

private:
  bool needs_patch; //True when detected a change in config
};

} // namespace mpisyncstart
} // namespace faodel



#endif //FAODEL_MPISYNCHSTART_HH
