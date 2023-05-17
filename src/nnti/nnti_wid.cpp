// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

/**
 * @file nnti_wid.cpp
 *
 * @brief nnti_wid.cpp
 *
 * @author Todd Kordenbrock (thkorde\@sandia.gov).
 * Created on: Oct 02, 2015
 */


#include "nnti/nnti_pch.hpp"

#include "nnti/nntiConfig.h"

#include <atomic>

#include <assert.h>

#include <atomic>
#include <deque>
#include <map>
#include <sstream>
#include <string>

#include "nnti/nnti_wid.hpp"

#include "nnti/nnti_types.h"
#include "nnti/nnti_datatype.hpp"
#include "nnti/nnti_wr.hpp"
#include "nnti/nnti_threads.h"
#include "nnti/nnti_serialize.hpp"
#include "nnti/nnti_util.hpp"



std::atomic<uint32_t> nnti::datatype::nnti_work_id::next_id_ = {1};

namespace nnti  {
namespace datatype {

nnti_work_id::nnti_work_id(
    nnti::transports::transport *transport)
: nnti_datatype(transport,
    NNTI_dt_work_id),
    wr_(transport),
    complete_(false)
{
    nthread_lock_init(&lock_);
    id_ = next_id_.fetch_add(1);
    return;
}
nnti_work_id::nnti_work_id(
    nnti::transports::transport *transport,
    NNTI_work_request_t         &wr)
: nnti_datatype(transport,
    NNTI_dt_work_id),
    wr_(transport,
        wr),
        complete_(false)
{
    nthread_lock_init(&lock_);
    id_ = next_id_.fetch_add(1);

    return;
}
nnti_work_id::nnti_work_id(
    nnti::datatype::nnti_work_request &wr)
: nnti_datatype(wr.transport(),
    NNTI_dt_work_id),
    wr_(wr),
    complete_(false)
{
    nthread_lock_init(&lock_);
    id_ = next_id_.fetch_add(1);

    return;
}

nnti_work_id::~nnti_work_id() {
    nthread_lock_fini(&lock_);
    return;
}

uint32_t
nnti_work_id::id(void) const
{
    return id_;
}
nnti_work_request&
nnti_work_id::wr(void)
{
    return wr_;
}

int
nnti_work_id::lock()
{
    return nthread_lock(&lock_);
}
int
nnti_work_id::unlock()
{
    return nthread_unlock(&lock_);
}

std::string
nnti_work_id::toString(void) const {
    std::stringstream out;
    out << "id_==" << id_;
    return out.str();
}
bool
nnti_work_id::is_complete(void) const
{
    return complete_;
}


nnti_work_id_queue::nnti_work_id_queue()
{
    nthread_lock_init(&lock_);
}
nnti_work_id_queue::~nnti_work_id_queue()
{
    nthread_lock_fini(&lock_);
}

void
nnti_work_id_queue::push(
    nnti_work_id *wr)
{
    nthread_lock(&lock_);
    queue_.push_back(wr);
    nthread_unlock(&lock_);
    log_debug("nnti_wr", "pushed wr=%p", wr);
}
nnti_work_id *
nnti_work_id_queue::pop()
{
    nthread_lock(&lock_);
    nnti_work_id *wr=queue_.front();
    queue_.pop_front();
    nthread_unlock(&lock_);
    log_debug("nnti_wr", "popped wr=%p", wr);
    return(wr);
}
nnti_work_id *
nnti_work_id_queue::front()
{
    nthread_lock(&lock_);
    nnti_work_id *wr=queue_.front();
    nthread_unlock(&lock_);
    log_debug("nnti_wr", "fronted wr=%p", wr);
    return(wr);
}

bool
nnti_work_id_queue::empty()
{
    nthread_lock(&lock_);
    bool rc=queue_.empty();
    nthread_unlock(&lock_);
    return(rc);
}

nnti_work_id *
nnti_work_id_queue::first_incomplete()
{
    nnti_work_id *wr=NULL;

    nthread_lock(&lock_);
    log_debug("nnti_wr", "wr queue_.size()==%d", queue_.size());
    if (!queue_.empty()) {
        std::deque<nnti_work_id *>::iterator i;
        for (i=queue_.begin(); i != queue_.end(); i++) {
            assert(*i);
            if (!(*i)->is_complete()) {
                wr=*i;
                break;
            }
        }
    }
    nthread_unlock(&lock_);
    log_debug("nnti_wr", "first incomplete wr=%p", wr);

    return(wr);
}

nnti_work_id_queue_iter_t
nnti_work_id_queue::begin()
{
    return queue_.begin();
}
nnti_work_id_queue_iter_t
nnti_work_id_queue::end()
{
    return queue_.end();
}


nnti_work_id_map::nnti_work_id_map()
{
    nthread_lock_init(&lock_);
}
nnti_work_id_map::~nnti_work_id_map()
{
    nthread_lock_fini(&lock_);
}

void
nnti_work_id_map::insert(
    nnti_work_id *wr)
{
    nthread_lock(&lock_);
    assert(map_.find(wr->id()) == map_.end());
    map_[wr->id()] = wr;
    nthread_unlock(&lock_);
    return;
}

nnti_work_id *
nnti_work_id_map::get(
    uint32_t id)
{
    nnti_work_id *wr=NULL;
    nthread_lock(&lock_);
    if (map_.find(id) != map_.end()) {
        wr = map_[id];
    }
    nthread_unlock(&lock_);
    return(wr);
}

nnti_work_id *
nnti_work_id_map::remove(
    nnti_work_id *wr)
{
    return(remove(wr->id()));
}
nnti_work_id *
nnti_work_id_map::remove(
    uint32_t id)
{
    nnti_work_id *wr=NULL;
    nthread_lock(&lock_);
    if (map_.find(id) != map_.end()) {
        wr = map_[id];
    }
    if (wr != NULL) {
        map_.erase(id);
    }
    nthread_unlock(&lock_);
    return(wr);
}

bool
nnti_work_id_map::empty()
{
    nthread_lock(&lock_);
    bool rc=map_.empty();
    nthread_unlock(&lock_);
    return(rc);
}

} /* namespace datatype */
} /* namespace nnti */
