// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

/**
 * @file nnti_eq.hpp
 *
 * @brief nnti_eq.hpp
 *
 * @author Todd Kordenbrock (thkorde\@sandia.gov).
 * Created on: Jul 07, 2015
 */


#ifndef NNTI_EQ_HPP
#define NNTI_EQ_HPP

#include "nnti/nntiConfig.h"

#include <fcntl.h>

#include <atomic>

#include <boost/lockfree/stack.hpp>
#include <boost/lockfree/queue.hpp>

#include "nnti/nnti_packable.h"
#include "nnti/nnti_types.h"

#include "nnti/nnti_datatype.hpp"
#include "nnti/nnti_callback.hpp"


namespace nnti  {
namespace datatype {

typedef int64_t reservation;

class default_eq_callback {
public:
    NNTI_result_t operator() (NNTI_event_t *event, void *context) {
        // this default callback returns !NNTI_OK so the event will pushed into the EQ
        return NNTI_EIO;
    }
};

class nnti_event_queue
: public nnti_datatype {
private:
    class reservation_manager {
    public:
        virtual bool get_reservation(nnti::datatype::reservation &r)    = 0;
        virtual bool return_reservation(void)                           = 0;
        virtual bool return_reservation(nnti::datatype::reservation &r) = 0;
    };
    class simple_reservation_manager
    : public reservation_manager {
    private:
        const uint64_t                           max_reservations_;
        std::atomic<nnti::datatype::reservation> outstanding_reservations_;
    public:
        simple_reservation_manager(
            const uint64_t max_reservations)
            : max_reservations_(max_reservations)
        {
            outstanding_reservations_ = {0};
            return;
        }
        bool
        get_reservation(
            nnti::datatype::reservation &r)
        {
            r = outstanding_reservations_.fetch_add(1);
            if (r >= max_reservations_) {
                outstanding_reservations_.fetch_add(-1);
                r = -1;
                return(false);
            }
            return true;
        }
        bool
        return_reservation()
        {
            outstanding_reservations_.fetch_add(-1);
            return true;
        }
        bool
        return_reservation(
            nnti::datatype::reservation &r)
        {
            return return_reservation();
        }

    };
    class empty_reservation_manager
    : public reservation_manager {
    public:
        bool
        get_reservation(
            nnti::datatype::reservation &r)
        {
            return true;
        }
        bool
        return_reservation()
        {
            return true;
        }
        bool
        return_reservation(
            nnti::datatype::reservation &r)
        {
            return true;
        }
    };

private:
    bool                                                                        require_reservation_;
    reservation_manager                                                        *reservation_manager_;
    int                                                                         notification_pipe_[2];
    const uint64_t                                                              q_size_;
    boost::lockfree::queue<NNTI_event_t *, boost::lockfree::fixed_sized<true>>  q_;
    nnti::datatype::nnti_event_callback                                         cb_;
    void                                                                       *cb_context_;

    NNTI_result_t
    setup_notification_pipe(void)
    {
        int rc;
        int flags;

        rc=pipe(notification_pipe_);
        if (rc < 0) {
            log_error("nnti_event_queue", "pipe() failed: %s", strerror(errno));
            return NNTI_EIO;
        }

        /* use non-blocking IO on the read side of the interrupt pipe */
        flags = fcntl(notification_pipe_[0], F_GETFL);
        if (flags < 0) {
            log_error("nnti_event_queue", "failed to get interrupt_pipe flags: %s", strerror(errno));
            return NNTI_EIO;
        }
        if (fcntl(notification_pipe_[0], F_SETFL, flags | O_NONBLOCK) < 0) {
            log_error("nnti_event_queue", "failed to set interrupt_pipe to nonblocking: %s", strerror(errno));
            return NNTI_EIO;
        }

        /* use non-blocking IO on the write side of the interrupt pipe */
        flags = fcntl(notification_pipe_[1], F_GETFL);
        if (flags < 0) {
            log_error("nnti_event_queue", "failed to get interrupt_pipe flags: %s", strerror(errno));
            return NNTI_EIO;
        }
        if (fcntl(notification_pipe_[1], F_SETFL, flags | O_NONBLOCK) < 0) {
            log_error("nnti_event_queue", "failed to set interrupt_pipe to nonblocking: %s", strerror(errno));
            return NNTI_EIO;
        }
    }

public:
    nnti_event_queue(
        bool     require_reservation,
        uint64_t size)
        : require_reservation_(require_reservation),
          q_size_(size),
          q_(size),
          nnti_datatype(NNTI_dt_event_queue)
    {
        if (require_reservation) {
            reservation_manager_ = new simple_reservation_manager(q_size_);
        } else {
            reservation_manager_ = new empty_reservation_manager();
        }

        setup_notification_pipe();

        return;
    }
    nnti_event_queue(
        bool                         require_reservation,
        uint64_t                     size,
        nnti::transports::transport *transport)
        : require_reservation_(require_reservation),
          q_size_(size),
          q_(size),
          cb_(transport, default_eq_callback()),
          cb_context_(nullptr),
          nnti_datatype(transport,
                        NNTI_dt_event_queue)
    {
        if (require_reservation) {
            reservation_manager_ = new simple_reservation_manager(q_size_);
        } else {
            reservation_manager_ = new empty_reservation_manager();
        }

        setup_notification_pipe();

        return;
    }
    nnti_event_queue(
        bool                                 require_reservation,
        uint64_t                             size,
        nnti::datatype::nnti_event_callback  cb,
        void                                *cb_context,
        nnti::transports::transport         *transport)
        : require_reservation_(require_reservation),
          q_size_(size),
          q_(size),
          cb_(cb),
          cb_context_(cb_context),
          nnti_datatype(transport,
                        NNTI_dt_event_queue)
    {
        if (require_reservation) {
            reservation_manager_ = new simple_reservation_manager(q_size_);
        } else {
            reservation_manager_ = new empty_reservation_manager();
        }

        setup_notification_pipe();

        return;
    }
    virtual ~nnti_event_queue()
    {
        delete reservation_manager_;
    }

    bool
    requires_reservation()
    {
        return require_reservation_;
    }

    bool
    get_reservation(
        nnti::datatype::reservation &r)
    {
        return reservation_manager_->get_reservation(r);
    }

    bool
    return_reservation()
    {
        return reservation_manager_->return_reservation();
    }

    bool
    return_reservation(
        nnti::datatype::reservation &r)
    {
        return reservation_manager_->return_reservation(r);
    }

    bool
    push(
        NNTI_event_t *&e)
    {
        return q_.push(e);
    }

    bool
    push(
        nnti::datatype::reservation &r, NNTI_event_t *&e)
    {
        return q_.push(e);
    }

    bool
    pop(
        NNTI_event_t *&e)
    {
        NNTI_event_t *event;
        if (q_.pop(event)) {
            e = event;
            reservation_manager_->return_reservation();
            return true;
        }
        return false;
    }

    nnti_event_callback
    callback(void)
    {
        return cb_;
    }
    void *
    cb_context(void)
    {
        return cb_context_;
    }
    NNTI_result_t invoke_cb(NNTI_event_t *event) {
        log_debug("nnti_event_queue", "invoking the EQ callback");
        return cb_.invoke(event, cb_context_);
    }

    void
    notify(void)
    {
        uint32_t dummy=0xAAAAAAAA;
        write(notification_pipe_[1], &dummy, 4);
    }
    int
    read_fd(void)
    {
        return notification_pipe_[0];
    }



    static inline nnti_event_queue*
    to_obj(
        NNTI_event_queue_t eq_hdl)
    {
        return (nnti_event_queue *)eq_hdl;
    }

    static inline NNTI_event_queue_t
    to_hdl(
        nnti_event_queue *eq)
    {
        return (NNTI_event_queue_t)eq;
    }
};

} /* namespace datatype */
} /* namespace nnti */

#endif /* NNTI_EQ_HPP */
