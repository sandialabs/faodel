// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef FAODEL_COMMON_INFOINTERFACE_HH
#define FAODEL_COMMON_INFOINTERFACE_HH

#include <sstream>
#include <string>

namespace faodel {

/**
 * @brief A simple interface for returning status info about FAODEL components
 *
 * This provides a standard stringstream interface for dumping info about
 * a component.
 */
class InfoInterface {

public:
  virtual void sstr(std::stringstream &ss, int depth=0, int indent=0) const = 0;
  virtual ~InfoInterface() = default;

  /**
   * @brief Get debug info about this object
   * @param[in] depth How many more steps in hierarchy to go down (default=0)
   * @param[in] indent How many spaces to put in front of this line (default=0)
   * @returns string with info
   */
  std::string str(int depth=0, int indent=0) const {
    std::stringstream ss;
    sstr(ss,depth,indent);
    return ss.str();
  }

};


} // namespace faodel

#endif // FAODEL_COMMON_INFOINTERFACE_HH
