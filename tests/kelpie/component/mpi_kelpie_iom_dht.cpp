// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 


#include <mpi.h>

#include "gtest/gtest.h"

#include "faodel-common/Common.hh"
#include "lunasa/Lunasa.hh"
#include "opbox/OpBox.hh"
#include "dirman/DirMan.hh"
#include "faodel-services/MPISyncStart.hh"
#include "kelpie/Kelpie.hh"

#include "support/ExperimentLauncher.hh"

using namespace std;
using namespace faodel;
using namespace opbox;

const int CMD_DUMP_RESOURCES = 1;
const int CMD_WRITE_PARTICLES = 2;
const int CMD_CHECK_PARTICLES = 3; //Check a list of keys to see if they match

bool ENABLE_DEBUG=false;


const int PARTICLE_BLOB_BYTES = 1024;


string default_config_string = R"EOF(

# Multiple runs need to be done with malloc
lunasa.lazy_memory_manager  malloc
lunasa.eager_memory_manager malloc

# Enable all debug by labeling this node's role as debug_node
debug_node.mpisyncstart.debug      true
debug_node.bootstrap.debug         true
debug_node.webhook.debug           true
debug_node.opbox.debug             true
debug_node.dirman.debug            true
debug_node.dirman.cache.mine.debug true
debug_node.dirman.cache.others     true
debug_node.dirman.cache.owners     true
debug_node.kelpie.debug            true
debug_node.kelpie.pool.debug       true
debug_node.lunasa.debug            true
debug_node.lunasa.allocator.debug  true


#bootstrap.status_on_shutdown true
#bootstrap.halt_on_shutdown true

bootstrap.sleep_seconds_before_shutdown 0

# All iom work is PIO and goes to faodel_data
default.iom.type    PosixIndividualObjects
default.iom.path    ./faodel_data

## All Tests must define any additional settings in this order:
##   mpisyncstart.enable  -- if mpi is filling in any info
##   default.ioms         -- list of ioms everyone should have
##   (iom.iomname.path)   -- a path for each iom's path, if not default
##   dirman.type          -- centralized or static
##   dirman.root_node     -- root id if you're centralized
##   dirman.resources     -- lists of all the dirman entries to use


)EOF";



//Function prototypes
int removeUnderlyingParticleFiles(int mpi_size);
string getUnderlyingParticleFilename(int rank);
int checkUnderlyingParticleFileSizes(int mpi_size);

class IOMTest : public testing::Test {
protected:
  void SetUp() override {
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);

    //Get rid of previous files
    removeUnderlyingParticleFiles(mpi_size);

    //Enable debug mode
    s_config  = (!ENABLE_DEBUG)  ? default_config_string : default_config_string + "node_role debug_node\n";

    char p1[] = "/tmp/gtestXXXXXX";
    char p2[] = "/tmp/gtestXXXXXX";
    char p3[] = "/tmp/gtestXXXXXX";
    string path1 = mkdtemp(p1);
    string path2 = mkdtemp(p2);
    string path3 = mkdtemp(p3);

    s_config += "\niom.myiom1.path " + path1 +
                "\niom.myiom2.path " + path2 +
                "\niom.myiom3.path " + path3;
  }

  void TearDown() override {
    //cout <<"Server teardown\n";
    el_bcastCommand(CMD_TEARDOWN);
    bootstrap::Finish();
    MPI_Barrier(MPI_COMM_WORLD);
  }

  string s_config;

  int mpi_rank, mpi_size;
  int rc;
};

//Prototypes
int dumpResources(const std::string &s);
int writeParticles(const std::string &s);
int checkParticles(const std::string &s);

//=============================================================================
//Purpose: Verify we can setup DHTs in config and have nodes see them (but don't write data)
//  Rank 0: Tester and dirman root
TEST_F(IOMTest, SetupPools) {

  s_config+= R"EOF(
mpisyncstart.enable  true
default.ioms         myiom1;myiom2;myiom3
dirman.type          centralized
dirman.root_node_mpi 0

#First:  particle pool is a DHT using myiom1
dirman.resources_mpi[] dht:/myapp/particle&info=booya&iom=myiom1   ALL

#Second: fluid pool is a local reference using myiom2
dirman.resources[]     local:/myapp/fluid&info=stuff&iom=myiom2

#Third:  set a default reference, then overwrite it
dirman.resources[]     local:/myapp/stuff&info=default
dirman.resources[]     local:/myapp/stuff&info=modified&iom=myiom3

#Fourth: Include a dht that contains all info
dirman.resources[]     dht:<0x1234>[0x5678]/other/thing&num=2&ag0=0x4444&ag1=0x5555

)EOF";

  el_bcastConfig(CMD_NEW_KELPIE_START, s_config);
  mpisyncstart::bootstrap();
  bootstrap::Start(Configuration(s_config), kelpie::bootstrap);

  //The pool should have the myiom1 hash associated with it
  auto pool_particle = kelpie::Connect("ref:/myapp/particle");
  EXPECT_EQ(0xeea6081, faodel::hash32("myiom1"));
  EXPECT_EQ(faodel::hash32("myiom1"), pool_particle.GetIomHash());
  EXPECT_EQ("dht", pool_particle.GetURL().resource_type);

  //The pool should have the myiom2 hash associated with it
  auto pool_fluid = kelpie::Connect("ref:/myapp/fluid");
  EXPECT_EQ(0xeea6082, faodel::hash32("myiom2"));
  EXPECT_EQ(faodel::hash32("myiom2"), pool_fluid.GetIomHash());
  EXPECT_EQ("local", pool_fluid.GetURL().resource_type);

  //The pool should have the myiom2 hash associated with it
  auto pool_stuff = kelpie::Connect("ref:/myapp/stuff");
  EXPECT_EQ(0xeea6083, faodel::hash32("myiom3"));
  EXPECT_EQ(faodel::hash32("myiom3"), pool_stuff.GetIomHash());
  EXPECT_EQ("local", pool_fluid.GetURL().resource_type);

  el_bcastCommand(CMD_DUMP_RESOURCES, "/myapp/fluid");
  el_bcastCommand(CMD_DUMP_RESOURCES, "/myapp/particle");
  el_bcastCommand(CMD_DUMP_RESOURCES, "/myapp/stuff");

  //Ask for a nonexistent pool
  kelpie::Pool pnope;
  EXPECT_ANY_THROW(pnope = kelpie::Connect("local:/not_here/guys&iom=missing"));

}
//=============================================================================
//Purpose: Have tester write data to a local directory, then peek to make sure ok
// Rank 0: Tester
// dirman is static
TEST_F(IOMTest, CurrentDirectory) {

  s_config +=  R"EOF(
mpisyncstart.enable false
default.ioms        empire_particles;empire_fields
# just using default iom paths
dirman.type         static
dirman.resources[]  local:/EMPIRE/particles&iom=empire_particles
dirman.resources[]  local:/EMPIRE/fields&iom=empire_fields

)EOF";

  el_bcastConfig(CMD_NEW_KELPIE_START, s_config);
  mpisyncstart::bootstrap();
  bootstrap::Start(Configuration(s_config), kelpie::bootstrap);

  //The pool should have the myiom1 hash associated with it
  auto pool_particles = kelpie::Connect("ref:/EMPIRE/particles");
  auto pool_fields    = kelpie::Connect("ref:/EMPIRE/fields");

  //cout <<"ep is "<<hex<<faodel::hash32("empire_particles")<<endl;
  //cout <<"ef is "<<hex<<faodel::hash32("empire_fields")<<endl;
  EXPECT_EQ(0x512cfb4d, faodel::hash32("empire_particles"));
  EXPECT_EQ(0x31d0fd3d, faodel::hash32("empire_fields"));
  EXPECT_EQ(faodel::hash32("empire_particles"), pool_particles.GetIomHash());
  EXPECT_EQ(faodel::hash32("empire_fields"),   pool_fields.GetIomHash());
  EXPECT_EQ("local", pool_particles.GetURL().resource_type);
  EXPECT_EQ("local", pool_fields.GetURL().resource_type);

  el_bcastCommand(CMD_DUMP_RESOURCES, "/EMPIRE/particles");
  el_bcastCommand(CMD_DUMP_RESOURCES, "/EMPIRE/fields");

  //Write some data out
  el_bcastCommand(CMD_WRITE_PARTICLES);
  int rc = writeParticles("");
  EXPECT_EQ(1032, rc); //1024+8


  //Verify that files are there
  rc = checkUnderlyingParticleFileSizes(mpi_size);
  EXPECT_EQ(0, rc);

}

//=============================================================================
//Purpose: Verify all nodes can get DHT info, and that everyone can write into it
//  Rank 0: Tester + dirman root
//  DHT:    All ranks, with iom attached
TEST_F(IOMTest, WriteIOMDHT) {

  s_config += + R"EOF(
mpisyncstart.enable     true
default.ioms            my_iom
dirman.type             centralized
dirman.root_node_mpi    0
dirman.resources_mpi[]  dht:/EMPIRE/particles&info=booya&iom=my_iom   ALL

)EOF";

  //Share our config and start
  el_bcastConfig(CMD_NEW_KELPIE_START, s_config);
  mpisyncstart::bootstrap();
  bootstrap::Start(Configuration(s_config), kelpie::bootstrap);

  //The tester is also the root. It should find the pool and detect that myiom1 hash is associated with it
  auto pool = kelpie::Connect("ref:/EMPIRE/particles");
  EXPECT_EQ(faodel::hash32("my_iom"), pool.GetIomHash());

  //Make sure everyone can get the dirman resource
  el_bcastCommand(CMD_DUMP_RESOURCES, "/EMPIRE/particles");

  //Have all nodes write some particle data into the dht
  el_bcastCommand(CMD_WRITE_PARTICLES);
  int user_size = writeParticles("");

  //Create a list of things to check. First line is pool, rest are keys and expected length, We
  // build up a string that has the key, the rank, and expected number of bytes per rank
  stringstream ss;
  ss<<"ref:/EMPIRE/particles"<<endl;
  for(int i=0; i<mpi_size; i++){
    ss<<"my_particles:"<<i<<":"<<PARTICLE_BLOB_BYTES<<endl;
  }

  //Check the data on the tester node
  int rc = checkParticles(ss.str());
  EXPECT_EQ(0, rc);


  //Check the data exists from the worker nodes
  rc = el_bcastCommand(CMD_CHECK_PARTICLES, ss.str());
  //cout <<"Check particles: total mpi num bad = "<<rc<<endl;
  EXPECT_EQ(0, rc);

  int good_count=0;
  //cout <<"Going to get info\n";
  for(int i=0; i<mpi_size; i++){
    kelpie::Key key("my_particles", std::to_string(i));
    //cout <<"Working on retrieving info for "<<key.str()<<endl;
    kelpie::kv_col_info_t col_info;
    for(int retries=10; retries>0; retries--) {
      rc = pool.Info(key, &col_info);
      if(rc!=kelpie::KELPIE_OK){
        //cout <<"Didn't find "<<key.str()<<" Retries left: "<<retries<<endl;
        sleep(1);
      } else {
        //cout <<"Found info for "<<key.str()<<" is "<<col_info.str()<<endl;
        EXPECT_EQ(user_size, col_info.num_bytes);
        good_count++;
        break;
      }
    }
  }
  //cout <<"XXXX> Done getting infos. Good count is "<<good_count<<"\n";
  EXPECT_EQ(good_count, mpi_size);


  //Verify that files are there
  rc = checkUnderlyingParticleFileSizes(mpi_size);
  EXPECT_EQ(0, rc);
}


/**
 * @brief Generate the underlying filename that kelpie will generate for a rank
 * @param rank The rank that generates the filename
 * @return filename The path to the actual file
 * @note Assumes directory is ./faodel_data and default bucket is used
 */
string getUnderlyingParticleFilename(int rank) {
  stringstream ss;
  string s_rank_dec_digits;

  assert((rank < 1000) && "Test needs modifying because of large rank size");

  if(rank<10) s_rank_dec_digits = "01";
  else if(rank<100) s_rank_dec_digits = "02";
  else if(rank<1000) s_rank_dec_digits = "03";
  else if(rank<10000) s_rank_dec_digits = "04";

  //file names should look like  ./faodel_data/0xadd7ee83/%0c%01my%5fparticles1
  //                             ./faodel_data/0xadd7ee83/%0c%01my%5fparticles2
  //                                                   ...
  //                             ./faodel_data/0xadd7ee83/%0c%02my%5fparticles13
  ss << "./faodel_data/0xadd7ee83/%0c%" << s_rank_dec_digits << "my%5fparticles" << dec << to_string(rank);

  return ss.str();
}

//Manually remove any files we expect. This just generates all the particle filenames
//and removes them.
/**
 * @brief Manually remove any of the underlying particle files that get generated in this test
 * @param mpi_size How many mpi ranks are run
 * @return removed_files The number of files that were successfully removed
 */
int removeUnderlyingParticleFiles(int mpi_size) {
  int num_removed=0;
  for(int i=0; i<mpi_size; i++) {
    string fname = getUnderlyingParticleFilename(i);
    int rc = remove(fname.c_str());
    if(rc==0) num_removed++;
    cout <<"Removing file "<<fname<< " : "<<((rc==0)?"Success":"File Not found")<<endl;
  }
  //cout <<"Removed "<<num_removed<<" particle files out of "<<mpi_size<<endl;
  return num_removed;
}

/**
 * @brief Manually look at the files in the iom data directory and make sure they're the right size
 * @param mpi_size How many ranks there are
 * @return num_bad How many files were bad
 */
int checkUnderlyingParticleFileSizes(int mpi_size) {
  int num_bad=0;

  for(int i=0; i<mpi_size; i++) {
    bool ok = false;
    struct stat sb;
    string fname = getUnderlyingParticleFilename(i);
    for(int retries = 0; retries<5; retries++) {
      if((stat(fname.c_str(), &sb) == 0) && (S_ISREG(sb.st_mode))) {
        //cout << "Hit: File " << ss.str() << " filesize is " << to_string(sb.st_size) << endl;
        if(sb.st_size == 1032) {
          ok = true;
          break;
        }
      }
      sleep(2);
    }
    if(!ok) {
      cout << "Did not get correct filesize for filename " << fname << " Saw "<<sb.st_size<< endl;
      num_bad++;
    }
    EXPECT_TRUE(ok);
  }
  return num_bad;
}

//Command for dumping out data on the remote side
//todo: make a similar function that returns content info
int dumpResources(const std::string &s) {
  //cout <<"Node got dumpResources. My root is "<<dirman::GetAuthorityNode().GetHex()<<endl;

  DirectoryInfo dir_info;
  bool ok=false;
  for(int retries=5; (!ok) && (retries>0); retries--) {
    ok = dirman::GetDirectoryInfo(ResourceURL(s), &dir_info);
    if(!ok) {
      cout <<"XXXX DUMP Resource "<<s<<" Client missed. Current cached entries are:\n";
      vector<string> known_items;
      dirman::GetCachedNames(&known_items);
      for(auto s : known_items){
        cout <<s<<endl;
      }
      sleep(1);
    }
  }


  if(!ok) {
    cout << "No dirinfo for " << s << endl;
    // KHALT("Couldn't find dir info for "+s);
    return -1;
  } else {
    //cout <<"Found dirinfo for "<<s<<"\n"<<dir_info.str(100);
    return 0;
  }



}

//Command for dumping out particle data on the remote side
//todo: make a similar function that returns info to master
int writeParticles(const std::string &s) {

  int mpi_rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
  kelpie::Key key("my_particles", std::to_string(mpi_rank));

  auto pool_particles = kelpie::Connect("ref:/EMPIRE/particles");

  //cout <<"Publishing Particles with key "<<key.str()<<endl;
  lunasa::DataObject ldo(PARTICLE_BLOB_BYTES);
  auto dptr = ldo.GetDataPtr<uint32_t *>();
  for(int i=0; i<PARTICLE_BLOB_BYTES/sizeof(uint32_t); i++)
    dptr[i] = ((mpi_rank&0x0FF)<<24) | (i&0x00FFFFFF);

  pool_particles.Publish(key, ldo);
  //cout <<"Done\n";
  return ldo.GetWireSize();
}

int checkParticles(const std::string &s){

  int rc;
  int bad_count=0;
  istringstream ssi(s);
  stringstream sso; //output
  string pool_name;
  getline(ssi, pool_name);

  auto pool = kelpie::Connect(pool_name);

  string line;
  while(getline(ssi, line)) {
    auto tokens =  faodel::Split(line, ':');
    if(tokens.size() != 3)
      throw runtime_error("checkParticles parsed to something that didn't have 2 args? for line"+line);
    //cout <<"Checking line. '"<<tokens[0]<<"' '"<<tokens[1]<<"'"<<endl;
    int expected_bytes = atoi(tokens[2].c_str());
    kelpie::Key key(tokens[0], tokens[1]);
    kelpie::kv_col_info_t col_info;
    for(int retries=5; retries>0; retries--){
      rc = pool.Info(key, &col_info);
      if(rc==kelpie::KELPIE_OK) break;
      sleep(2);
    }
    if(rc!=kelpie::KELPIE_OK) {
      sso << "fail Could not find " << key.str() << endl;
      bad_count++;
    } else if (col_info.num_bytes-8!=expected_bytes) { //TODO: Replace 8 with ldo header size.. or fiz up stream
      sso << "fail Expected length of blob was wrong. Got "<<col_info.num_bytes<<endl;
      bad_count++;
    } else {
      sso<<"ok   found key "<<key.str()<<" size is "<<col_info.num_bytes<<" "<<availability_to_string(col_info.availability)<<endl;
    }
  }

  if(bad_count>0){
    cout <<sso.str();
  }

  return bad_count;

}


int main(int argc, char **argv){

  el_registerCommand(CMD_DUMP_RESOURCES,  dumpResources);
  el_registerCommand(CMD_WRITE_PARTICLES, writeParticles);
  el_registerCommand(CMD_CHECK_PARTICLES, checkParticles);

  return el_defaultMain(argc, argv);
}
