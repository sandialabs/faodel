// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include <mpi.h>

#include <iostream>
#include <fstream>
#include <thread>
#include <mutex>
#include <map>

#include <gtest/gtest.h>

#include "common/Common.hh"

#include "webhook/Server.hh"
#include "webhook/client/Client.hh"
#include "webhook/common/QuickHTML.hh"

using namespace std;
using namespace faodel;


string default_config_string = R"EOF(
webhook.interfaces    ipogif0,eth,lo
)EOF";


int num_tests=0;

class ClientServer : public testing::Test {

protected:

    std::pair<std::string,std::string>
    split_string(
        const std::string &item,
        const char         delim)
    {
        size_t p1 = item.find(delim);

        if(p1==std::string::npos) return std::pair<std::string,std::string>(item,"");
        if(p1==item.size())       return std::pair<std::string,std::string>(item.substr(0,p1),"");
        if(p1==0)                 return std::pair<std::string,std::string>("",item.substr(p1+1));

        return std::pair<std::string,std::string>(item.substr(0,p1), item.substr(p1+1));
    }


  virtual void SetUp(){
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);

    if (mpi_rank == 0) {
        desired_port=1990;
        //port=webhook::start(desired_port);
        num_tests++; //Keep track so main can close out this many tests

        cerr<<"TODO\n"; exit(-1);
        //strcpy(server_hostname, webhook::Server::hostname().c_str());
        server_port = port;
    }
    MPI_Bcast(server_hostname, 1024, MPI_BYTE, 0, MPI_COMM_WORLD);
    MPI_Bcast(&server_port, sizeof(int), MPI_BYTE, 0, MPI_COMM_WORLD);
    cout<<"rank("<<mpi_rank<<"): server_hostname="<<server_hostname<<endl;
    cout<<"rank("<<mpi_rank<<"): server_port="<<server_port<<endl;

    MPI_Barrier(MPI_COMM_WORLD);
  }
  virtual void TearDown(){
    //TODO: ideally we'd put a stop here, but when the count goes to zero, the
    //      global webhook will stop all threads and close in a way that eats
    //      the port. For now, handle
    //webhook::stop(); //stop kills it for everyone
  }
  int port;
  int desired_port;
  char server_hostname[1024];
  int  server_port;

  int mpi_rank,mpi_size;
};


// Register hooks that allow you to set/read a value
TEST_F(ClientServer, Simple){

    if (mpi_rank == 0) {
      webhook::Server::registerHook("/test_simple", [] (const map<string,string> &args, stringstream &results) {
          //cout <<"Got op\n";
          //Grab the value

          string value;
          auto new_val = args.find("newval");
          if(new_val != args.end()){
            //cout <<"Set to "<<new_val->second<<endl;
            value = new_val->second;
          }
          results << "value=" << value << std::endl;
        });
    }
      MPI_Barrier(MPI_COMM_WORLD);

      //Now try pulling the data back
      int rc; string result;
      stringstream ss_port;  ss_port << server_port;
      for(int i=0; i<10; i++){
        stringstream ss_newval; ss_newval <<i;
        stringstream ss_path;   ss_path << "/test_simple&newval="<<i;
        rc = webhook::retrieveData(server_hostname, ss_port.str(), ss_path.str(), &result);


        std::map<std::string,std::string> param_map;
        std::istringstream iss(result);
        std::string line;
        while (std::getline(iss, line)) {
            std::pair<std::string,std::string> res = split_string(line, '=');
            param_map[res.first] = res.second;
        }

        EXPECT_EQ(0,rc);
        EXPECT_EQ(ss_newval.str(), param_map["value"]);
        //cout <<"Got back "<<value2<<endl;
        //cout <<"Result is :'"<<result<<"'\n";
      }

      MPI_Barrier(MPI_COMM_WORLD);

    if (mpi_rank == 0) {
      int rc=webhook::Server::deregisterHook("/test_simple"); EXPECT_EQ(0,rc);
    }
}


int main(int argc, char **argv){

    ::testing::InitGoogleTest(&argc, argv);

    int mpi_rank,mpi_size;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);

    Configuration conf(default_config_string);
    if(argc>1){
        if(string(argv[1])=="-v"){         conf.Append("loglevel all");
        } else if(string(argv[1])=="-V"){  conf.Append("loglevel all\nnssi_rpc.loglevel all");
        }
    }
    conf.Append("node_role", (mpi_rank==0) ? "tester" : "target");
    bootstrap::Start(conf, webhook::bootstrap);

    int rc = RUN_ALL_TESTS();
    cout <<"Tester completed all tests.\n";

    for(int i=0; i<num_tests; i++){
      //webhook::stop(); //stop kills it for everyone
    }

    MPI_Barrier(MPI_COMM_WORLD);
    bootstrap::Finish();

    MPI_Finalize();

  return rc;
}
