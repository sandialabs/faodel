// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

/**
 * @file nnti_wid.hpp
 *
 * @brief nnti_wid.hpp
 *
 * @author Todd Kordenbrock (thkorde\@sandia.gov).
 * Created on: Jul 07, 2015
 */


#ifndef NNTI_WID_HPP
#define NNTI_WID_HPP

#include "nnti/nntiConfig.h"

#include <assert.h>

#include <atomic>
#include <deque>
#include <map>
#include <sstream>
#include <string>

#include "nnti/nnti_types.h"

#include "nnti/nnti_datatype.hpp"
#include "nnti/nnti_wr.hpp"

#include "nnti/nnti_threads.h"
#include "nnti/nnti_util.hpp"


namespace nnti  {
namespace datatype {

class nnti_work_id
: public nnti_datatype {
private:
    static std::atomic<uint32_t> next_id_;

    uint32_t          id_;
    nthread_lock_t    lock_;
    nnti_work_request wr_; // a copy of the work request that generated this work ID
    bool              complete_;

private:
    nnti_work_id(void);

public:
    nnti_work_id(
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
    nnti_work_id(
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
    nnti_work_id(
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

  ~nnti_work_id() override {
        nthread_lock_fini(&lock_);
        return;
    }

    virtual uint32_t
    id(void) const
    {
        return id_;
    }
    virtual nnti_work_request&
    wr(void)
    {
        return wr_;
    }

    virtual int
    lock()
    {
        return nthread_lock(&lock_);
    }
    virtual int
    unlock()
    {
        return nthread_unlock(&lock_);
    }

  std::string
    toString(void) override {
        std::stringstream out;
        out << "id_==" << id_;
        return out.str();
    }
    virtual bool
    is_complete(void) const
    {
        return complete_;
    }
};

typedef std::deque<nnti_work_id *>::iterator nnti_work_id_queue_iter_t;

class nnti_work_id_queue {
private:
    std::deque<nnti_work_id *> queue_;
    nthread_lock_t             lock_;

public:
    nnti_work_id_queue()
    {
        nthread_lock_init(&lock_);
    }
    virtual ~nnti_work_id_queue()
    {
        nthread_lock_fini(&lock_);
    }

    void
    push(
        nnti_work_id *wr)
    {
        nthread_lock(&lock_);
        queue_.push_back(wr);
        nthread_unlock(&lock_);
        log_debug("nnti_wr", "pushed wr=%p", wr);
    }
    nnti_work_id *
    pop()
    {
        nthread_lock(&lock_);
        nnti_work_id *wr=queue_.front();
        queue_.pop_front();
        nthread_unlock(&lock_);
        log_debug("nnti_wr", "popped wr=%p", wr);
        return(wr);
    }
    nnti_work_id *
    front()
    {
        nthread_lock(&lock_);
        nnti_work_id *wr=queue_.front();
        nthread_unlock(&lock_);
        log_debug("nnti_wr", "fronted wr=%p", wr);
        return(wr);
    }

    bool
    empty()
    {
        nthread_lock(&lock_);
        bool rc=queue_.empty();
        nthread_unlock(&lock_);
        return(rc);
    }

    nnti_work_id *
    first_incomplete()
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
    begin()
    {
        return queue_.begin();
    }
    nnti_work_id_queue_iter_t
    end()
    {
        return queue_.end();
    }
};

class nnti_work_id_map {
private:
    std::map<uint32_t, nnti_work_id *>           map_;
    std::map<uint32_t, nnti_work_id *>::iterator map_iter_;

    nthread_lock_t lock_;

public:
    nnti_work_id_map()
{
        nthread_lock_init(&lock_);
    }
    virtual ~nnti_work_id_map()
    {
        nthread_lock_fini(&lock_);
    }

    void
    insert(
        nnti_work_id *wr)
    {
        nthread_lock(&lock_);
        assert(map_.find(wr->id()) == map_.end());
        map_[wr->id()] = wr;
        nthread_unlock(&lock_);
        return;
    }

    nnti_work_id *
    get(
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
    remove(
        nnti_work_id *wr)
    {
        return(remove(wr->id()));
    }
    nnti_work_id *
    remove(
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
    empty()
    {
        nthread_lock(&lock_);
        bool rc=map_.empty();
        nthread_unlock(&lock_);
        return(rc);
    }
};

} /* namespace datatype */
} /* namespace nnti */

#endif /* NNTI_WID_HPP */
