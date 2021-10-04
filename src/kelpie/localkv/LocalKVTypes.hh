// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef FAODEL_LOCALKVTYPES_HH
#define FAODEL_LOCALKVTYPES_HH


namespace kelpie {

namespace lkv {

typedef uint8_t lambda_flags_t;

/**
 * @brief Flags passed into the LocalKV's lambda operators
 * The LocalKV has doRowOp/doColCop functions to let you write core functions at the top level (localkv)
 * without having to worry about all the mutex handling and output generation on the way down. We use the
 * lambda_flags option to ensure we are more descriptive about what actions should be taken in the lambda.
 * Currently there are two flags:
 *
 * CREATE_IF_MISSING: If the key we're looking for doesn't exist, allocate space for it. Puts, Gets usually
 *                    need this, but info functions do not
 * TRIGGER_DEPENDENCIES: When done, check the entry to see if anything was waiting on this op and needs to
 *                    be woken up. Puts need this, but gets do not.
 */
struct LambdaFlags {
  static constexpr lambda_flags_t CREATE_IF_MISSING = 0x01;
  static constexpr lambda_flags_t TRIGGER_DEPENDENCIES = 0x02;
  static constexpr lambda_flags_t DONT_CREATE_OR_TRIGGER = 0x0;
};

} // namespace lkv
} // namespace kelpie

#endif //FAODEL_LOCALKVTYPES_HH
