// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.


#include <nnti/nnti_types.h>

/********** Initializers **********/
//Note: these pragmas are here because C++ people can't cope with human readable c99 structures
//#pragma GCC diagnostic push
//#pragma GCC diagnostic ignored "-Wpedantic"
/**
 * @brief A simple initializer to zero-out a work request.
 */
struct NNTI_work_request_t NNTI_WR_INITIALIZER =
{   .op = NNTI_OP_NOOP,
    .flags = NNTI_OF_UNSET,
    .trans_hdl = 0,
    .peer = 0,
    .local_hdl = 0,
    .local_offset = 0,
    .remote_hdl = 0,
    .remote_offset = 0,
    .length = 0,
    .operand1 = 0,
    .operand2 = 0,
    .alt_eq = 0,
    .callback = 0,
    .cb_context = 0,
    .event_context = 0
};
//#pragma GCC diagnostic pop
