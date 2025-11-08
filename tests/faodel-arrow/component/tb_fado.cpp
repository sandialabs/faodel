// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.



#include "gtest/gtest.h"

#include "faodel-common/Common.hh"
#include "faodel-services/BackBurner.hh"
#include "lunasa/Lunasa.hh"
#include "lunasa/DataObject.hh"

#include "faodel-arrow/ArrowDataObject.hh"

#include "support/ArrowHelpers.hh"

using namespace std;
using namespace faodel;
using namespace arrow;

string default_config_string = R"EOF(

#bootstrap.debug true
#whookie.debug true
#lunasa.debug true

# Must use simple malloc for multiple start/stop tests
lunasa.lazy_memory_manager malloc
lunasa.eager_memory_manager malloc

)EOF";



class Fado : public testing::Test {
protected:
   void SetUp() override {
      bootstrap::Start(faodel::Configuration(default_config_string), lunasa::bootstrap);
   }

   void TearDown() override {
      bootstrap::FinishSoft();
   }
};



TEST_F(Fado, ArrowTableGenerate) {

   //Make sure we generated a table
   auto t1 = createParticleTable(64);
   EXPECT_EQ(64, t1->num_rows());
   EXPECT_EQ(5, t1->num_columns());

   //Dummy table with different data, make sure schema is same (ie, no -1 on schema comparison), but data fails
   auto t2 = createParticleTable(64);
   EXPECT_EQ(-2, compareTables(t1,t2));

}

TEST_F(Fado, CopyAndMove) {
   //This is a sanity check to make sure moves and copies take place correct. Since the ldo holds all data,
   //copies and moves should be resolved in the ldo code. If someone copies a FADO, Both should point
   //to the same ldo. If someone moves it, the origin goes empty, while the target gets the data

   //Create a faodel-arrow table
   auto t1 = createParticleTable(64);
   auto f1 = faodel::ArrowDataObject(t1);

   //Make two copies
   faodel::ArrowDataObject f2 = f1;
   faodel::ArrowDataObject f3(f1);
   EXPECT_EQ(f2.ExportDataObject(),f1.ExportDataObject());
   EXPECT_EQ(64, f1.NumberOfRows());
   EXPECT_EQ(64, f2.NumberOfRows());
   EXPECT_EQ(64, f3.NumberOfRows());
   EXPECT_EQ(0, f1.GetObjectStatus());
   EXPECT_EQ(0, f2.GetObjectStatus());
   EXPECT_EQ(0, f3.GetObjectStatus());

   //Set the Object status
   f1.SetObjectStatus(1001);
   EXPECT_EQ(1001, f1.GetObjectStatus());
   EXPECT_EQ(1001, f2.GetObjectStatus());


   //Modifying one should affect the others
   f1.Wipe(faodel::internal_use_only);
   EXPECT_EQ(0, f1.NumberOfRows());
   EXPECT_EQ(0, f2.NumberOfRows());
   EXPECT_EQ(0, f3.NumberOfRows());
   EXPECT_EQ(0, f1.GetObjectStatus());
   EXPECT_EQ(0, f2.GetObjectStatus());
   EXPECT_EQ(0, f3.GetObjectStatus());

   auto stat = f1.Append(t1);
   EXPECT_TRUE(stat.ok());
   EXPECT_EQ(64, f1.NumberOfRows());
   EXPECT_EQ(64, f2.NumberOfRows());
   EXPECT_EQ(64, f3.NumberOfRows());

   f1.SetObjectStatus(2002);

   faodel::ArrowDataObject g1;
   g1  = std::move(f1);
   EXPECT_EQ(0, f1.NumberOfTables());
   EXPECT_EQ(0, f1.NumberOfRows());
   EXPECT_EQ(0, f1.GetObjectStatus());
   EXPECT_EQ(1, g1.NumberOfTables());
   EXPECT_EQ(64, g1.NumberOfRows());
   EXPECT_EQ(2002, g1.GetObjectStatus());

   faodel::ArrowDataObject g2(std::move(f2));
   EXPECT_EQ(0, f2.NumberOfTables());
   EXPECT_EQ(0, f2.NumberOfRows());
   EXPECT_EQ(1, g2.NumberOfTables());
   EXPECT_EQ(64, g1.NumberOfRows());

   EXPECT_EQ(0,    f1.GetObjectStatus());
   EXPECT_EQ(2002, g1.GetObjectStatus());
}

TEST_F(Fado, SmallSinglePupCtor) {

   //Create an faodel-arrow table
   auto t1 = createParticleTable(64);
   EXPECT_EQ(64, t1->num_rows());
   EXPECT_EQ(5, t1->num_columns());

   //Pack into a single Fado and validate the table basics are also valid
   //ArrowDataObject f1(t1);
   auto f1 = faodel::ArrowDataObject(t1);
   //EXPECT_TRUE(res1.ok());
   //auto f1 = res1.ValueOrDie();


   EXPECT_EQ(1, f1.NumberOfTables());
   EXPECT_EQ(64, f1.NumberOfRows());
   EXPECT_EQ(ArrowDataObject::object_type_id, f1.ExportDataObject().GetTypeID());

   //Extract to a new table
   auto t2 = f1.ExtractTable(0).ValueOrDie();
   EXPECT_EQ(64, t2->num_rows());
   EXPECT_EQ(5, t2->num_columns());

   //Make sure schema and data is teh same
   int rc = compareTables(t1,t2);
   EXPECT_EQ(0, rc);
}


TEST_F(Fado, SmallSinglePupMake) {

   //Create an faodel-arrow table
   auto t1 = createParticleTable(64);
   EXPECT_EQ(64, t1->num_rows());
   EXPECT_EQ(5, t1->num_columns());

   //Pack into a single Fado and validate the table basics are also valid
   //ArrowDataObject f1(t1);
   auto res1 = faodel::ArrowDataObject::Make(t1);
   if(res1.ok()) {
      //cout<<"Is ok\n";
   } else {
      cout <<"Not ok: "<<res1.status()<<endl;
   }
   EXPECT_TRUE(res1.ok());
   auto f1 = res1.ValueOrDie();

   EXPECT_EQ(1, f1.NumberOfTables());
   EXPECT_EQ(64, f1.NumberOfRows());
   EXPECT_EQ(ArrowDataObject::object_type_id, f1.ExportDataObject().GetTypeID());

   //Extract to a new table
   auto t2 = f1.ExtractTable(0).ValueOrDie();
   EXPECT_EQ(64, t2->num_rows());
   EXPECT_EQ(5, t2->num_columns());

   //Make sure schema and data is teh same
   int rc = compareTables(t1,t2);
   EXPECT_EQ(0, rc);

}


TEST_F(Fado, PackMultipleCtor) {

   //Goal: Allocate a Fado with space, then append 3 tables using different compression methods

   vector<arrow::Compression::type> codecs = {arrow::Compression::UNCOMPRESSED, arrow::Compression::LZ4_FRAME, arrow::Compression::ZSTD};
   int NUM_CODECS=3; //Should match above


   //Create a faodel-arrow table
   auto t1 = createParticleTable(64);

   //Overshoot on how much space we need
   int fudge=2; //Guess that in the worst case, serialized object is fudge times bigger than naive r*c
   ArrowDataObject f1(codecs.size() * t1->num_rows() * t1->num_columns() * sizeof(uint64_t) * fudge );

   //Pack test
   for(int i=0; i<NUM_CODECS; i++) {
      auto status  = f1.Append(t1, codecs[i]);
      //cout <<"Status ok: "<<status.ok()<<status<<endl;
      EXPECT_TRUE(status.ok());
      //cout <<"Packed table size for "<< codecs[i] <<" : "<<f1.GetPackedRecordSize(i)<<endl;
   }

   //Check the aggregate stats
   EXPECT_EQ(NUM_CODECS, f1.NumberOfTables());
   EXPECT_EQ(NUM_CODECS*64, f1.NumberOfRows());

   //Check each table
   for(int i=0; i<NUM_CODECS; i++) {
      auto tx = f1.ExtractTable(i).ValueOrDie();
      EXPECT_EQ(0, compareTables(t1,tx));
   }

   //Blow out test -- append data, make sure available space goes down
   auto usize = f1.GetPackedRecordSize(0);
   auto asize_prev = f1.GetAvailableCapacity();

   EXPECT_LT(usize, asize_prev); //Sanity check - Test should have space for one run

   //cout<<"Blow out test: packed size is "<<usize<<" available size is "<<asize_prev<<endl;
   int extras_inserted=0;
   arrow::Status status;
   for(int i=0; i<10; i++) {
      //cout<<"Inserting: "<<i<<" pre-insert numbers: LoadedTables: "<< f1.NumberOfTables()<<" SpaceLeft: "<<f1.GetAvailableCapacity()<<" InsertSize: "<<usize<<" ap: "<<asize_prev<<endl;
      status = f1.Append(t1);
      if(status.ok()) {
         auto asize = f1.GetAvailableCapacity();
         EXPECT_LT(asize, asize_prev);
         asize_prev = asize;
         extras_inserted++;
         EXPECT_EQ(NUM_CODECS+extras_inserted, f1.NumberOfTables()); //Table added
      } else {
         EXPECT_EQ(NUM_CODECS+extras_inserted, f1.NumberOfTables()); //table not added - should be constant now
      }
   }

   EXPECT_FALSE(status.ok()); //Sanity check - make sure we did some tests

}

TEST_F(Fado, StatusErrors) {

   int num_tables=5;

   //Create a faodel-arrow table we can use a few times
   auto t1 = createParticleTable(64);
   auto t2 = createIntTable(10,4);

   //Create a wrapper with no ldo. If we append, we should be told the request is invalid
   ArrowDataObject fempty;
   auto stat1 = fempty.Append(t1);
   EXPECT_FALSE(stat1.ok());
   EXPECT_TRUE(stat1.IsInvalid());
   EXPECT_FALSE(stat1.IsOutOfMemory());

   //Create a small allocation and try to overpack it
   ArrowDataObject fsmall(100);
   EXPECT_TRUE(fsmall.Valid());
   auto stat2 = fsmall.Append(t1);
   EXPECT_FALSE(stat2.ok());
   EXPECT_FALSE(stat2.IsInvalid());
   EXPECT_FALSE(stat2.IsOutOfMemory());
   EXPECT_TRUE(stat2.IsCapacityError());

   //Try merging tables with different schema
   auto res1 = ArrowDataObject::MakeMerged({t1,t2});
   EXPECT_FALSE(res1.ok());
   EXPECT_TRUE(res1.status().IsInvalid());
   //cout << res1.status()<<endl; //Message should say record batches with different schema

   //Two similar tables should be ok though
   auto res2 = ArrowDataObject::MakeMerged({t1,t1});
   EXPECT_TRUE(res2.ok());
   if(res2.ok()){
      auto fado = res2.ValueUnsafe();
      EXPECT_EQ(64*2, fado.NumberOfRows());
   }

   //Same here
   auto res3 = ArrowDataObject::MakeMerged({t2,t2});
   EXPECT_TRUE(res3.ok());
   if(res3.ok()){
      auto fado = res3.ValueUnsafe();
      EXPECT_EQ(10*2, fado.NumberOfRows());
   }

}

TEST_F(Fado, VectorInitMake) {

   //Goal: Merge several small tables into one allocation

   //Create two faodel-arrow tables
   auto t1 = createParticleTable(64);
   auto t1s = {t1, t1, t1, t1};
   auto res = ArrowDataObject::Make(t1s, arrow::Compression::UNCOMPRESSED);
   EXPECT_TRUE(res.ok());
   if(res.ok()) {
      auto f1 = res.ValueOrDie();
      EXPECT_EQ(4, f1.NumberOfTables());
      EXPECT_EQ(4 * 64, f1.NumberOfRows());
      for(int i=0; i<4; i++) {
        EXPECT_EQ(0, compareTables(t1, f1.ExtractTable(i).ValueOrDie())); //Good meta, but bad data
      }

      for(int i=0; i<4; i++) {
         //Warning: This just sums up the INTEGER columns in the data and compares to original table
         auto t2 = f1.ExtractTable(i).ValueOrDie();
         auto sum1 = sumTableColumns(t1);
         auto sum2 = sumTableColumns(t2);
         for (int j = 0; (j < (sum1.size())) && (j < sum2.size()); j++) {
            //cout << "Original: " << sum1.at(j) << " modified: " << sum2.at(j) << endl;
            EXPECT_EQ(sum1.at(j), sum2.at(j));
         }
      }
   }
}

TEST_F(Fado, VectorInitMakeMerged) {

   //Goal: Merge several small tables into one allocation
   //Create two faodel-arrow tables
   auto t1 = createParticleTable(64);
   auto t1s = {t1, t1, t1, t1};
   auto res = ArrowDataObject::MakeMerged(t1s, arrow::Compression::UNCOMPRESSED);
   EXPECT_TRUE(res.ok());
   if(res.ok()) {
      auto f1 = res.ValueOrDie();
      EXPECT_EQ(1, f1.NumberOfTables());
      EXPECT_EQ(4 * 64, f1.NumberOfRows());

      auto t2 = f1.ExtractTable(0).ValueOrDie();

      auto sum1 = sumTableColumns(t1);
      auto sum2 = sumTableColumns(t2);
      for(int i=0; (i<(sum1.size())) && (i<sum2.size()); i++){
         //cout <<"Original: "<<sum1.at(i) <<" modified: "<<sum2.at(i)<<endl;
         EXPECT_EQ(4*sum1.at(i), sum2.at(i));
      }

   }
}

TEST_F(Fado, VectorInitMakeFromFados) {

   //Goal: Combine multiple FADOs together without unpacking the data

   //Create two faodel-arrow tables
   //==============================================================================
   // First Table: 64 rows, repeated four times
   //==============================================================================
   auto t1 = createParticleTable(64);
   auto t1s = {t1, t1, t1, t1};
   auto res = ArrowDataObject::Make(t1s, arrow::Compression::UNCOMPRESSED);
   EXPECT_TRUE(res.ok()); if(!res.ok()) return;
   auto f1 = res.ValueOrDie();
   EXPECT_TRUE(f1.Valid());
   EXPECT_EQ(4, f1.NumberOfTables());
   EXPECT_EQ(4 * 64, f1.NumberOfRows());

   //Make sure each table-chunk sums up the same as the original table
   auto sum1 = sumTableColumns(t1);
   for(int i=0; i<f1.NumberOfTables(); i++) {
      auto t1b = f1.ExtractTable(i);
      auto sum1b = sumTableColumns(*t1b);
      for(int j=0; (j<(sum1.size())) && (j<sum1b.size()); j++){
         //cout <<"Original: "<<sum1.at(i) <<" modified: "<<sum2.at(i)<<endl;
         EXPECT_EQ(sum1.at(j), sum1b.at(j));
      }
   }

   //==============================================================================
   // Second Table: 101 rows, repeated three times
   //==============================================================================
   auto t2 = createParticleTable(101);
   auto t2s = {t2,t2,t2};
   res = ArrowDataObject::Make(t2s, arrow::Compression::UNCOMPRESSED);
   EXPECT_TRUE(res.ok()); if(!res.ok()) return;
   auto f2 = res.ValueOrDie();
   EXPECT_TRUE(f2.Valid());
   EXPECT_EQ(3, f2.NumberOfTables());
   EXPECT_EQ(3 * 101, f2.NumberOfRows());

   //Make sure each table-chunk sums up the same as the original table
   auto sum2 = sumTableColumns(t2);
   for(int i=0; i<f2.NumberOfTables(); i++) {
      auto t2b = f2.ExtractTable(i);
      auto sum2b = sumTableColumns(*t2b);
      for(int j=0; (j<(sum1.size())) && (j<sum2b.size()); j++){
         //cout <<"Original: "<<sum1.at(i) <<" modified: "<<sum2.at(i)<<endl;
         EXPECT_EQ(sum2.at(j), sum2b.at(j));
      }
   }

   //==============================================================================
   // Merge all Fado's together into one object
   //==============================================================================
   res = ArrowDataObject::Make({f1,f2,f1,f2});
   EXPECT_TRUE(res.ok()); if(!res.ok()) { cout<<"Err message: "<<res.status()<<endl; return; }
   auto fcombined = res.ValueOrDie();
   EXPECT_TRUE(fcombined.Valid());
   EXPECT_EQ(4+3+4+3, fcombined.NumberOfTables());
   EXPECT_EQ(4*64 + 3*101 + 4*64 + 3*101, fcombined.NumberOfRows());

   //Allow user to try appending an empty FADO.. Should be same as the two originals
   faodel::ArrowDataObject f3; //Empty
   res = ArrowDataObject::Make({f1,f3, f2});
   EXPECT_TRUE(res.ok()); if(!res.ok()) { cout<<"Err message: "<<res.status()<<endl; return; }
   auto fcombined2 = res.ValueOrDie();
   EXPECT_TRUE(fcombined2.Valid());
   EXPECT_EQ(4+0+3, fcombined2.NumberOfTables());
   EXPECT_EQ(4*64 + 0 + 3*101, fcombined2.NumberOfRows());

   //Empties should make an empty
   res = ArrowDataObject::Make({f3, f3, f3});
   EXPECT_TRUE(res.ok()); if(!res.ok()) { cout<<"Err message: "<<res.status()<<endl; return; }
   auto fcombined3 = res.ValueOrDie();
   EXPECT_FALSE(fcombined3.Valid());

   //Read all tables and compare to originals
   vector<int> table_sequence={1,1,1,1,   2,2,2,  1,1,1,1,  2,2,2 }; //The order in which t1 and t2 have been appended to object
   EXPECT_EQ(fcombined.NumberOfTables(), table_sequence.size());
   for(int i=0; (i<fcombined.NumberOfTables()) && (i<table_sequence.size()); i++) {
      auto tcomb = fcombined.ExtractTable(i);
      auto sum_comb = sumTableColumns(*tcomb);
      EXPECT_EQ(sum1.size(), sum2.size());
      EXPECT_EQ(sum2.size(), sum_comb.size());
      cout <<"Working on table "<<i<<" seq "<<table_sequence[i]<<endl;
      for(int j=0; (j<(sum1.size())) && (j<sum_comb.size()); j++) { //Note:sum1, sum2, and sum_comb should all be same dim
         //cout <<"Original: "<<sum1.at(i) <<" modified: "<<sum2.at(i)<<endl;
         if(table_sequence[i]==1) {
            EXPECT_EQ(sum1.at(j), sum_comb.at(j));
         } else {
            EXPECT_EQ(sum2.at(j), sum_comb.at(j));
         }
      }
   }

}

TEST_F(Fado, InitMakeMerged) {

   //Build two tables, then merge a few copies into a single table.
   auto t1 = createParticleTable(64);
   auto t2 = createParticleTable(101);
   auto tables = {t1,t2,t1,t2};
   auto res = ArrowDataObject::MakeMerged(tables, arrow::Compression::UNCOMPRESSED);
   auto fc = res.ValueOrDie();
   EXPECT_EQ(1, fc.NumberOfTables());
   EXPECT_EQ(64+101 +64+101, fc.NumberOfRows());

   //Sum up each column. Since the original tables are just repeated in combined table, multiply col values
   auto sum1 = sumTableColumns(t1);
   auto sum2 = sumTableColumns(t2);
   auto table_combined = fc.ExtractTable(0);
   auto sum_combined = sumTableColumns(*table_combined);
   for(int j=0; (j<(sum1.size())) && (j<sum_combined.size()); j++){
      //cout <<"Original: "<<sum1.at(i) <<" modified: "<<sum2.at(i)<<endl;
      EXPECT_EQ(2*sum1.at(j) + 2*sum2.at(j), sum_combined.at(j));
   }

}



TEST_F(Fado, EjectLDO) {

   //Goal: Pack a table, eject the ldo, and then make sure we can rewrap it and extract table


   //Create two faodel-arrow tables
   auto t1 = createParticleTable(64);


   //Use this for referencing the ldo
   lunasa::DataObject ldo;

   //Create the initial object and get underlying ldo. Should see two holders until the creating Fado disappears
   {
      ArrowDataObject f1(t1); //Create an ldo with data in it
      ldo = f1.ExportDataObject();

      EXPECT_EQ(ArrowDataObject::object_type_id, ldo.GetTypeID());
      int ref_count = ldo.internal_use_only.GetRefCount();
      //cout<<"Ref count Start   "<<ref_count<<endl;
      EXPECT_EQ( 2, ref_count);
   }

   //Transit: All we have now is an ldo with no wrapper
   int ref_count2 = ldo.internal_use_only.GetRefCount();
   EXPECT_EQ( 1, ref_count2);
   //cout<<"Ref count Transit "<<ref_count2<<endl;

   //Revive: Pass the ldo back in to rewrap it and get at contents
   {
      ArrowDataObject f2(ldo);
      int ref_count3 = ldo.internal_use_only.GetRefCount();
      EXPECT_EQ( 2, ref_count3);
      //cout<<"Ref count Revive  "<<ref_count3<<endl;
   }

   //Done: Once wrapper disappears, we're back to just the ldo
   int ref_count4 = ldo.internal_use_only.GetRefCount();
   EXPECT_EQ( 1, ref_count4);
   //cout<<"Ref count Done    "<<ref_count4<<endl;

}

TEST_F(Fado, Wipe) {

   //Goal: Create an allocation, wipe it, reload with compressed data

   //Create two faodel-arrow tables
   auto t1 = createParticleTable(64);
   auto t2 = createParticleTable(64); //This should be different
   EXPECT_EQ(-2, compareTables(t1,t2));

   int num_tables=4;


   //Overshoot on how much space we need
   int fudge=2; //Guess that in the worst case, serialized object is fudge times bigger than naive r*c
   ArrowDataObject f1(num_tables * t1->num_rows() * t1->num_columns() * sizeof(uint64_t) * fudge );

   //Pack test
   for(int i=0; i<num_tables; i++) {
      auto status = f1.Append(t1, arrow::Compression::UNCOMPRESSED);
      EXPECT_TRUE(status.ok());
      //cout <<"Packed table size for "<< codecs[i] <<" : "<<f1.GetPackedRecordSize(i)<<endl;
   }

   //Wipe
   f1.SetObjectStatus(1881);
   EXPECT_EQ(1881, f1.GetObjectStatus());
   f1.Wipe(faodel::internal_use_only, true);
   EXPECT_EQ(0, f1.GetObjectStatus());
   EXPECT_EQ(0, f1.NumberOfTables());
   EXPECT_EQ(0, f1.NumberOfRows());
   EXPECT_GT(f1.GetAvailableCapacity(), 0);
   EXPECT_EQ(ArrowDataObject::object_type_id, f1.ExportDataObject().GetTypeID());

   //Insert other table
   for(int i=0; i<num_tables; i++) {
      auto status = f1.Append(t2, arrow::Compression::ZSTD);
      EXPECT_TRUE(status.ok());
   }
   //Make sure it reads right
   for(int i=0; i<num_tables; i++) {
      EXPECT_EQ(0, compareTables(t2, f1.ExtractTable(i).ValueOrDie()));
   }

}