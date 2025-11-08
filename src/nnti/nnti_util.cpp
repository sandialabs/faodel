// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

/*
 *  @file: nnti_util.hpp
 *
 *  Created on: Jul 13, 2015
 *      Author: thkorde
 */

#include "nnti/nntiConfig.h"

#include "nnti/nnti_util.hpp"

uint32_t
nnti::util::str2uint32(const std::string &s)
{
    uint32_t i = 0;

    try {
        i = boost::lexical_cast<uint32_t>(s);
    }
    catch (boost::bad_lexical_cast &) {
        // i is already 0.  do nothing.
        // i = 0;
    }

    return i;
}
uint64_t
nnti::util::str2uint64(const std::string &s)
{
    uint64_t i = 0;

    try {
        i = boost::lexical_cast<uint64_t>(s);
    }
    catch (boost::bad_lexical_cast &) {
        // i is already 0.  do nothing.
        // i = 0;
    }

    return i;
}

int32_t
nnti::util::str2int32(const std::string &s)
{
    int32_t i = 0;

    try {
        i = boost::lexical_cast<int32_t>(s);
    }
    catch (boost::bad_lexical_cast &) {
        // i is already 0.  do nothing.
        // i = 0;
    }

    return i;
}
int64_t
nnti::util::str2int64(const std::string &s)
{
    int64_t i = 0;

    try {
        i = boost::lexical_cast<int64_t>(s);
    }
    catch (boost::bad_lexical_cast &) {
        // i is already 0.  do nothing.
        // i = 0;
    }

    return i;
}
