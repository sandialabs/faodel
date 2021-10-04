// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <iomanip>
#include "faodel-common/Common.hh"

#include "Job.hh"


using namespace std;

Job::Job(const faodel::Configuration &config, const std::string &job_category)
  : LoggingInterface(job_category),
    job_category(job_category),
    num_threads(1),
    initialized(false),
    dump_tsv(false),
    ops_completed(0) {

  ConfigureLogging(config);

  int rc;
  uint64_t us;
  config.GetTimeUS(&us, "faodel-stress.time_limit", "30s");
  run_time_seconds = us / 1000000;

  config.GetBool(&dump_tsv,"faodel-stress.dump_tsv", "false");

  rc = config.GetUInt(&num_threads, "faodel-stress.job."+job_category+".num_threads", "1");
  if(rc != 0)
    rc = config.GetUInt(&num_threads, "faodel-stress.num_threads", "1");
  F_ASSERT(rc == 0, "Unable to parse faodel-stress.num_threads out of config?");
}

void Job::DumpJobNames() {
  for(const auto &job_name : job_names) {
    cout << std::left<< std::setw(15)<<job_category<<job_name<<endl;
  }
}
void Job::DumpJobStats(const string &job_name) {
  if(dump_tsv) {
    std::cout << job_category    << "\t"
              << job_name        << "\t"
              << num_threads     << "\t"
              << ops_completed   << "\t"
              << getTestTimeUS() << "\t"
              << getTestMops()   << endl;

  } else {
    std::cout <<job_category<<":"<<job_name<<" Done. "<<num_threads<<" workers completed "<<ops_completed<<" ops in "<<getTestTimeUS()<<" us. : "<<getTestMops()<<" Mops/s\n";
  }
}

vector<string> Job::GetMatchingJobNames(const std::string &search_names) {
  string search_names_lower = faodel::ToLowercase(search_names);
  if((search_names_lower=="all") || (search_names_lower=="*")) return job_names;
  std::vector<std::string> found_names;
  auto names=faodel::Split(search_names,',', true);
  string job_category_lower = faodel::ToLowercase(job_category);
  for(auto &search_name : names) {
    string search_name_lower = faodel::ToLowercase(search_name);
    if((search_name_lower==job_category_lower+":all") ||
       (search_name_lower==job_category_lower+":*")      )
      return job_names; //All in this category

    //Check for a simple * wildcard in the search name
    size_t star_spot = search_name_lower.find_first_of("*");
    bool is_wildcard = (star_spot!=string::npos);
    if(is_wildcard) {
      search_name_lower = search_name_lower.substr(0, star_spot);
    }
    //Go through all the jobs in this category and check for hits
    for(auto &job_name : job_names) {
      string job_name_lower = faodel::ToLowercase(job_name);
      if( (search_name_lower == job_name_lower) ||
              (is_wildcard && faodel::StringBeginsWith(job_name_lower, search_name_lower)) ) {
        found_names.push_back(job_name);
      }
    }
  }
  return found_names;
}

int Job::Setup(const std::string &search_names) {
  selected_job_names = GetMatchingJobNames(search_names);
  initialized = true;
  return selected_job_names.size();
}

int Job::ExecuteAll() {
  int rc=0;
  for(const auto &name: selected_job_names) {
    rc |= Execute(name);
  }
  return rc;
}
