// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

/**
 * @file transport_factory.hpp
 *
 * @brief transport_factory.hpp
 *
 * @author Todd Kordenbrock (thkorde\@sandia.gov).
 * Created on: August 03, 2015
 */

#ifndef TRANSPORT_FACTORY_HPP_
#define TRANSPORT_FACTORY_HPP_

#include <map>
#include <string>

#include "faodel-common/Configuration.hh"

#include "nnti/nnti_transport.hpp"


/** @addtogroup nnti_impl
 *  @{
 */

namespace nnti  {
namespace transports {

class factory {
private:
    static std::map<NNTI_transport_id_t, transport *> transport_map_;

private:
    factory()
    {
        return;
    }
    factory(
        const factory &)
    {
        return;
    }
    factory &
    operator=(
        const factory &)
    {
        return *this;
    }

public:
    ~factory();

    static transport *
    get_instance(
        const NNTI_transport_id_t &trans_id);
    static transport *
    get_instance(
        const std::string &trans_name);
    static transport *
    get_instance(
        const NNTI_transport_id_t &trans_id,
        faodel::Configuration    &config);
    static transport *
    get_instance(
        const std::string      &trans_name,
        faodel::Configuration &config);
    static transport *
    get_instance(
        faodel::Configuration &config);

    static bool
    exists(
        const NNTI_transport_id_t &trans_id);
};

} /* namespace transports */
} /* namespace nnti */


/** @} */

#endif /* TRANSPORT_FACTORY_HPP_*/
