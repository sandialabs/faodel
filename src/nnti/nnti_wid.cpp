// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

/**
 * @file nnti_wid.cpp
 *
 * @brief nnti_wid.cpp
 *
 * @author Todd Kordenbrock (thkorde\@sandia.gov).
 * Created on: Oct 02, 2015
 */


#include "nnti/nnti_pch.hpp"

#include "nnti/nntiConfig.h"

#include <atomic>

#include "nnti/nnti_wid.hpp"


std::atomic<uint32_t> nnti::datatype::nnti_work_id::next_id_ = {1};
