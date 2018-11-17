// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#ifndef FAODEL_COMMON_REPLYSTREAM_HH
#define FAODEL_COMMON_REPLYSTREAM_HH

#include <vector>
#include <map>
#include <sstream>
#include <utility>

namespace faodel {

enum class ReplyStreamType { TEXT, HTML, JSON };

/**
 * @brief Provides a way for hooks to pass results back to Webhook
 *
 * Webhook needs a way for hooks to provide reply data back to users. A
 * ReplyStream is a wrapper around stringstream that makes it easier
 * to append webpage structure.
 *
 * @note Future versions of this may encode the information in different
 * formats (eg JSON), but the current version only does Text and HTML.
 */
class ReplyStream {

public:
  ReplyStream(ReplyStreamType format, std::string title, std::stringstream *existing_sstream);
  ReplyStream(const std::map<std::string, std::string> &input_args, std::string title, std::stringstream *existing_sstream);

  
  void mkSection(std::string label, int heading_level=1);
  void mkText(std::string text);
  void mkTable(const std::vector<std::pair<std::string,std::string>> entries, std::string label="", bool highlight_top=true);
  void mkTable(const std::map<std::string,std::string> entries, std::string label="", bool highlight_top=true);
  void mkTable(const std::vector<std::vector<std::string>> entries, std::string label="", bool highlight_top=true);

  
  void tableBegin(std::string label, int heading_level=1);
  void tableTop(const std::vector<std::string> col_names);
  void tableRow(const std::vector<std::string> cols);
  void tableEnd();

  
  void mkList( const std::vector<std::string> &entries, const std::string &label="");
  
  void Finish();
  
private:
  ReplyStreamType format;
  std::stringstream *ss; //Reference to an external stringstream
};

} // namespace faodel


#endif // FAODEL_COMMON_REPLYSTREAM_HH
