// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <iostream>
#include <functional>
#include <vector>
#include <thread>

#include "whookie/Whookie.hh"
#include "whookie/Server.hh"

#include "Worker.hh"
#include "JobWebClient.hh"

using namespace std;

class WorkerWebClient
        : public Worker {

public:
  WorkerWebClient() = default;
  WorkerWebClient(int id, JobWebClient::params_t params)
    : Worker(id, params.batch_size, params.item_len_min, params.item_len_max),
      params(params),
      bytes_retrieved(0) {

    nid = whookie::Server::GetNodeID();
    path = ((params.item_len_min==0)&&(params.item_len_max==0)) ? "/stress/minimal"
                                                                : "/stress/sized";

  }
  ~WorkerWebClient() = default;

  void server() {

    std::function<std::string ()> f_createpathsuffix;
    if(params.item_len_min == params.item_len_max) {
      if(params.item_len_min==0)
        //Fixed, minimal length
        f_createpathsuffix = []() { return ""; };
      else
        //Fixed length
        f_createpathsuffix = [this]() {
          return "&size="+std::to_string(params.item_len_min);
        };
    } else {
      //Random length
      f_createpathsuffix = [this]() {
        return "&size="+std::to_string(prngGetRangedInteger());
      };
    }

    size_t bytes_retrieved=0;
    do {
      for(int i=0; i < params.batch_size; i++) {
        string s;
        whookie::retrieveData(nid, path+f_createpathsuffix(), &s);
        bytes_retrieved += s.size();
      }
      ops_completed += params.batch_size;
    } while(!kill_server);
  }
private:

  JobWebClient::params_t params;
  int id;

  faodel::nodeid_t nid;
  string path;

  size_t bytes_retrieved;

};





JobWebClient::JobWebClient(const faodel::Configuration &config)
        : Job(config, JobCategoryName()) {

  for(auto &name_params : options)
    job_names.push_back(name_params.first);

}

int JobWebClient::Init() {

  //Register a simple nop
  whookie::Server::registerHook("/stress/minimal", [this] (const map<string,string> &args, stringstream &results) {
  });

  //Register a more complicated callback to a function that handles different sizes
  whookie::Server::registerHook("/stress/sized", [this] (const map<string,string> &args, stringstream &results) {
      return wh_Reply(args, results);
  });

  return 0;
}

int JobWebClient::Execute(const std::string &job_name) {
  return standardExecuteWorker<WorkerWebClient, JobWebClient::params_t>(job_name, options);
}

int JobWebClient::wh_Reply(const map<string, string> &args, stringstream &results) const {

  string s; //Empty
  auto it = args.find("size");
  if(it!=args.end()) {
    uint32_t num_bytes=1;
    faodel::StringToUInt32(&num_bytes, it->second);
    s=std::string(num_bytes, 'x');
  }
  //cout<<"whookie reply w. string len "<<s.size()<<endl;
  results<<s;
  return 0;
}