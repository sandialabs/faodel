// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include "gtest/gtest.h"

#include <thread>

#include "faodel-common/Common.hh"
#include "lunasa/Lunasa.hh"
#include "lunasa/DataObject.hh"

using namespace std;
using namespace faodel;
using namespace lunasa;

string default_config = R"EOF(
lunasa.lazy_memory_manager malloc
lunasa.eager_memory_manager malloc
)EOF";

static const auto MAX_THREADS = 8;

class LunasaCopyLDOTest : public testing::Test {
protected:
    void SetUp() override {
        Configuration config(default_config);
        config.AppendFromReferences();

        bootstrap::Init(config, lunasa::bootstrap);
        bootstrap::Start();
    }

    void TearDown() override {
        bootstrap::Finish();
    }
};

struct TestRunner {
    virtual void operator()() = 0;
};
struct CopyAssignmentRunner : public TestRunner {
    DataObject original;

    CopyAssignmentRunner(DataObject ldo)
    : original(ldo)
    { }

    void operator()() {
        DataObject copied;

        for (int i=0;i<1000000;i++) {
            copied = original;
            char *dataPtr = (char*)copied.GetDataPtr();
            dataPtr[0]='6';
        }
    }
};
struct MoveAssignmentRunner : public TestRunner {
    DataObject original;

    MoveAssignmentRunner(DataObject ldo)
    : original(ldo)
    { }

    void operator()() {
        DataObject moved;
        for (int i=0;i<1000000;i++) {
            moved = std::move(original);

            char *dataPtr = (char*)moved.GetDataPtr();
            dataPtr[0]='6';

            original = std::move(moved); // move it back
        }
    }
};
struct CopyConstructorRunner : public TestRunner {
    DataObject original;

    CopyConstructorRunner(DataObject ldo)
    : original(ldo)
    { }

    void operator()() {
        for (int i=0;i<1000000;i++) {
            DataObject copied(original);
            char *dataPtr = (char*)copied.GetDataPtr();
            dataPtr[0]='6';
        }
    }
};
struct MoveConstructorRunner : public TestRunner {
    DataObject original;

    MoveConstructorRunner(DataObject ldo)
    : original(ldo)
    { }

    void operator()() {
        for (int i=0;i<1000000;i++) {
            DataObject moved(std::move(original));

            char *dataPtr = (char*)moved.GetDataPtr();
            dataPtr[0]='6';

            original = std::move(moved); // move it back
        }
    }
};
struct CopyNewDeleteRunner : public TestRunner {
    struct DeleteLdo {
        DataObject *ldo;

        DeleteLdo(DataObject *ldo)
        : ldo(ldo)
        {
        }
        void operator()(void)
        {
            delete ldo;
        }
    };

    void operator()() {
        for (int j=0;j<1000;j++) {
            std::thread thr[MAX_THREADS];
            for (int i=0;i<MAX_THREADS;i++) {
                DataObject original(1024,64,128, lunasa::DataObject::AllocatorType::eager, 0x2112);
                thr[i] = std::thread(DeleteLdo(new DataObject(original)));
            }
            for (int i=0;i<MAX_THREADS;i++) {
                thr[i].join();
            }
        }
    }
};
struct ThreadWorker {
    ThreadWorker(TestRunner *tr)
    : runner(tr)
    {

    }
    void operator()(void)
    {
        (*runner)();
    }
    TestRunner *runner;
};

//Make sure allocations wind up being right
TEST_F(LunasaCopyLDOTest, CopyAssignment) {
    std::thread thr[MAX_THREADS];

    DataObject original(1024,64,128, lunasa::DataObject::AllocatorType::eager, 0x2112);

    for (int j = 1; j <= MAX_THREADS; j++) {
        for (int i = 0; i < j; ++i) {
            thr[i] = std::thread(ThreadWorker(new CopyAssignmentRunner(original)));
        }
        for (int i = 0; i < j; ++i) {
            thr[i].join();
        }
    }
}

TEST_F(LunasaCopyLDOTest, MoveAssignment) {
    std::thread thr[MAX_THREADS];

    DataObject original(1024,64,128, lunasa::DataObject::AllocatorType::eager, 0x2112);

    for (int j = 1; j <= MAX_THREADS; j++) {
        for (int i = 0; i < j; ++i) {
            thr[i] = std::thread(ThreadWorker(new MoveAssignmentRunner(original)));
        }
        for (int i = 0; i < j; ++i) {
            thr[i].join();
        }
    }
}

TEST_F(LunasaCopyLDOTest, CopyConstructor) {
    std::thread thr[MAX_THREADS];

    DataObject original(1024,64,128, lunasa::DataObject::AllocatorType::eager, 0x2112);

    for (int j = 1; j <= MAX_THREADS; j++) {
        for (int i = 0; i < j; ++i) {
            thr[i] = std::thread(ThreadWorker(new CopyConstructorRunner(original)));
        }
        for (int i = 0; i < j; ++i) {
            thr[i].join();
        }
    }
}

TEST_F(LunasaCopyLDOTest, MoveConstructor) {
    std::thread thr[MAX_THREADS];

    DataObject original(1024,64,128, lunasa::DataObject::AllocatorType::eager, 0x2112);

    for (int j = 1; j <= MAX_THREADS; j++) {
        for (int i = 0; i < j; ++i) {
            thr[i] = std::thread(ThreadWorker(new MoveConstructorRunner(original)));
        }
        for (int i = 0; i < j; ++i) {
            thr[i].join();
        }
    }
}

TEST_F(LunasaCopyLDOTest, CopyNewDelete) {
    std::thread thr[MAX_THREADS];

    DataObject original(1024,64,128, lunasa::DataObject::AllocatorType::eager, 0x2112);

    ThreadWorker worker(new CopyNewDeleteRunner());
    worker();
}
