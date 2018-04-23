// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

/**
 * @file nnti_datatype.hpp
 *
 * @brief nnti_datatype.hpp
 *
 * @author Todd Kordenbrock (thkorde\@sandia.gov).
 * Created on: Jul 07, 2015
 */


#ifndef NNTI_DATATYPE_HPP
#define NNTI_DATATYPE_HPP

#include "nnti/nntiConfig.h"

#include <assert.h>

#include <sstream>
#include <string>

#include "nnti/nnti_transport.hpp"


namespace nnti  {
namespace datatype {

class nnti_datatype {
protected:
    nnti::transports::transport *transport_;
    NNTI_datatype_t              datatype_;
public:
    nnti_datatype()
    {
        return;
    }
    nnti_datatype(
        NNTI_datatype_t datatype)
        : datatype_(datatype)
    {
        return;
    }
    nnti_datatype(
        nnti::transports::transport *transport,
        NNTI_datatype_t              datatype)
        : transport_(transport),
          datatype_(datatype)
    {
        return;
    }
    virtual ~nnti_datatype()
    {
        return;
    }

    nnti::transports::transport *
    transport()
    {
        return transport_;
    }
    NNTI_datatype_t
    datatype()
    {
        return datatype_;
    }

    virtual std::string
    toString()
    {
        std::stringstream out;
        out << "datatype_==" << datatype_;
        return out.str();
    }
};

} /* namespace datatype */
} /* namespace nnti */

#endif /* NNTI_DATATYPE_HPP */
