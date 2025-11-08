// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include "faodel-common/QuickHTML.hh"

using namespace std;

namespace html {

void mkHeader(stringstream &ss, const string &title, const string css_style) {
    ss<<"<!doctype html public \"-//w3c//dtd html 4.0 transitional//en\">\n"
      <<"<html><head><meta http-equiv=\"Content-Type\" content=\"text/html; charset=iso-8859-1\">\n"
      <<"<style>"<<css_style<<"</style>\n"
      <<"<title>"<<title<<"</title>\n"
      <<"</head><body>\n";
}
void mkFooter(stringstream &ss) {
  ss<< mkLink("Return to top", "/");
  ss<<"</body></html>";
}
void mkSection(stringstream &ss, string label, int heading_level){
  ss<<"<h"<<heading_level<<">"<<label<<"</h"<<heading_level<<">\n"; 
}
void mkCode(stringstream &ss, const string text, const string &code_name) {
  if(!code_name.empty()) mkSection(ss, code_name,1);
  
  ss << "<code>";
  size_t pos_prv=0,pos=0;
  while((pos=text.find("\n",pos))!=string::npos){
    ss << text.substr(pos_prv, pos-pos_prv) << "<br>\n";
    pos_prv=++pos;
  }
  ss << text.substr(pos_prv) <<"<br></code>\n";

}

void mkTable(stringstream &ss, const map<string,string> &items, const string &table_name,  bool highlight_top) {

  string top_tag = (highlight_top) ? " class=HDR" : "";
  
  if(table_name!=""){
    ss << "<h1>"<<table_name<<"</h1>";
  }
  ss << "<table>";
  for(auto &name_val : items){
    ss << "<td><td"<<top_tag<<">"<<name_val.first<<"</td><td"<<top_tag<<">"<<name_val.second<<"</td></tr>\n";
    top_tag="";
  }
  ss << "</table>\n";
}
void tableBegin(stringstream &ss, string label, int heading_level){
  if(!label.empty()){
    ss << "<h"<<heading_level<<">"<<label<<"</h"<<heading_level<<">";
  }
  ss << "<table>";
}
void tableTop(stringstream &ss, const vector<string> col_names){
  ss<<"<tr>";
  for(auto &name : col_names)
    ss << "<td class=HDR>"<<name<<"</td>";
  ss<<"</tr>\n";
}
void tableRow(stringstream &ss, const vector<string> cols, string row_tag){
  ss<<"<tr>";
  for(auto &name : cols)
    ss << "<td"<<row_tag<<">"<<name<<"</td>";
  ss<<"</tr>\n";
}
void tableEnd(stringstream &ss){
  ss<<"</table><br>";
}
void mkTable(stringstream &ss, const vector<pair<string,string>> &items, const string &table_name, bool highlight_top){

  string top_tag = (highlight_top) ? " class=HDR" : "";
  tableBegin(ss, table_name, 1);
  for(auto &name_val : items){
    tableRow(ss, {name_val.first, name_val.second}, top_tag);
    top_tag="";
  }
  tableEnd(ss);
}
void mkTable(stringstream &ss, const vector<vector<string>> &items, const string &table_name, bool highlight_top){
  string top_tag = (highlight_top) ? " class=HDR" : "";
  tableBegin(ss, table_name, 1);
  for(auto &row : items) {
    tableRow(ss, row, top_tag);
    top_tag="";
  }
  tableEnd(ss);
}

void mkList(stringstream &ss, const vector<string> &items, const string &list_name){
  if(list_name!="")
    ss << "<h1>"<<list_name<<"</h1>";
  ss << "<ul>";
  for(auto &name : items)
    ss << "<li>"<<name<<"</li>\n";
  ss << "</ul>\n";
}

string mkLink(const string &name, const string &link) {
  return "<a href="+link+">"+name+"</a>";
}
void mkText(stringstream &ss, const string &text, int level){
  if((level<1)||(level>7)) ss<<"<p>"<<text<<"</p>\n";
  else                     ss<<"<h"<<level<<">"<<text<<"</h"<<level<<">\n";  
}

} //namespace html

