// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <iostream>
#include <vector>
#include <algorithm>

#include "faodelConfig.h"
#ifdef Faodel_ENABLE_MPI_SUPPORT
#include <mpi.h>
#endif

#include "gtest/gtest.h"

#include "faodel-common/ResourceURL.hh"
#include "kelpie/Kelpie.hh"

#include "kelpie/core/Singleton.hh" //For core access

using namespace std;
using namespace faodel;
using namespace kelpie;

//The configuration used in this example
std::string default_config_string = R"EOF(

# For local testing, tell kelpie to use the nonet implementation
kelpie.type nonet
dirman.type none


# Uncomment these options to get debug info for each component
#bootstrap.debug true
#whookie.debug   true
#opbox.debug     true
#dirman.debug    true
#kelpie.debug    true

# We start/stop multiple times (which lunasa's tcmalloc does not like), so
# we have to switch to a plain malloc allocator
lunasa.lazy_memory_manager malloc
lunasa.eager_memory_manager malloc

)EOF";


class IomFromUrl : public testing::Test {
protected:
  void SetUp() override {


    config.Append(default_config_string);


    bootstrap::Start(config, kelpie::bootstrap);
  }
  virtual void TearDown() {
    bootstrap::Finish();
  }

  rc_t rc;
  internal_use_only_t iuo;
  Configuration config;

};

TEST_F(IomFromUrl, Basics) {

  string url1(  "dht:/my/thing"
                "&iom=foobar"
                "&iom_type=PosixIndividualObjects"
                "&iom_path=/tmp/zip" );

  rc = internal::getKelpieCore()->iom_registry.RegisterIomFromURL(faodel::ResourceURL(url1));
  //rc = kelpie::RegisterIomFromURL( faodel::ResourceURL(url1) );
  EXPECT_EQ(0,rc);

  string url2(  "dht:/my/other/thing"
                "&iom=boston-creme"
                "&iom_type=PosixIndividualObjects"
                "&iom_path=/tmp/zip" );

  rc = internal::getKelpieCore()->iom_registry.RegisterIomFromURL(faodel::ResourceURL(url2));
  //rc = kelpie::RegisterIomFromURL( faodel::ResourceURL(url2) );
  EXPECT_EQ(0,rc);

  string url3(  "dht:/my/other/thing"
                "&iom=honey-glaze"
                "&iom_type=PosixIndividualObjects"
                "&iom_path=/tmp/zip" );

  rc = internal::getKelpieCore()->iom_registry.RegisterIomFromURL(faodel::ResourceURL(url3));
  //rc = kelpie::RegisterIomFromURL( faodel::ResourceURL(url3) );
  EXPECT_EQ(0,rc);


  auto names = kelpie::GetIomNames();
  auto f1 = find(names.begin(), names.end(), "foobar");       EXPECT_NE(names.end(), f1);
  auto f2 = find(names.begin(), names.end(), "boston-creme"); EXPECT_NE(names.end(), f2);
  auto f3 = find(names.begin(), names.end(), "honey-glaze");  EXPECT_NE(names.end(), f3);


  //for(auto &n : names) cout<<n<<endl;
}

TEST_F(IomFromUrl, NoInserts) {

  string url1(  "dht:/my/other/thing"
                "&iom=single-good-one"
                "&iom_type=PosixIndividualObjects"
                "&iom_path=/tmp/zip" );

  rc = internal::getKelpieCore()->iom_registry.RegisterIomFromURL(faodel::ResourceURL(url1));
  //rc = kelpie::RegisterIomFromURL( faodel::ResourceURL(url1) );
  EXPECT_EQ(0,rc);


  string url2(  "dht:/my/other/thing"
                "&iom=missing-type"
                "&iom_path=/tmp/zip" );

  rc = internal::getKelpieCore()->iom_registry.RegisterIomFromURL(faodel::ResourceURL(url2));
  //rc = kelpie::RegisterIomFromURL( faodel::ResourceURL(url2) );
  EXPECT_EQ(-1,rc);

  string url3(  "dht:/my/other/thing"
                "&iom=missing-path"
                "&iom_type=PosixIndividualObjects" );

  rc = internal::getKelpieCore()->iom_registry.RegisterIomFromURL(faodel::ResourceURL(url3));
  //rc = kelpie::RegisterIomFromURL( faodel::ResourceURL(url3) );
  EXPECT_EQ(-1,rc);

  auto names = kelpie::GetIomNames();
  auto f1 = find(names.begin(), names.end(), "single-good-one"); EXPECT_NE(names.end(), f1);
  auto f2 = find(names.begin(), names.end(), "missing-type");    EXPECT_EQ(names.end(), f2);
  auto f3 = find(names.begin(), names.end(), "missing-path");    EXPECT_EQ(names.end(), f3);

  //for(auto &n : names) cout<<n<<endl;
}

TEST_F(IomFromUrl, MissingIOMPrefix) {

  string url1(  "dht:/my/thing"
                "&iom=foobar"
                "&iom_type=PosixIndividualObjects"
                "&iom_path=/tmp/zip" );

  rc = internal::getKelpieCore()->iom_registry.RegisterIomFromURL(faodel::ResourceURL(url1));
  //rc = kelpie::RegisterIomFromURL( faodel::ResourceURL(url1) );
  EXPECT_EQ(0,rc);

  string url2(  "dht:/my/other/thing"
                "&iom=boston-creme"
                "&type=PosixIndividualObjects"
                "&iom_path=/tmp/zip" );

  rc = internal::getKelpieCore()->iom_registry.RegisterIomFromURL(faodel::ResourceURL(url2));
  //rc = kelpie::RegisterIomFromURL( faodel::ResourceURL(url2) );
  EXPECT_EQ(-1,rc);

  string url3(  "dht:/my/other/thing"
                "&iom=honey-glaze"
                "&iom_type=PosixIndividualObjects"
                "&path=/tmp/zip" );

  rc = internal::getKelpieCore()->iom_registry.RegisterIomFromURL(faodel::ResourceURL(url3));
  //rc = kelpie::RegisterIomFromURL( faodel::ResourceURL(url3) );
  EXPECT_EQ(-1,rc);


  auto names = kelpie::GetIomNames();
  auto f1 = find(names.begin(), names.end(), "foobar");       EXPECT_NE(names.end(), f1);
  auto f2 = find(names.begin(), names.end(), "boston-creme"); EXPECT_EQ(names.end(), f2);
  auto f3 = find(names.begin(), names.end(), "honey-glaze");  EXPECT_EQ(names.end(), f3);

  //for(auto &n : names) cout<<n<<endl;

}

TEST_F(IomFromUrl, PIOItems) {

  auto types = kelpie::GetRegisteredIomTypes();

  auto f1 = find(types.begin(), types.end(), "posixindividualobjects");
  EXPECT_NE( types.end(), f1);

  auto names_descs = kelpie::GetRegisteredIOMTypeParameters("posixindividualobjects");
  EXPECT_EQ(1, names_descs.size());

  //for(auto &t : types) cout <<t<<endl;
  //for(auto &n_d : names_descs) cout<<n_d.first<<"|"<<n_d.second<<endl;
}

int main(int argc, char **argv) {

  int rc=0;
  ::testing::InitGoogleTest(&argc, argv);

  #ifdef Faodel_ENABLE_MPI_SUPPORT
    //If we don't do this, lower layers may try start/stopping mpi each time they run, causing ops after Finalize
    int mpi_rank;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
    if(mpi_rank==0)
      rc = RUN_ALL_TESTS();
    MPI_Finalize();
  #else
    rc = RUN_ALL_TESTS();
  #endif

  return rc;
}
