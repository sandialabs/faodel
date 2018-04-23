// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

/**
 * @file nnti_op.cpp
 *
 * @brief nnti_op.cpp
 *
 * @author Todd Kordenbrock (thkorde\@sandia.gov).
 * Created on: Oct 14, 2015
 */


#include "nnti/nnti_pch.hpp"

#include "nnti/nntiConfig.h"

#include <atomic>

#include "nnti/nnti_op.hpp"


std::atomic<uint32_t> nnti::core::nnti_op::next_id_ = {1};
