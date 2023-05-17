// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

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

#include <functional>

#include "nnti/nnti_types.h"
#include "nnti/nnti_datatype.hpp"


namespace nnti  {
namespace datatype {

class nnti_event_callback
: public nnti_datatype {
private:
    std::function< NNTI_result_t(NNTI_event_t*,void*) > cb_;

public:
    nnti_event_callback();
    nnti_event_callback(
        nnti::datatype::nnti_event_callback &other);
    nnti_event_callback(
        nnti::transports::transport *transport);
    nnti_event_callback(
        nnti::transports::transport *transport,
        NNTI_event_callback_t        cb);
    nnti_event_callback(
        nnti::transports::transport                         *transport,
        std::function< NNTI_result_t(NNTI_event_t*,void*) >  cb);

    ~nnti_event_callback() override;

    explicit operator bool() const;

    NNTI_result_t
    invoke(NNTI_event_t *event, void *context) const;

    virtual std::string
    toString() const override;

};

} /* namespace datatype */
} /* namespace nnti */

#endif /* NNTI_CALLBACK_HPP */
