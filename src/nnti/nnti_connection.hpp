// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

/**
 * @file nnti_connection.hpp
 *
 * @brief nnti_connection.hpp
 *
 * @author Todd Kordenbrock (thkorde\@sandia.gov).
 * Created on: Jul 07, 2015
 */


#ifndef NNTI_CONNECTION_HPP_
#define NNTI_CONNECTION_HPP_

#include "nnti/nntiConfig.h"

#include <atomic>
#include <map>
#include <set>
#include <sstream>
#include <string>

#include "nnti/nnti_serialize.hpp"
#include "nnti/nnti_types.h"
#include "nnti/nnti_threads.h"
#include "nnti/nnti_peer.hpp"
#include "nnti/nnti_url.hpp"
#include "nnti/nnti_util.hpp"
#include "nnti/nnti_vector.hpp"

namespace nnti  {

namespace core {

class nnti_connection {
private:
    static std::atomic<uint32_t> next_id_;

protected:
    uint32_t                   id_;
    NNTI_process_id_t          peer_pid_;
    nnti::datatype::nnti_peer *peer_;

public:
    uint32_t index;

public:
    nnti_connection(void)
    : peer_pid_(0)
    {
        id_ = next_id_.fetch_add(1);
        return;
    }
    nnti_connection(
        const NNTI_process_id_t peer_pid)
    : peer_pid_(peer_pid)
    {
        id_       = next_id_.fetch_add(1);
        log_debug("nnti_connection", "peer_pid(%016lX)", peer_pid_);
        return;
    }
    nnti_connection(
        const std::string &peer_url)
    {
        id_       = next_id_.fetch_add(1);
        peer_pid_ = nnti::datatype::nnti_pid::to_pid(peer_url);
        log_debug("nnti_connection", "peer_pid(%016lX)", peer_pid_);
        return;
    }
    virtual ~nnti_connection()
    {
        return;
    }
    virtual uint32_t
    id()
    {
        return id_;
    }
    virtual const NNTI_process_id_t
    peer_pid()
    {
        return peer_pid_;
    }
    virtual void
    peer(
        nnti::datatype::nnti_peer *peer)
    {
        peer_ = peer;
        peer_pid_ = peer->pid();
    }
    virtual nnti::datatype::nnti_peer *
    peer() const
    {
        return peer_;
    }
    /*
     * generate a string that can be added into a URL query string
     */
    virtual std::string
    query_string(void)
    {
        std::stringstream ss;
        return ss.str();
    }
    /*
     * generate a key=value (one per line) string that can be included in a Whookie reply
     */
    virtual std::string
    reply_string(void)
    {
        std::stringstream ss;
        return ss.str();
    }
    virtual std::string
    toString()
    {
        std::stringstream out;
        out << "conn=" << (void*)this << " ; id_=" << id_ << " ; peer_pid_=" << peer_pid_ << " ; peer=" << (void*)peer();
        return out.str();
    }
};

typedef std::set<nnti_connection *>::iterator nnti_connection_map_iter_t;

class nnti_connection_map {
private:
    std::map<uint32_t, nnti_connection *>          id_map_;
    std::map<NNTI_process_id_t, nnti_connection *> pid_map_;
    std::set<nnti_connection *>                    conn_set_;

    nthread_lock_t lock_;

private:
    // There is no locking in here.  Please lock externally.
    nnti_connection *
    remove(
        const uint32_t id)
    {
        nnti_connection *conn=nullptr;
        if (id_map_.find(id) != id_map_.end()) {
            conn = id_map_[id];
        }
        if (conn != nullptr) {
            id_map_.erase(id);
        }
        return(conn);
    }
    // There is no locking in here.  Please lock externally.
    nnti_connection *
    remove(
        const NNTI_process_id_t pid)
    {
        nnti_connection *conn=nullptr;
        if (pid_map_.find(pid) != pid_map_.end()) {
            conn = pid_map_[pid];
        }
        if (conn != nullptr) {
            pid_map_.erase(pid);
        }
        return(conn);
    }
public:
    nnti_connection_map(void)
    {
        nthread_lock_init(&lock_);
    }
    virtual ~nnti_connection_map(void)
    {
        nthread_lock_fini(&lock_);
    }

    void
    insert(
        nnti_connection *conn)
    {
        log_debug("nnti_connection", "inserting conn==(%s) with pid key=%016lX", conn->toString().c_str(), conn->peer_pid());

        nthread_lock(&lock_);

        if (pid_map_.find(conn->peer_pid()) == pid_map_.end()) {
            pid_map_[conn->peer_pid()] = conn;

            assert (id_map_.find(conn->id()) == id_map_.end());
            id_map_[conn->id()] = conn;

            assert(conn_set_.find(conn) == conn_set_.end());
            conn_set_.insert(conn);
        }

        nthread_unlock(&lock_);

        log_debug("nnti_connection", "inserted conn==(%s) with pid key=%016lX", conn->toString().c_str(), conn->peer_pid());

        return;
    }

    nnti_connection *
    get(
        const uint32_t id)
    {
        nnti_connection *conn=nullptr;
        nthread_lock(&lock_);
        if (id_map_.find(id) != id_map_.end()) {
            conn = id_map_[id];
        }
        nthread_unlock(&lock_);
        return(conn);
    }
    nnti_connection *
    get(
        const NNTI_process_id_t pid)
    {
        nnti_connection *conn=nullptr;
        nthread_lock(&lock_);
        if (pid_map_.find(pid) != pid_map_.end()) {
            conn = pid_map_[pid];
        }
        nthread_unlock(&lock_);
        for(auto &k_v : pid_map_){
            log_debug("nnti_connection_map", "Key: %016lx   val: %p", k_v.first, k_v.second);
        }
        return(conn);
    }

    nnti_connection *
    remove(
        nnti_connection *conn)
    {
        nnti_connection_map_iter_t  set_iter;
        nnti_connection            *id_conn  = nullptr;
        nnti_connection            *pid_conn = nullptr;

        nthread_lock(&lock_);

        id_conn =remove(conn->id());
        pid_conn=remove(conn->peer_pid());

        if (id_conn != pid_conn) {
            // what!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        }

        set_iter=conn_set_.find(conn);
        conn_set_.erase(set_iter);

        nthread_unlock(&lock_);

        return(id_conn);
    }

    nnti_connection_map_iter_t
    begin(void)
    {
        return conn_set_.begin();
    }
    nnti_connection_map_iter_t
    end(void)
    {
        return conn_set_.end();
    }
};

typedef nnti_vector<nnti_connection *> nnti_connection_vector;

} /* namespace core */
} /* namespace nnti */

#endif /* NNTI_CONNECTION_HPP_ */
