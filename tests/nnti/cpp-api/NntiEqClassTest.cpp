// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.


#include "nnti/nnti_pch.hpp"

#include "nnti/nntiConfig.h"

#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include <iostream>
#include <thread>

#include "nnti/nnti_logger.hpp"

#include "nnti/nnti_eq.hpp"


static const uint32_t num_consumers       = 3;
static const uint32_t num_producers       = 1;
static const uint64_t queue_size          = 32;
static const uint32_t events_per_producer = queue_size * 1024;
static const uint32_t total_events        = events_per_producer * num_producers;

static const uint64_t NOT_PRODUCED = 0xA;  // this event has not been pushed onto the EQ
static const uint64_t NOT_CONSUMED = 0xB;  // this event has not been popped off the EQ
static const uint64_t CONSUMED     = 0xC;  // this event has been popped off the EQ


NNTI_event_t event_source[total_events];
std::atomic<uint32_t> next_event(0);
std::atomic<uint32_t> events_consumed(0);

struct ProducerWithReservation {
    nnti::datatype::nnti_event_queue *eq_;

    ProducerWithReservation(
        nnti::datatype::nnti_event_queue *eq)
    {
        eq_ = eq;
        return;
    }

    void
    operator()()
    {
        for (uint32_t i = 0; i < events_per_producer; i ++) {
            uint32_t current_event = next_event.fetch_add(1);
            assert(event_source[current_event].offset == NOT_PRODUCED);
            event_source[current_event].offset = NOT_CONSUMED;
            nnti::datatype::reservation r;
            while (!eq_->get_reservation(r)) {
                std::this_thread::yield();
            }
            NNTI_event_t *e = &event_source[current_event];
            while (!eq_->push(r, e)) {
                abort();
            }
        }
    }
};

struct ProducerWithoutReservation {
    nnti::datatype::nnti_event_queue *eq_;

    ProducerWithoutReservation(
        nnti::datatype::nnti_event_queue *eq)
    {
        eq_ = eq;
        return;
    }

    void
    operator()()
    {
        for (uint32_t i = 0; i < total_events; i += num_producers) {
            uint32_t current_event = next_event.fetch_add(1);
            assert(event_source[current_event].offset == NOT_PRODUCED);
            event_source[i].offset = NOT_CONSUMED;
            NNTI_event_t *e = &event_source[i];
            while (!eq_->push(e)) {
                std::this_thread::yield();
            }
        }
    }
};

struct Consumer {
    nnti::datatype::nnti_event_queue *eq_;

    Consumer(
        nnti::datatype::nnti_event_queue *eq)
    {
        eq_ = eq;
        return;
    }

    void
    operator()()
    {
        while (events_consumed.fetch_add(1) < total_events) {
            NNTI_event_t *e = nullptr;
            while (!eq_->pop(e)) {
                std::this_thread::yield();
            }
            assert(e);
            assert(e->offset != NOT_PRODUCED);
            assert(e->offset != CONSUMED);
            assert(e->offset == NOT_CONSUMED);
            e->offset = CONSUMED; // don't write zero
        }
    }
};

static inline unsigned long
tv_to_ms(
    const struct timeval &tv)
{
    return ((unsigned long)tv.tv_sec * 1000000 + tv.tv_usec) / 1000;
}

bool
test_without_reservation()
{
    nnti::datatype::nnti_event_queue eq_without_reservations(false, queue_size);

    std::thread producers[num_producers];
    std::thread consumers[num_consumers];

    next_event.store(0);
    events_consumed.store(0);
    memset(event_source, 0, total_events * sizeof(NNTI_event_t));
    for (uint32_t i = 0; i < total_events; ++i) {
        event_source[i].offset = NOT_PRODUCED;
    }

    struct timeval tv0, tv1;
    gettimeofday(&tv0, NULL);

    for (uint32_t i = 0; i < num_producers; ++i) {
        producers[i] = std::thread(ProducerWithoutReservation(&eq_without_reservations));
    }

    usleep(10 * 1000);

    for (uint32_t i = 0; i < num_consumers; ++i) {
        consumers[i] = std::thread(Consumer(&eq_without_reservations));
    }

    for (uint32_t i = 0; i < num_producers; ++i) {
        producers[i].join();
    }
    for (uint32_t i = 0; i < num_consumers; ++i) {
        consumers[i].join();
    }

    gettimeofday(&tv1, NULL);
    std::cout << (tv_to_ms(tv1) - tv_to_ms(tv0)) << "ms" << std::endl;

    bool success = true;
    std::cout << "check results..." << std::endl;
    for (uint32_t i = 0; i < total_events; ++i) {
        if (event_source[i].offset == NOT_PRODUCED) {
            std::cout << "not produced " << i << std::endl;
            success = false;
        } else if (event_source[i].offset == NOT_CONSUMED) {
            std::cout << "not consumed " << i << std::endl;
            success = false;
        }
    }
    std::cout << (success ? "Passed" : "FAILED") << std::endl;

    return success;
}

bool
test_with_reservation()
{
    nnti::datatype::nnti_event_queue eq_with_reservations(true, queue_size);

    std::thread producers[num_producers];
    std::thread consumers[num_consumers];

    next_event.store(0);
    events_consumed.store(0);
    memset(event_source, 0, total_events * sizeof(NNTI_event_t));
    for (uint32_t i = 0; i < total_events; ++i) {
        event_source[i].offset = NOT_PRODUCED;
    }

    struct timeval tv0, tv1;
    gettimeofday(&tv0, NULL);

    for (uint32_t i = 0; i < num_producers; ++i) {
        producers[i] = std::thread(ProducerWithReservation(&eq_with_reservations));
    }

    usleep(10 * 1000);

    for (uint32_t i = 0; i < num_consumers; ++i) {
        consumers[i] = std::thread(Consumer(&eq_with_reservations));
    }

    for (uint32_t i = 0; i < num_producers; ++i) {
        producers[i].join();
    }
    for (uint32_t i = 0; i < num_consumers; ++i) {
        consumers[i].join();
    }

    gettimeofday(&tv1, NULL);
    std::cout << (tv_to_ms(tv1) - tv_to_ms(tv0)) << "ms" << std::endl;

    bool success = true;
    std::cout << "check results..." << std::endl;
    for (uint32_t i = 0; i < total_events; ++i) {
        if (event_source[i].offset == NOT_PRODUCED) {
            std::cout << "not produced " << i << std::endl;
            success = false;
        } else if (event_source[i].offset == NOT_CONSUMED) {
            std::cout << "not consumed " << i << std::endl;
            success = false;
        }
    }
    std::cout << (success ? "Passed" : "FAILED") << std::endl;

    return success;
}

int main(int argc, char *argv[])
{
    std::stringstream logfile;
    logfile << "NntiEqClassTest.log";
    nnti::core::logger::init(logfile.str(), sbl::severity_level::error);

    bool rc1 = test_without_reservation();
    bool rc2 = test_with_reservation();

    if (rc1 && rc2)
        std::cout << "\nEnd Result: TEST PASSED" << std::endl;
    else
        std::cout << "\nEnd Result: TEST FAILED" << std::endl;

    return ((rc1 && rc2) ? 0 : 1 );
}
