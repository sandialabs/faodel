// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <iostream>
#include <random>

#include <arrow/api.h>
#include <arrow/compute/api.h>
#include <arrow/dataset/dataset.h>

#include "ArrowHelpers.hh"


using namespace std;
using namespace arrow;

std::shared_ptr<arrow::Table> createParticleTable(int num_particles) {


   const long int MAX_PARTICLES_PER_BATCH=1024;

   std::random_device rd;
   std::mt19937 mt(rd());
   std::uniform_real_distribution<float> rng_pos(0, 1.0);
   std::uniform_int_distribution<long int> rng_id(0, num_particles);
   std::uniform_int_distribution<long int> rng_time(0, 10);

   long int time=0;
   vector<shared_ptr<RecordBatch>> record_batches;

   long int particles_per_batch = MAX_PARTICLES_PER_BATCH;
   for(int particles_left=num_particles; particles_left>0; particles_left -= particles_per_batch) {

      std::shared_ptr<arrow::Array> array_time, array_id, array_x, array_y, array_z;

      arrow::NumericBuilder<arrow::Int64Type> builder_id, builder_time;
      arrow::NumericBuilder<arrow::FloatType> builder_x, builder_y, builder_z;

      if(particles_left<particles_per_batch) {
         particles_per_batch = particles_left;
      }
      int ok=0;
      for(int row = 0; row < particles_per_batch; row++) {
         time += rng_time(mt);
         ok += builder_time.Append(time).ok();
         ok += builder_id.Append(rng_id(mt)).ok();
         ok += builder_x.Append(rng_pos(mt)).ok();
         ok += builder_y.Append(rng_pos(mt)).ok();
         ok += builder_z.Append(rng_pos(mt)).ok();
      }
      ok += builder_time.Finish(&array_time).ok();
      ok += builder_id.Finish(&array_id).ok();
      ok += builder_x.Finish(&array_x).ok();
      ok += builder_y.Finish(&array_y).ok();
      ok += builder_z.Finish(&array_z).ok();
      if(ok!=5*particles_per_batch+5) cerr<<"Particle table not created properly\n";

      shared_ptr<Schema> schema = arrow::schema({field("Time", arrow::int64()),
                                                 field("Id", arrow::int64()),
                                                 field("X", arrow::float32()),
                                                 field("Y", arrow::float32()),
                                                 field("Z", arrow::float32())});

      //cout<<"Creating rb with "<<num_entries_per_batch<<" tsize:"<<array_time->length()<< " idsize: "<<array_id->length()<<"\n";
      record_batches.push_back(arrow::RecordBatch::Make(schema, particles_per_batch,
                                                        {array_time, array_id, array_x, array_y, array_z}));
   }

   //cout <<"generating table from "<<record_batches.size()<<endl;
   auto res = arrow::Table::FromRecordBatches(record_batches);
   if(!res.ok()) {cerr <<"Did not create table correctly"; return nullptr;}
   //cout <<"Table ok:" << table.ok() <<endl;
   return res.ValueUnsafe();
}

/// Create a simple integer table of random data rows x cols
/// @param num_rows Number of rows (may split across record batches
/// @param num_cols Number of columns
/// @returns Table
std::shared_ptr<arrow::Table> createIntTable(int num_rows, int num_cols) {

   std::random_device rd;
   std::mt19937 mt(rd());
   std::uniform_int_distribution<long int> rng_id(0, 1024);

   //Create the schema
   std::vector<shared_ptr<arrow::Field>> fields;
   for(int i=0; i<num_cols; i++) {
      shared_ptr<arrow::Field> field = make_shared<arrow::Field>("Time", arrow::int64());
      fields.push_back(field);
   }
   shared_ptr<Schema> schema = arrow::schema(fields);


   long int time=0;
   vector<shared_ptr<RecordBatch>> record_batches;

   long int rows_per_batch = 1024;
   for(int rows_left=num_rows; rows_left>0; rows_left -= rows_per_batch) {

      std::vector<std::shared_ptr<arrow::Array>> arrays;

      if(rows_left<rows_per_batch) {
         rows_per_batch = rows_left;
      }
      for(int c=0; c<num_cols; c++) {
         std::shared_ptr<arrow::Array> array;
         arrow::NumericBuilder<arrow::Int64Type> builder;
         for(int r=0; r<rows_per_batch; r++) {
            auto stat = builder.Append(rng_id(mt));
            if(!stat.ok()) cerr<<"Table building error\n";
         }
         auto stat = builder.Finish(&array);
         if(!stat.ok()) cerr<<"Table building error\n";
         arrays.push_back(array);
      }

      record_batches.push_back(arrow::RecordBatch::Make(schema, rows_per_batch, arrays));
   }

   //cout <<"generating table from "<<record_batches.size()<<endl;
   auto table = arrow::Table::FromRecordBatches(record_batches);
   //cout <<"Table ok:" << table.ok() <<endl;
   return *table;
}


/// Use Arrow's built-in table comparison operators to see if tables are same
/// @retval -1 Schemas are different
/// @retval -2 Schemas same, but data is different
/// @retval 0 Tables are equal
int compareTables(std::shared_ptr<arrow::Table> t1, std::shared_ptr<arrow::Table> t2) {
   auto s1 = t1->schema();
   auto s2 = t1->schema();
   if(!s1->Equals(*s2, true)) return -1;
   if(!t1->Equals(*t2, true)) return -2;
   return 0;
}

/// Use Arrow's built-in sum function to add up all digits in each integer column
vector<int64_t> sumTableColumns(std::shared_ptr<arrow::Table> t) {
   vector<int64_t> results;

   for(int i=0; i<t->num_columns(); i++) {
      if(t->column(i)->type() != arrow::int64()){
         results.push_back(0);
         continue;
      }

      auto res = arrow::compute::CallFunction(("sum"), {t->column(i)});
      if(!res.status().ok()) {
         results.push_back(0); //Drop to zero if problems
      } else {
         auto scalar = res->scalar();
         std::shared_ptr<arrow::Int64Scalar> scalar_int = std::dynamic_pointer_cast<arrow::Int64Scalar>(scalar);
         results.push_back( scalar_int->value);
      }
   }
   return results;
}
