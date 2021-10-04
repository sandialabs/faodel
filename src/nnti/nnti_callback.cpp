// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.


#include "nnti/nntiConfig.h"

#include <assert.h>

#include <functional>
#include <sstream>
#include <string>

#include "nnti/nnti_callback.hpp"
#include "nnti/nnti_serialize.hpp"


namespace nnti  {
namespace datatype {

class default_event_callback {
public:
    NNTI_result_t operator() (NNTI_event_t *event, void *context) {
        // this default callback returns !NNTI_OK so the event will pushed into the EQ
        return NNTI_EIO;
    }
};

nnti_event_callback::nnti_event_callback()
: nnti_datatype(NNTI_dt_callback),
  cb_(default_event_callback())
{
    return;
}
nnti_event_callback::nnti_event_callback(
    nnti::datatype::nnti_event_callback &other)
: nnti_datatype(other.transport_, other.datatype_),
  cb_(other.cb_)
{
    return;
}
nnti_event_callback::nnti_event_callback(
    nnti::transports::transport *transport)
: nnti_datatype(transport, NNTI_dt_callback),
  cb_(default_event_callback())
{
    return;
}
nnti_event_callback::nnti_event_callback(
    nnti::transports::transport *transport,
    NNTI_event_callback_t        cb)
: nnti_datatype(transport, NNTI_dt_callback),
  cb_(cb)
{
    if (!cb_) {
        cb_  = default_event_callback();
    }
    return;
}
nnti_event_callback::nnti_event_callback(
    nnti::transports::transport                         *transport,
    std::function< NNTI_result_t(NNTI_event_t*,void*) >  cb)
: nnti_datatype(transport, NNTI_dt_callback),
  cb_(cb)
{
    if (!cb_) {
        cb_  = default_event_callback();
    }
    return;
}

nnti_event_callback::~nnti_event_callback()
{
    return;
}

nnti_event_callback::operator bool() const
{
    return cb_.operator bool();
}

NNTI_result_t
nnti_event_callback::invoke(
    NNTI_event_t *event, void *context) const
{
    return cb_(event, context);
}

std::string
nnti_event_callback::toString() const {
    std::stringstream out;
    out << "cb_==" << &cb_;
    return out.str();
}

} /* namespace datatype */
} /* namespace nnti */
