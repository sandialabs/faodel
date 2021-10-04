// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <iostream>
#include <thread>
#include <chrono>

#include "whookie/client/Client.hh"

#include "faodel_cli.hh"

using namespace std;
using namespace whookie;

int whookieClientGet(const vector<string> &args);


bool dumpHelpWhookieClient(string subcommand) {
  string help_wget[5] = {
          "whookie-get", "wget", "<url>", "Retrieve a faodel service webpage",
          R"(
whookie-get arguments:
  -h/--html               : Return the data in html format
  -t/--text               : Return the page in plain text
  -x S                    : Repeat this command evert S seconds
  url                     : the url to fetch

The whookie-get command provides a way for you to issue queries to a faodel
application's whookie server and get responses back. It is meant to provide a
simple command-line web client (like wget or curl) on platforms where these
tools aren't available or a proxy gets it the way. By default it issues requests
with the 'text' format enabled to make it easier to parse results.
)"
  };

  bool found=false;
  found |= dumpSpecificHelp(subcommand, help_wget);
  return found;
}

int checkWhookieClientCommands(const std::string &cmd, const vector<string> &args) {
  if(  (cmd == "whookie-get") || (cmd=="wget")) return whookieClientGet(args);
  return ENOENT;
}

void whookieClientParseBasicArgs(const vector<string> &args,
                                 string *url_string,
                                 string *format,
                                 int *sleep_interval) {

  *url_string ="";

  for(size_t i=0; i<args.size(); i++) {
    string s = args[i];
    if(      (s == "-h") || (s == "--html")) *format="html";
    else if( (s == "-t") || (s == "--text")) *format="text";
    else if( (s == "-x") ) {
      i++;
      if(i==args.size()) {
        cerr <<"Missing a value for -x ?\n";
        exit(-1);
      }
      *sleep_interval = atoi(args[i].c_str());

    } else if (s.at(0)=='-') {
      cerr << "Unrecognized option '" << s << "'\n";
      exit(-1);
    } else if( *url_string != "") {
      cerr <<"Multiple urls detected. Can only parse one.\n";
      exit(-1);
    } else {
      *url_string = s;
    }

  }
  if(*url_string == "") {
    cerr<<"No url provided?\n";
    exit(-1);
  }
  return;
}

int whookieClientGet(const vector<string> &args) {

  string mode, url;
  string format="text";
  int sleep_interval=0;

  whookieClientParseBasicArgs(args, &url, &format, &sleep_interval);

  if(url.compare(0,7,"http://") || (url.size()<8)) {
    cerr<<"URL must begin with 'http://'. Received '"<<url<<"'\n";
    exit(-1);
  }

  string host="localhost";
  string port="1990";
  string path="/";


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

    path += "&format="+format;

    do {
      string data;
      whookie::retrieveData(host, port, path, &data);
      cout << data;
      if(sleep_interval) {
        std::this_thread::sleep_for(std::chrono::seconds(sleep_interval));
      }
    } while(sleep_interval!=0);
  }
  return 0;
}