// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

/**
 * @file nnti_freelist.hpp
 *
 * @brief nnti_freelist.hpp
 *
 * @author Todd Kordenbrock (thkorde\@sandia.gov).
 * Created on: Jul 07, 2015
 */


#ifndef NNTI_FREELIST_HPP
#define NNTI_FREELIST_HPP

#include "nnti/nntiConfig.h"

#include <fcntl.h>

#include <atomic>

#include <boost/lockfree/stack.hpp>
#include <boost/lockfree/queue.hpp>

#include "nnti/nnti_serialize.hpp"
#include "nnti/nnti_types.h"

#include "nnti/nnti_datatype.hpp"


namespace nnti  {
namespace core {

template <typename _Tp>
class nnti_freelist {
private:
    const uint64_t                                                   stack_size_;
    boost::lockfree::stack<_Tp, boost::lockfree::fixed_sized<true>>  stack_;

public:
    nnti_freelist(
        uint64_t size)
        : stack_size_(size),
          stack_(size)
    {
        return;
    }
    virtual ~nnti_freelist()
    {
    }

    bool
    push(
        _Tp &t)
    {
        log_debug("nnti_freelist", "pushing (stack_=%x)", &stack_);
        return stack_.push(t);
    }

    bool
    pop(
        _Tp &t)
    {
        _Tp tmp;
        if (stack_.pop(tmp)) {
            t = tmp;
            log_debug("nnti_freelist", "pop success (stack_=%x)", &stack_);
            return true;
        }
        log_debug("nnti_freelist", "pop fail (stack_=%x)", &stack_);
        return false;
    }

    bool
    empty(void)
    {
        return stack_.empty();
    }
};

} /* namespace datatype */
} /* namespace nnti */

#endif /* NNTI_FREELIST_HPP */
