// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <unistd.h>
#include <iostream>

#include "faodelConfig.h"

#if USE_NNTI==1
#include "nnti/nntiConfig.h"
#endif

#include <infiniband/verbs.h>
#if (NNTI_HAVE_VERBS_EXP_H)
#include <infiniband/verbs_exp.h>
#endif

using namespace std;


struct ibv_device *get_ib_device(void)
{
    struct ibv_device *dev=nullptr;
    struct ibv_device **dev_list;
    int dev_count=0;

    dev_list = ibv_get_device_list(&dev_count);
    if (dev_count == 0)
        return nullptr;
    if (dev_count > 1) {
        cout << "found " << dev_count << " devices, defaulting the dev_list[0] (" << dev_list[0] << ")" << endl;
    }
    dev = dev_list[0];
    ibv_free_device_list(dev_list);

    return dev;
}
bool exp_qp(void)
{
#if NNTI_HAVE_IBV_EXP_CREATE_QP
    return true;
#else
    return false;
#endif
}
bool exp_atomic_cap(struct ibv_device_attr *dev_attr)
{
    if (dev_attr->atomic_cap & IBV_ATOMIC_HCA) {
        return false;
    } else {
        return true;
    }
}
bool atomic_result_is_be(struct ibv_context *ctx)
{
#if (NNTI_HAVE_IBV_EXP_QUERY_DEVICE && NNTI_HAVE_IBV_EXP_ATOMIC_HCA_REPLY_BE)
    int ibv_rc=0;
    struct ibv_exp_device_attr exp_dev_attr;
    memset(&exp_dev_attr, 0, sizeof(exp_dev_attr));
    exp_dev_attr.comp_mask = IBV_EXP_DEVICE_ATTR_RESERVED - 1;
    ibv_rc = ibv_exp_query_device(ctx, &exp_dev_attr);
    if (ibv_rc) {
        cout << "Couldn't query ibverbs compatible device." << endl;
        return false;
    }

    return (exp_dev_attr.exp_atomic_cap == IBV_EXP_ATOMIC_HCA_REPLY_BE);
#else
    return false;
#endif
}
void ib_sanity_check()
{

    struct ibv_device *dev=get_ib_device();
    if (!dev) {
        cout << "Couldn't find an ibverbs compatible device on this machine." << endl;
        return;
    }

    struct ibv_context *ctx = ibv_open_device(dev);
    if (!ctx) {
        cout << "Couldn't open ibverbs compatible device." << endl;
        return;
    }

    struct ibv_device_attr dev_attr;
    int rc = ibv_query_device(ctx, &dev_attr);
    if (rc) {
        cout << "Couldn't query ibverbs compatible device." << endl;
        return;
    }

    bool have_exp_qp             = exp_qp();
    bool have_exp_atomic_cap     = exp_atomic_cap(&dev_attr);
    //bool byte_swap_atomic_result = atomic_result_is_be(ctx);

    cout << "========================IBVerbs Sanity Check==========================" << endl;
    if (have_exp_qp) {
        cout << " Faodel was built with the expanded verbs API." << endl;
    } else {
        cout << " Faodel was built with the standard verbs API." << endl;
    }
    if (have_exp_atomic_cap) {
        cout << " The NIC in this machine has expanded atomics capabilities." << endl;
    } else {
        cout << " The NIC in this machine has standard atomics capabilities." << endl;
    }
    if (have_exp_qp && have_exp_atomic_cap) {
        cout << " Good News!!  Atomics will work on this machine." << endl;
    } else if (have_exp_qp && !have_exp_atomic_cap) {
        cout << " Good News!!  Atomics will work on this machine." << endl;
    } else if (!have_exp_qp && have_exp_atomic_cap) {
        cout << " Bad News!!  Atomics will not work on this machine." << endl;
    } else if (!have_exp_qp && !have_exp_atomic_cap) {
        cout << " Good News!!  Atomics will work on this machine." << endl;
    }
    cout << "======================================================================" << endl;
    fprintf(stdout, "\n");
}
