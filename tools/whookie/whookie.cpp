// This is a simple client tool for doing web queries to a FAODEL-based
// service. It's only intended to serve as a simple query tool when
// curl or wget are not available.

#include <iostream>

#include "webhook/client/Client.hh"

using namespace std;

int parseURL(const string &url, string &host, string &port, string &path){

  port="80";
  host="localhost";
  path="/";

  
  if((url.compare(0,7,"http://")||(url.size()<8))){
    cout<<"URL must begin with 'http://'\n";
    return 1;
  }
  string plain_url = url.substr(7);
  size_t pos1=plain_url.find_first_of(":/");
  if(pos1 == string::npos){
    //No markers, assume it's the host
    host=plain_url;
  } else {
    host=plain_url.substr(0,pos1);
    if(plain_url.at(pos1)==':'){
      size_t pos2 = plain_url.substr(pos1).find_first_of("/");

      if(pos2==string::npos){
        //No /
        if(pos1+1<plain_url.length()) 
          port=plain_url.substr(pos1+1);
        pos1=string::npos;
      } else if(pos2>1){
        //valid port and a /
        port=plain_url.substr(pos1+1,pos2-1);
        pos1+=pos2;
      } else {
        //invalid port and a /
        pos1+=pos2;
      }
    }
    if(pos1!=string::npos){
      path=plain_url.substr(pos1);
    }
  }
  return 0;
}

int main(int argc, char **argv){

  if(argc!=2){
    cout << "usage: whookie <url>\n";
    return 1;
  }
  string host,port,path;

  parseURL(string(argv[1]), host, port,path);

  string data;
  webhook::retrieveData(host, port, path, &data);
  

  cout << host<<endl
       << port<<endl
       << path<<endl;

  cout <<data<<endl;

  return 0;
}
