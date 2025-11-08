// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef OPBOX_OPARGSOBJECTAVAILABLE_HH
#define OPBOX_OPARGSOBJECTAVAILABLE_HH

#include "opbox/common/Types.hh"
#include "opbox/common/OpArgs.hh"
#include "lunasa/DataObject.hh"
#include "kelpie/Kelpie.hh"
#include "kelpie/common/Types.hh"

namespace kelpie {

/**
 * @brief OpBox arguments that provide info about a requested item
 */
class OpArgsObjectAvailable : public opbox::OpArgs {

public:
  OpArgsObjectAvailable(lunasa::DataObject ldo, object_info_t info)
    : OpArgs(opbox::UpdateType::user_trigger),
      ldo(std::move(ldo)),
      info(info) {
  }

  ~OpArgsObjectAvailable() override = default;

  //InfoInterface
  void sstr(std::stringstream &ss, int depth=0, int indent=0) const override;

  //ObjectAvailable-specific Variables
  lunasa::DataObject ldo;
  object_info_t info;

};


}  // namespace kelpie

#endif  // OPBOX_OPARGSOBJECTAVAILABLE_HH
