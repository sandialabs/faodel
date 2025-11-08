// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

/**
 * @file nnti_logger.h
 *
 * @brief nnti_logger.h
 *
 * @author Todd Kordenbrock (thkorde\@sandia.gov).
 * Created on: August 03, 2015
 *
 */

#ifndef NNTI_LOGGER_H_
#define NNTI_LOGGER_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifdef NNTI_ENABLE_DEBUG_LOGGING
#define log_debug(c,...) log_debug_msg(c,__FUNCTION__,__FILE__,__LINE__,__VA_ARGS__)
#else
#define log_debug(c,...)
#endif
#define log_info(c,...)  log_info_msg(c,__FUNCTION__,__FILE__,__LINE__,__VA_ARGS__)
#define log_warn(c,...)  log_warn_msg(c,__FUNCTION__,__FILE__,__LINE__,__VA_ARGS__)
#define log_error(c,...) log_error_msg(c,__FUNCTION__,__FILE__,__LINE__,__VA_ARGS__)
#define log_fatal(c,...) log_fatal_msg(c,__FUNCTION__,__FILE__,__LINE__,__VA_ARGS__)


void
log_debug_msg(
    const char *channel,
    const char *func_name,
    const char *file_name,
    const int   line_num,
    const char *msg,
    ...);
void
log_info_msg(
    const char *channel,
    const char *func_name,
    const char *file_name,
    const int   line_num,
    const char *msg,
    ...);
void
log_warn_msg(
    const char *channel,
    const char *func_name,
    const char *file_name,
    const int   line_num,
    const char *msg,
    ...);
void
log_error_msg(
    const char *channel,
    const char *func_name,
    const char *file_name,
    const int   line_num,
    const char *msg,
    ...);
void
log_fatal_msg(
    const char *channel,
    const char *func_name,
    const char *file_name,
    const int   line_num,
    const char *msg,
    ...);

#ifdef __cplusplus
}
#endif

#endif /* NNTI_LOGGER_H_ */
