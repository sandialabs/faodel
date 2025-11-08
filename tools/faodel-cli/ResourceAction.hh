// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef FAODEL_RESOURCEACTION_HH
#define FAODEL_RESOURCEACTION_HH

#include <string>
#include <vector>

#include "ActionInterface.hh"


class ResourceAction
        : public ActionInterface {

public:
  ResourceAction() = default;
  ResourceAction(const std::string &long_or_short_cmd);

  std::vector<std::string> rargs;

  faodel::rc_t ParseArgs(const std::vector<std::string> &args);

};


#endif //FAODEL_RESOURCEACTION_HH
