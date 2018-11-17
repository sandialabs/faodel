// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include <iostream>
#include <algorithm>

#include "webhook/common/QuickHTML.hh"
#include "webhook/common/ReplyStream.hh"


using namespace std;
using namespace webhook;

ReplyStream::ReplyStream(ReplyStreamType format, string title, stringstream *existing_sstream)
  : format(format),ss(existing_sstream) {
  switch(format){
  case ReplyStreamType::TEXT: break;
  case ReplyStreamType::HTML: html::mkHeader(*ss, title); break;
  default:
    cerr <<"Unsupported format in ReplyStream\n";
    exit(-1);
  }
}

ReplyStream::ReplyStream(const map<string,string> &input_args, string title, stringstream *existing_sstream)
  : ss(existing_sstream) {

  format = ReplyStreamType::HTML; //Default
  
  auto fmt = input_args.find("format");
  if(fmt != input_args.end()){
    string val = fmt->second;
    std::transform(val.begin(), val.end(), val.begin(), ::tolower);
    if     (val == "text" || val == "txt") format=ReplyStreamType::TEXT;
    else if(val == "html")                 format=ReplyStreamType::HTML;
    else   cout <<"Ignoring bad or unsupported format: "<<val<<endl; 
  }

  
  switch(format){
  case ReplyStreamType::TEXT: break; //No Header
  case ReplyStreamType::HTML: html::mkHeader(*ss, title); break;
  default:
    cerr <<"Unsupported format in ReplyStream\n";
    exit(-1);
  }
}

void ReplyStream::mkSection(string label, int heading_level){

  switch(format){
  case ReplyStreamType::TEXT:   *ss<<label<<endl; break;
  case ReplyStreamType::HTML:   html::mkSection(*ss, label, heading_level); break;
  default:
    cerr <<"Unsupported format in ReplyStream\n";
    exit(-1);
  }
}
  
void ReplyStream::mkText(string text){
 switch(format){
 case ReplyStreamType::TEXT:   *ss<<text<<endl; break;
 case ReplyStreamType::HTML:   *ss<<"<p>"<<text<<"</p>\n"; break;
 default:
   cerr <<"Unsupported format in ReplyStream\n";
   exit(-1);
 }  
}

void ReplyStream::mkTable(const vector<pair<string,string>> entries, string label, bool highlight_top){

  switch(format){
  case ReplyStreamType::TEXT:
    if(label!="")
      *ss<<label<<endl;
    for(auto &val1_val2 : entries)
      *ss<<val1_val2.first<<"\t"<<val1_val2.second<<endl;
    break;

  case ReplyStreamType::HTML:
    html::mkTable(*ss, entries, label, highlight_top);
    break;
    
  default:
    cerr <<"Unsupported format in ReplyStream\n";
    exit(-1);
  }  
}

void ReplyStream::mkTable(const map<string,string> entries, string label, bool highlight_top){
  
  switch(format){
  case ReplyStreamType::TEXT:
    if(label!="")
      *ss<<label<<endl;
    for(auto &val1_val2 : entries)
      *ss<<val1_val2.first<<"\t"<<val1_val2.second<<endl;
    break;

  case ReplyStreamType::HTML:
    html::mkTable(*ss, entries, label, highlight_top);
    break;
    
  default:
    cerr <<"Unsupported format in ReplyStream\n";
    exit(-1);
  }
  
}

void ReplyStream::mkTable(const vector<vector<string>> entries, string label, bool highlight_top) {

  switch(format){
  case ReplyStreamType::TEXT:
    if(label!="")
      *ss<<label<<endl;
    for(auto &row : entries){
      for(auto &col : row){
        *ss<<col<<"\t";
      }
      *ss<<endl;
    }
    break;

  case ReplyStreamType::HTML:
    html::mkTable(*ss, entries, label, highlight_top);
    break;
    
  default:
    cerr <<"Unsupported format in ReplyStream\n";
    exit(-1);
  }  
}

void ReplyStream::tableBegin(string label, int heading_level){
  switch(format){
  case ReplyStreamType::TEXT:
    if(label!="")
      *ss<<label<<endl;
    break;

  case ReplyStreamType::HTML:
    html::tableBegin(*ss, label);
    break;
    
  default:
    cerr <<"Unsupported format in ReplyStream\n";
    exit(-1);
  }  
}
void ReplyStream::tableTop(const vector<string> col_names) {
  switch(format){
  case ReplyStreamType::TEXT:
    for(auto &col : col_names){
        *ss<<col<<"\t";
    }
    *ss<<endl;
    break;

  case ReplyStreamType::HTML:
    html::tableTop(*ss, col_names);
    break;
    
  default:
    cerr <<"Unsupported format in ReplyStream\n";
    exit(-1);
  }  
}
void ReplyStream::tableRow(const vector<string> cols){
  switch(format){
  case ReplyStreamType::TEXT:
    for(auto &col : cols){
        *ss<<col<<"\t";
    }
    *ss<<endl;
    break;

  case ReplyStreamType::HTML:
    html::tableRow(*ss, cols);
    break;
    
  default:
    cerr <<"Unsupported format in ReplyStream\n";
    exit(-1);
  }  

}
void ReplyStream::tableEnd(){
  switch(format){
  case ReplyStreamType::TEXT:
    *ss<<endl;
    break;

  case ReplyStreamType::HTML:
    html::tableEnd(*ss);
    break;
    
  default:
    cerr <<"Unsupported format in ReplyStream\n";
    exit(-1);
  }  
}




void ReplyStream::mkList( const vector<string> &entries, const string &label){
  
  switch(format){
  case ReplyStreamType::TEXT:
    if(label!="")
      *ss<<label<<endl;
    for(auto &val : entries)
      *ss<<val<<endl;
    break;

  case ReplyStreamType::HTML:
    html::mkList(*ss, entries, label);
    break;
    
  default:
    cerr <<"Unsupported format in ReplyStream\n";
    exit(-1);
  }
  
}
  
void ReplyStream::Finish() {
  
  switch(format){
  case ReplyStreamType::TEXT:  break;
  case ReplyStreamType::HTML:
    html::mkFooter(*ss);
    break;
  default:
    cerr <<"Unsupported format in ReplyStream\n";
    exit(-1);
  }

}
  
