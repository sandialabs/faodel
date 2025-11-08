// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef FAODEL_ARROWHELPERS_HH
#define FAODEL_ARROWHELPERS_HH

#include <iostream>
#include <random>


#include <arrow/dataset/dataset.h>


std::shared_ptr<arrow::Table> createParticleTable(int num_particles);
std::shared_ptr<arrow::Table> createIntTable(int num_rows, int num_cols);

int compareTables(std::shared_ptr<arrow::Table> t1, std::shared_ptr<arrow::Table> t2);

std::vector<int64_t> sumTableColumns(std::shared_ptr<arrow::Table> t);


#endif //FAODEL_ARROWHELPERS_HH
