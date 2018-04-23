// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 


#ifndef NNTI_VECTOR_HPP_
#define NNTI_VECTOR_HPP_

#include "nnti/nntiConfig.h"

#include <vector>
#include <sstream>
#include <string>

#include "nnti/nnti_threads.h"


namespace nnti {
namespace core {

template<class T>
class nnti_vector {
private:
    nthread_lock_t lock_;
    std::vector<T> vector_;
    uint32_t       lowest_avail_;

public:
    nnti_vector() : nnti_vector(256) {}
    nnti_vector(uint32_t initial_size)
    : vector_(initial_size, nullptr),
      lowest_avail_(0)
    {
        nthread_lock_init(&lock_);
    }
    virtual ~nnti_vector()
    {
        nthread_lock_fini(&lock_);
    }

    uint32_t
    add(
        T t)
    {
        uint32_t index=0;
        nthread_lock(&lock_);
        vector_[lowest_avail_] = t;
        index = lowest_avail_;

        // find a new lowest available index
        for (uint32_t i=lowest_avail_+1;i<vector_.size();i++) {
            if (vector_[i] == nullptr) {
                lowest_avail_ = i;
                break;
            }
        }
        if (lowest_avail_ == index) {
            // the vector is full.  expand by 50% if possible, otherwise expand to max_size().
            uint32_t old_size = vector_.size();
            // sanity check - if vector is already at max_size() then abort()
            if (old_size == vector_.max_size()) {
                log_fatal("nnti_vector", "this vector has reached max_size() and cannot be expanded.  Aborting...");
                abort();
            }
            if ((old_size * 1.5) < vector_.max_size()) {
                vector_.resize(old_size * 1.5, nullptr);
            } else {
                vector_.resize(vector_.max_size(), nullptr);
            }
            lowest_avail_ = old_size + 1;
        }
        nthread_unlock(&lock_);
        log_debug("nnti_vector", "add() t=%p index=%u", t, index);

        return index;
    }
    T
    remove(uint32_t index)
    {
        nthread_lock(&lock_);
        T t = vector_[index];
        vector_[index] = nullptr;
        if (index < lowest_avail_) {
            lowest_avail_ = index;
        }
        nthread_unlock(&lock_);
        log_debug("nnti_vector", "remove() t=%p", t);
        return(t);
    }
    T
    at(uint32_t index)
    {
        nthread_lock(&lock_);
        T t = vector_[index];
        nthread_unlock(&lock_);
        log_debug("nnti_vector", "at() t=%p", t);
        return(t);
    }

//    nnti_vector_iter_t
//    begin(void)
//    {
//        return vector_.begin();
//    }
//    nnti_vector_iter_t
//    end(void)
//    {
//        return vector_.end();
//    }
};


} /* namespace core */
} /* namespace nnti */

#endif /* NNTI_VECTOR_HPP_ */
