// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

/**
 * @file nnti_connection.cpp
 *
 * @brief nnti_connection.cpp
 *
 * @author Todd Kordenbrock (thkorde\@sandia.gov).
 * Created on: Sep 28, 2015
 */


#include "nnti/nnti_pch.hpp"

#include "nnti/nntiConfig.h"

#include <atomic>

#include "nnti/nnti_connection.hpp"


std::atomic<uint32_t> nnti::core::nnti_connection::next_id_ = {1};
