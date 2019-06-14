// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include <vector>
#include <iostream>
#include <set>
#include <stdexcept>

#include "faodel-common/Bootstrap.hh"
#include "faodel-services/MPISyncStart.hh"
#include "faodel-common/Configuration.hh"

#include "whookie/Server.hh"

#ifdef Faodel_ENABLE_MPI_SUPPORT
#include <mpi.h>
#endif


using namespace std;

namespace faodel {
namespace mpisyncstart {

MPISyncStart::MPISyncStart()
        : LoggingInterface("mpisyncstart") {
}

MPISyncStart::~MPISyncStart() = default;

/**
 * @brief Checks to see if we need to do mpi sync, then does a barrier and bcast of root node id
 * @param config The configuration to modify
 */
void MPISyncStart::InitAndModifyConfiguration(Configuration *config) {

  ConfigureLogging(*config);

  needs_patch = false;
  string dirman_root_mpi_string;

  std::vector<string> dirman_resources_mpi;
  int64_t dirman_root_mpi;

  config->GetBool(&needs_patch, "mpisyncstart.enable", "false");
  //needs_patch |= (ENOENT != config->GetInt(&dirman_root_mpi, "dirman.root_node_mpi", "-1"));
  needs_patch |= (ENOENT != config->GetString(&dirman_root_mpi_string, "dirman.root_node_mpi", "-1")); //Finalize in mpi section

  config->GetStringVector(&dirman_resources_mpi, "dirman.resources_mpi");
  needs_patch |= (!dirman_resources_mpi.empty());

  dbg("Does this require an MPI Sync Start? " + ((needs_patch) ? string("yes") : string("no")));

  if (!needs_patch) return;

  #ifndef Faodel_ENABLE_MPI_SUPPORT
    throw std::invalid_argument("Configuration contained an mpi update, but FAODEL not configured with MPI.");
  #else

    int mpi_rank, mpi_size;
  
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);

    //Convert to an root string to integer. User may have passed in first/middle/last token to parse
    if(dirman_root_mpi_string == "-1") dirman_root_mpi = -1;
    else {
      auto ids = ExtractIDs(dirman_root_mpi_string, mpi_size);
      kassert(ids.size()==1, "dirman.root_node_mpi can only have one value. Observed:"+dirman_root_mpi_string);
      dirman_root_mpi = *ids.begin(); //Should be first value
    }


    nodeid_t my_id = whookie::Server::GetNodeID();

    //See if we just need a barrier
    if((dirman_root_mpi==-1) && (dirman_resources_mpi.empty())) {
      dbg("mpi_sync_start requested, but no specific needs spedified. Performing Barrier");
      MPI_Barrier(MPI_COMM_WORLD);
      dbg("Barrier completed.");
    }


    //An MPI rank specified for the dirman root. Find the ID of the node
    if(dirman_root_mpi != -1) {
      dbg("Dirman Root specified as rank "+std::to_string(dirman_root_mpi)+". Perform bcast to learn whookie root.");
      kassert(dirman_root_mpi < mpi_size, "dirman.root_node_mpi value is larger than mpi ranks");
      nodeid_t root_node = my_id;
      MPI_Bcast(&root_node, sizeof(nodeid_t), MPI_CHAR, dirman_root_mpi, MPI_COMM_WORLD);
      config->Append("dirman.root_node", root_node.GetHex());
      dbg("dirman root located.  Rank "+std::to_string(dirman_root_mpi)+" is " + root_node.GetHex());
    }

    //One or more dirman resources were specified with mpi ranks. Update them in the config.
    if(!dirman_resources_mpi.empty()){

      //TODO: narrow the scope to only be necessary nodes
      auto nodes = new faodel::nodeid_t[mpi_size];

      MPI_Allgather(&my_id, sizeof(faodel::nodeid_t), MPI_CHAR,
                    nodes,  sizeof(faodel::nodeid_t), MPI_CHAR,
                    MPI_COMM_WORLD);

      if(mpi_rank == dirman_root_mpi) {
        //Only load up resources on the root node
        for (const auto &line : dirman_resources_mpi) {
          //Pull out the url and list of MPI nodes
          vector<string> tokens;
          Split(tokens, line, ' ', true);
          if (tokens.size() < 2) throw std::runtime_error("MPISyncStart Parse error for " + line);
          stringstream ss;
          ss << tokens[0];
          tokens.erase(tokens.begin());
          string ranges = Join(tokens, ' ');

          //convert the mpi ids to nodeids and append them to the config
          auto ids = ExtractIDs(ranges, mpi_size);
          if(ids.empty()) throw std::runtime_error("MPISyncStart Parse error for " + line);
          ss<<"&num="<<ids.size();
          int i = 0;
          for (auto id : ids) {
            ss << "&ag" << i << "=" << nodes[id].GetHex();
            i++;
          }

          //Add back to the config
          config->Append("dirman.resources[]", ss.str());

          dbg("Adding new resource: " + ss.str());

        }
      }

      delete[] nodes; //TOOD: Could cache this?

    }

  #endif

}

void MPISyncStart::GetBootstrapDependencies(string &name,
                                              vector<string> &requires,
                                              vector<string> &optional) const {
  name = "mpisyncstart";
  requires = {"whookie"};
  optional = {};
}

void MPISyncStart::Start() {
  #ifdef Faodel_ENABLE_MPI_SUPPORT
    //If we made some changes to config, things like dirman will need a
    //barrier before getting out of start to make sure that changes are
    //properly inserted into the root node's caches
    if(needs_patch)
      MPI_Barrier(MPI_COMM_WORLD);
  #endif

}


/**
 * @brief Register the MPISyncStart service and dependencies with bootstrap
 * @retval "mpisyncstart"
 */
std::string bootstrap() {
  static MPISyncStart mpisyncstart;

  whookie::bootstrap();
  faodel::bootstrap::RegisterComponent(&mpisyncstart, true);
  return "mpisyncstart";
}

} // namespace mpisyncstart
} // namespace faodel
