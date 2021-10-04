// Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#ifndef FAODEL_COMMON_QUICKHTML_HH
#define FAODEL_COMMON_QUICKHTML_HH

#include <string>
#include <sstream>
#include <vector>
#include <map>


namespace html {

//Note: "Lucida Console" would make a nice monospace, but it's large
const std::string css_default=
  "body{background-color: #222930;}"
  "* { font-family: \"Trebuchet MS\", Helvetica, sans-serif; color: #E9E9E9;}"
  "H1 {color:#4EB1BA;} "
  "H2 {color:#d9d9d9;} "
  "A { color:#6Ed1dA; font-weight: bold; text-decoration: none;} "
  ".HDR {background-color: #444950; font-weight: bold } "
  ".HEXE {font-family: monospace; color: #D1D1F0;} "
  ".HEXO {font-family: monospace; color: #F0D1D1;} "
  "table{border-spacing:20px 0}";


void mkHeader(std::stringstream &ss, const std::string &title, const std::string css_syle=css_default);
void mkFooter(std::stringstream &ss);

void mkSection(std::stringstream &ss, const std::string label, int heading_level=1);
void mkCode(std::stringstream &ss, const std::string text, const std::string &code_name="");
void mkTable(std::stringstream &ss, const std::map<std::string,std::string> &items, const std::string &table_name="", bool highlight_top=true);
void mkTable(std::stringstream &ss, const std::vector<std::pair<std::string,std::string>> &items, const std::string &table_name="", bool highlight_top=true);
void mkTable(std::stringstream &ss, const std::vector<std::vector<std::string>> &items, const std::string &table_name="",  bool highlight_top=true);

void mkList(std::stringstream &ss, const std::vector<std::string> &items, const std::string &list_name="");
void mkText(std::stringstream &ss, const std::string &text, int level=-1);

std::string mkLink(const std::string &name, const std::string &link);

//For making a table as you go
void tableBegin(std::stringstream &ss, std::string label, int heading_level=1);
void tableTop(std::stringstream &ss, const std::vector<std::string> col_names);
void tableRow(std::stringstream &ss, const std::vector<std::string> cols, std::string row_css_tag="");
void tableEnd(std::stringstream &ss);


} //html

#endif // FAODEL_COMMON_QUICKHTML_HH

