// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

// DataObjectPacker Example
//
// Purpose: Demonstrate how DOP can be used to pack variables into an LDO
//
// Often users need a way to stuff a collection of variables into a memory
// blob that can be transported about the system. There are a few ways
// to do this, depending on how complex the data is:
//
// POD Structs: If you have plain-old-data types that can be described
//     by a C struct, just cast the struct to the LDO and plug away. This
//     is quick to pack/unpack and easy, but doesn't work well with
//     variable data.
//
// SER/DES Libs: If you have complex data structures (hierarchical classes.
//     c++ containers) or readability matters more than performance, use
//     a serialization lib (eg, boost serialization, cereal, xdr, avro) and
//     copy the serial stream into an ldo.
//
// DataObjectPacker: If you have a bunch of variable length arrays and you
//     don't mind working w/ pointers into the LDO, use Lunasa's DOP. The DOP
//     is a thin layer on top of an LDO that allocates and packs labeled
//     vars into the object. It's not fancy, but it doesn't take much overhead
//     and its fast.
//
// In this example a user has a class with multiple variables in it (MyData).
// A user needs to pack all of the values into an object and do something
// with the data on the receiving side. These examples show how to pack
// and unpack the items.
//
// Examples 1 and 3 show packing where we "allocate and pack" in the constructor
// Example 5 shows how to "Allocate a fixed capacity and append vars"
//
// The MyVariableGatherer and MyVariableAccess classes show how to simplify
// some of the tedious work in packing data structures.


#include <iostream>
#include <vector>
#include <assert.h>

#include "lunasa/Lunasa.hh"
#include "lunasa/common/DataObjectPacker.hh"

#include "MyData.hh"
#include "MyHelpers.hh"

using namespace std;

string default_config = R"EOF(
server.mutex_type rwlock
# In this example, the default allocator is lunasa::AllocatorTcmalloc
lunasa.eager_memory_manager tcmalloc
node_role server
)EOF";


//Since we often mix different data objects in Faodel, it's useful to define
//a unique id to each packed data object so we can check to make sure its
//the thing we're looking for before we go and unpack it. This data
const uint32_t myapp_data_id = faodel::const_hash32("my data app");


//Example 1: Simple up-front packing
// If you only have a few variables to pack, you can create a list of their
// stats and hand them over to the constructor. The ctor allocates the exact
// amount of space you need and then packs everything in.
//
// Note: This example only stores TWO variables. Example 3 shows how to
//       simplify the packing of many vars using a helper class and templates.
lunasa::DataObject ex1_manual_upfront_pack(const MyData &src) {

  //Get info for all variables that are going to be packed
  vector<string> names;
  vector<const void *> ptrs;
  vector<size_t> bytes;
  vector<uint8_t> types;

  //Simple String
  names.push_back("PROP1");
  ptrs.push_back((void *)src.prop1.c_str());
  bytes.push_back(src.prop1.length());
  types.push_back(MyTypes::STRING);


  //Float array
  names.push_back("FIELD1");
  ptrs.push_back((void *)src.field1);
  bytes.push_back(sizeof(double) * src.field_length);
  types.push_back( MyTypes::FLOAT);

  //Allocate an ldo and pack it
  lunasa::DataObjectPacker dop(names, ptrs, bytes, types, myapp_data_id);

  return dop.GetDataObject();

}


//Example 2: Manual unpacking
// If you don't have a lot of data, it isn't hard to manually unpack things
// yourself. Just lookup the variable and convert its pointers/length. The
// pointer is a raw pointer into the DataObject so be careful not to
// write data or pass the pointer to another application.

void ex2_manual_unpacking(lunasa::DataObject &ldo) {

  lunasa::DataObjectPacker dop(ldo);

  dop.VerifyDataType(myapp_data_id);

  void *ptr;
  size_t bytes;
  uint8_t type;
  int rc;

  rc = dop.GetVarPointer("PROP1", &ptr, &bytes, &type);
  assert(rc==0);
  string prop1((char *)ptr, bytes);
  cout <<"Prop1 is '"<<prop1<<endl;


  rc = dop.GetVarPointer("FIELD1", &ptr, &bytes, &type);
  assert(rc==0);
  assert(type==MyTypes::FLOAT); //This is defined in the var packer
  auto *fptr = reinterpret_cast<float *>(ptr);
  size_t words = bytes/sizeof(float);
  for(int i=0; i<words; i++){
    cout <<"field1["<<i<<"] "<<fptr[i] << ((((i+1)%4)==0) ? "\n" : "\t");
  }

}



//Example 3: More complex up-front packing
// It can be tedious to plug in all the length/type info when you have a
// lot of variables.  The MyVariableGatherer class gives an example of how
// to use templates to automate some of this packing
lunasa::DataObject ex3_upfront_pack(const MyData &src) {

  //Gather up all our variables
  MyVariableGatherer p;
  p.append("PROP1", (string *) &src.prop1);
  p.append("PROP2", (double *) &src.prop2, 1);
  p.append("FIELD1", src.field1, src.field_length);
  p.append("FIELD2", src.field2, src.field_length);
  p.append("FIELD3", (double *) src.field3.data(), src.field3.size());


  //Allocate an ldo and pack it
  lunasa::DataObjectPacker dop(p.names, p.ptrs, p.bytes, p.types, myapp_data_id);

  return dop.GetDataObject();

}

//Example 4: Unpacking more complex structures
// If you're pulling out a lot of variables, it can be useful to create a
// helper class like MyVariableAccess that checks types and does type
// conversions for you with templated functions.
void ex4_easier_unpacking(lunasa::DataObject &ldo, const MyData &src) {

  lunasa::DataObjectPacker dop(ldo);
  dop.VerifyDataType(myapp_data_id);

  MyVariableAccess access(&dop);
  string p1 = access.ExpectString("PROP1");
  assert( p1 == src.prop1);
  cout <<"Prop1: '"<<p1<<"'\n";

  size_t num_words=0;
  double *p2 = access.ExpectArray<double>("PROP2", &num_words);
  assert(p2!=nullptr);
  cout <<"Prop2: num_words="<<num_words<<" prop2[0]="<<p2[0]<<endl;

  size_t nw1,nw2,nw3;
  float  *f1 = access.ExpectArray<float>("FIELD1", &nw1);  assert(f1!=nullptr);
  int    *f2 = access.ExpectArray<int>("FIELD2", &nw2);    assert(f2!=nullptr);
  double *f3 = access.ExpectArray<double>("FIELD3", &nw3); assert(f3!=nullptr);

  cout <<"Field1: num_words="<<nw1<<" field1[last]="<<f1[nw1-1]<<endl;
  cout <<"Field2: num_words="<<nw2<<" field2[last]="<<f2[nw2-1]<<endl;
  cout <<"Field3: num_words="<<nw3<<" field3[last]="<<f3[nw3-1]<<endl;

}

//Example 5: Allocate a fixed-size ldo and then append vars into it
// If you know want to bound how big your data objects are, you can allocate
// them with a certain capacity and then use the AppendVariable command
// to fill it.
//
// note: These appends adjust the length of the data segment each time you
//       append data. While you can't get the capacity back until you
//       free the DataObject, an object transferred to another node will
//       only have a capacity of meta+data lengths when it is transferred.
lunasa::DataObject ex5_ondemand_pack(const MyData &src) {

  lunasa::DataObjectPacker dop(1024*1024, myapp_data_id);

  //Append in most of the variables
  int rc=0;
  rc |= dop.AppendVariable("PROP1", (void *)src.prop1.c_str(), src.prop1.length(), MyTypes::STRING);
  rc |= dop.AppendVariable("PROP2", (void *) &src.prop2, sizeof(double), MyTypes::DOUBLE);
  rc |= dop.AppendVariable("FIELD1", (void *) src.field1, src.field_length*sizeof(float), MyTypes::FLOAT);
  rc |= dop.AppendVariable("FIELD2", (void *) src.field2, src.field_length*sizeof(int), MyTypes::INT);
  assert(rc==0);

  //Try to add some data that is too big for our allocation. The packer
  //should detect the overflow and refuse to add it. Always check your rc's
  double *big_alloc = new double[1024*1024];
  rc = dop.AppendVariable("big bad data", (void *) big_alloc, 1024*1024*sizeof(double), MyTypes::DOUBLE);
  assert(rc!=0);

  //After a failure, you can still append in additional data if there's room.
  rc = dop.AppendVariable("FIELD3", (void *) &src.field3[0], src.field_length*sizeof(double), MyTypes::DOUBLE);
  assert(rc==0);

  return dop.GetDataObject();

}

//Example 6: Getting the list of vars to unpack
// If this was packed with a type1 format (ie, field names are included), you
// can get the list of variable names that are in the object so you can
// fetch them yourself.
void ex6_unpack_names(lunasa::DataObject &ldo) {

  lunasa::DataObjectPacker dop(ldo);
  vector<string> names;
  int rc = dop.GetVarNames(&names);
  if(rc==0) {
    cout<<"Received an object with the following vars:\n";
    for(int i=0; i<names.size(); i++) {
      void *ptr;
      size_t bytes;
      rc = dop.GetVarPointer(names[i],&ptr,&bytes,nullptr);
      cout <<"["<<i<<"] Name '"<<names[i]<<"' Found: " << ((rc==0)?"Yes":"No")<<"  Bytes: "<<bytes<<endl;
    }
  }


}



int main(int argc, char **argv) {

  cout << "Starting ldo packing example\n";

  faodel::Configuration config(default_config);
  faodel::bootstrap::Start(config, lunasa::bootstrap);

  MyData src(16); //Create some data


  //Pack/unpack variables manually
  auto ldo = ex1_manual_upfront_pack(src); //Pack it into an LDO
  cout <<"EX1 Packed data size (Meta/Data): "<<ldo.GetMetaSize()<<"/"<<ldo.GetDataSize()<<endl;
  ex2_manual_unpacking(ldo); //Extract content from the ldo

  //Pack/unpack variables using some templated helpers
  ldo = ex3_upfront_pack(src);
  cout <<"EX3 Packed data size (Meta/Data): "<<ldo.GetMetaSize()<<"/"<<ldo.GetDataSize()<<endl;
  ex4_easier_unpacking(ldo, src);

  //Allocate a DataObject and fill it approach
  ldo = ex5_ondemand_pack(src);
  cout <<"EX5 Packed data size (Meta/Data): "<<ldo.GetMetaSize()<<"/"<<ldo.GetDataSize()<<endl;
  ex4_easier_unpacking(ldo, src);

  //Extract the names and walk through the contents
  ex6_unpack_names(ldo);

  faodel::bootstrap::Finish();
}
