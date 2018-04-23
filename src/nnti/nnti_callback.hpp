// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

/**
 * @file nnti_callback.hpp
 *
 * @brief nnti_callback.hpp
 *
 * @author Todd Kordenbrock (thkorde\@sandia.gov).
 * Created on: Aug 24, 2015
 */


#ifndef NNTI_CALLBACK_HPP
#define NNTI_CALLBACK_HPP

#include "nnti/nntiConfig.h"

#include <assert.h>

#include <functional>
#include <sstream>
#include <string>

#include "nnti/nnti_packable.h"
#include "nnti/nnti_types.h"

#include "nnti/nnti_datatype.hpp"


namespace nnti  {
namespace datatype {

class default_event_callback {
public:
    NNTI_result_t operator() (NNTI_event_t *event, void *context) {
        // this default callback returns !NNTI_OK so the event will pushed into the EQ
        return NNTI_EIO;
    }
};

class nnti_event_callback
: public nnti_datatype {
private:
    std::function< NNTI_result_t(NNTI_event_t*,void*) > cb_;

public:
    nnti_event_callback()
        : cb_(default_event_callback()),
          nnti_datatype(NNTI_dt_callback)
    {
        return;
    }
    nnti_event_callback(
        nnti::datatype::nnti_event_callback &other)
        : cb_(other.cb_),
          nnti_datatype(other.transport_,
                        other.datatype_)
    {
        return;
    }
    nnti_event_callback(
        nnti::transports::transport *transport)
        : cb_(default_event_callback()),
          nnti_datatype(transport,
                        NNTI_dt_callback)
    {
        return;
    }
    nnti_event_callback(
        nnti::transports::transport *transport,
        NNTI_event_callback_t        cb)
        : cb_(cb),
          nnti_datatype(transport,
                        NNTI_dt_callback)
    {
        if (!cb_) {
            cb_  = default_event_callback();
        }
        return;
    }
    nnti_event_callback(
        nnti::transports::transport                         *transport,
        std::function< NNTI_result_t(NNTI_event_t*,void*) >  cb)
        : cb_(cb),
          nnti_datatype(transport,
                        NNTI_dt_callback)
    {
        if (!cb_) {
            cb_  = default_event_callback();
        }
        return;
    }
    virtual ~nnti_event_callback()
    {
        return;
    }

    explicit operator bool() const
    {
        return cb_.operator bool();
    }

    NNTI_result_t
    invoke(NNTI_event_t *event, void *context) const
    {
        return cb_(event, context);
    }

    virtual std::string
    toString()
    {
        std::stringstream out;
        out << "cb_==" << &cb_;
        return out.str();
    }

};

} /* namespace datatype */
} /* namespace nnti */

#endif /* NNTI_CALLBACK_HPP */
