// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#ifndef FAODEL_COMMON_TYPES_HH
#define FAODEL_COMMON_TYPES_HH

#include <cstdint>

namespace faodel {

using rc_t = int; //!< Mark integer return codes with rc_t for more clarity


/** @brief A marking used to designate API calls that must be visible, but are not expected to be used by end users */
struct internal_use_only_t {}; static constexpr internal_use_only_t internal_use_only = {};

} // namespace faodel

#endif // FAODEL_COMMON_TYPES_HH

