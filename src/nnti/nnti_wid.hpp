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

#include <atomic>
#include <deque>
#include <map>

#include "nnti/nnti_types.h"
#include "nnti/nnti_datatype.hpp"
#include "nnti/nnti_wr.hpp"
#include "nnti/nnti_threads.h"


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
        nnti::transports::transport *transport);
    nnti_work_id(
        nnti::transports::transport *transport,
        NNTI_work_request_t         &wr);
    nnti_work_id(
        nnti::datatype::nnti_work_request &wr);

    ~nnti_work_id();

    virtual uint32_t
    id(void) const;
    virtual nnti_work_request&
    wr(void);

    virtual int
    lock();
    virtual int
    unlock();

    std::string
    toString(void) override;
    virtual bool
    is_complete(void) const;
};

typedef std::deque<nnti_work_id *>::iterator nnti_work_id_queue_iter_t;

class nnti_work_id_queue {
private:
    std::deque<nnti_work_id *> queue_;
    nthread_lock_t             lock_;

public:
    nnti_work_id_queue();
    virtual ~nnti_work_id_queue();

    void
    push(
        nnti_work_id *wr);
    nnti_work_id *
    pop();
    nnti_work_id *
    front();

    bool
    empty();

    nnti_work_id *
    first_incomplete();

    nnti_work_id_queue_iter_t
    begin();
    nnti_work_id_queue_iter_t
    end();
};

class nnti_work_id_map {
private:
    std::map<uint32_t, nnti_work_id *>           map_;
    std::map<uint32_t, nnti_work_id *>::iterator map_iter_;

    nthread_lock_t lock_;

public:
    nnti_work_id_map();
    virtual ~nnti_work_id_map();

    void
    insert(
        nnti_work_id *wr);

    nnti_work_id *
    get(
        uint32_t id);

    nnti_work_id *
    remove(
        nnti_work_id *wr);
    nnti_work_id *
    remove(
        uint32_t id);

    bool
    empty();
};

} /* namespace datatype */
} /* namespace nnti */

#endif /* NNTI_WID_HPP */
