// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 


#ifndef NNTI_PID_HPP
#define NNTI_PID_HPP

#include "nnti/nntiConfig.h"

#include <string>

#include "nnti/nnti_packable.h"
#include "nnti/nnti_types.h"

namespace nnti  {

namespace core {
    // forward declaration of friend
    class nnti_url;
}

namespace datatype {

class nnti_pid {
private:
    nnti_pid() { }
    virtual ~nnti_pid() { }

public:
    static NNTI_process_id_t
    to_pid(
        const nnti::core::nnti_url &url);

    static NNTI_process_id_t
    to_pid(
        const std::string &url);

    static std::string
    to_url(
        NNTI_process_id_t pid);
};

} /* namespace datatype */
} /* namespace nnti */

#endif /* NNTI_PID_HPP */
