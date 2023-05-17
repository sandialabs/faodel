// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.


#ifndef NNTI_OP_HPP_
#define NNTI_OP_HPP_

#include "nnti/nntiConfig.h"

#include <assert.h>

#include <atomic>
#include <map>
#include <vector>
#include <sstream>
#include <string>

#include "nnti/nnti_serialize.hpp"
#include "nnti/nnti_types.h"
#include "nnti/nnti_threads.h"
#include "nnti/nnti_util.hpp"
#include "nnti/nnti_vector.hpp"

#include "nnti/nnti_datatype.hpp"
#include "nnti/nnti_transport.hpp"
#include "nnti/nnti_wid.hpp"


namespace nnti {
namespace core {

class nnti_op {
private:

protected:
    static std::atomic<uint32_t>  next_id_;
    nthread_lock_t                lock_;
    uint32_t                      id_;
    nnti::datatype::nnti_work_id *wid_;

public:
    uint32_t index;

public:
    nnti_op(void)
    : wid_(nullptr)
    {
        nthread_lock_init(&lock_);
        id_ = next_id_.fetch_add(1);
        return;
    }
    nnti_op(
        nnti::datatype::nnti_work_id *wid)
    : wid_(wid)
    {
        nthread_lock_init(&lock_);
        id_ = next_id_.fetch_add(1);
        return;
    }
    virtual ~nnti_op()
    {
        nthread_lock_fini(&lock_);
        return;
    }
    virtual uint32_t
    id(void)
    {
        log_debug("nnti_op", "id_=%d", id_);
        return id_;
    }
    virtual  nnti::datatype::nnti_work_id *
    wid(void)
    {
        log_debug("nnti_op", "wid_=%x", wid_);
        return wid_;
    }
    virtual std::string
    toString(void)
    {
        std::stringstream out;
        out << "id_==" << id_;
        return out.str();
    }
};

typedef std::deque<nnti_op *>::iterator nnti_op_queue_iter_t;

class nnti_op_queue {
private:
    std::deque<nnti_op *> queue_;
    nthread_lock_t        lock_;

public:
    nnti_op_queue()
    {
        nthread_lock_init(&lock_);
    }
    virtual ~nnti_op_queue()
    {
        nthread_lock_fini(&lock_);
    }

    void
    push(
        nnti_op *op)
    {
        nthread_lock(&lock_);
        queue_.push_back(op);
        nthread_unlock(&lock_);
        log_debug("nnti_op", "pushed op=%p", op);
    }
    nnti_op *
    pop(void)
    {
        nthread_lock(&lock_);
        nnti_op *op=queue_.front();
        queue_.pop_front();
        nthread_unlock(&lock_);
        log_debug("nnti_op", "popped op=%p", op);
        return(op);
    }
    nnti_op *
    front(void)
    {
        nthread_lock(&lock_);
        nnti_op *op=queue_.front();
        nthread_unlock(&lock_);
        log_debug("nnti_op", "fronted op=%p", op);
        return(op);
    }

    bool
    empty(void)
    {
        nthread_lock(&lock_);
        bool rc=queue_.empty();
        nthread_unlock(&lock_);
        return(rc);
    }

    nnti_op_queue_iter_t
    begin(void)
    {
        return queue_.begin();
    }
    nnti_op_queue_iter_t
    end(void)
    {
        return queue_.end();
    }
};

class nnti_op_map {
public:
    nnti_op_map(void)
    {
        nthread_lock_init(&lock_);
    }
    virtual ~nnti_op_map()
    {
        nthread_lock_fini(&lock_);
    }

    void
    insert(
        nnti_op *op)
    {
        nthread_lock(&lock_);
        assert(id_map_.find(op->id()) == id_map_.end());
        id_map_[op->id()] = op;
        nthread_unlock(&lock_);
        return;
    }

    nnti_op *
    get(
        uint32_t id)
    {
        nnti_op *op=NULL;
        nthread_lock(&lock_);
        if (id_map_.find(id) != id_map_.end()) {
            op = id_map_[id];
        }
        nthread_unlock(&lock_);
        return(op);
    }

    nnti_op *
    remove(
        nnti_op *op)
    {
        return(remove(op->id()));
    }
    nnti_op *
    remove(
        uint32_t id)
    {
        nnti_op *op=nullptr;
        nthread_lock(&lock_);
        if (id_map_.find(id) != id_map_.end()) {
            op = id_map_[id];
        }
        if (op != nullptr) {
            id_map_.erase(id);
        }
        nthread_unlock(&lock_);
        return(op);
    }

    bool
    empty(void)
    {
        nthread_lock(&lock_);
        bool rc=id_map_.empty();
        nthread_unlock(&lock_);
        return(rc);
    }

private:
    std::map<uint32_t, nnti_op *>           id_map_;

    nthread_lock_t lock_;
};

typedef nnti_vector<nnti_op *> nnti_op_vector;

} /* namespace core */
} /* namespace nnti */

#endif /* NNTI_OP_HPP_ */
