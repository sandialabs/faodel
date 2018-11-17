// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

#include <iostream>
#include <algorithm>

#include "faodel-common/QuickHTML.hh"
#include "faodel-common/ReplyStream.hh"


using namespace std;

namespace faodel {

/**
 * @brief Standard ctor for a creating a new reply stream
 * @param[in] format What type of formatting should be applied
 * @param[in] title The title of the webpage that gets generated
 * @param[in] existing_sstream A stream that this will append into
 */
ReplyStream::ReplyStream(ReplyStreamType format, string title, stringstream *existing_sstream)
        : format(format), ss(existing_sstream) {
  switch (format) {
    case ReplyStreamType::TEXT:
      break;
    case ReplyStreamType::HTML:
      html::mkHeader(*ss, title);
      break;
    default:
      cerr << "Unsupported format in ReplyStream\n";
      exit(-1);
  }
}

/**
 * @brief ReplyStream ctor that parses args from a map (currently only supports 'format')
 * @param[in] input_args A map of input parameters to pass into the stream
 * @param[in] title Title of the page
 * @param[in] existing_sstream An existing streamstream to append
 */
ReplyStream::ReplyStream(const map<string, string> &input_args, string title, stringstream *existing_sstream)
        : ss(existing_sstream) {

  format = ReplyStreamType::HTML; //Default

  auto fmt = input_args.find("format");
  if (fmt != input_args.end()) {
    string val = fmt->second;
    std::transform(val.begin(), val.end(), val.begin(), ::tolower);
    if (val == "text" || val == "txt") format = ReplyStreamType::TEXT;
    else if (val == "html") format = ReplyStreamType::HTML;
    else cout << "Ignoring bad or unsupported format: " << val << endl;
  }


  switch (format) {
    case ReplyStreamType::TEXT:
      break; //No Header
    case ReplyStreamType::HTML:
      html::mkHeader(*ss, title);
      break;
    default:
      cerr << "Unsupported format in ReplyStream\n";
      exit(-1);
  }
}

/**
 * @brief Add a new section header to the stream
 * @param[in] label The label to use for the section header
 * @param[in] heading_level The level of the section (1=most important, 2=subsection, 3=subsubsection)
 */
void ReplyStream::mkSection(string label, int heading_level) {

  switch (format) {
    case ReplyStreamType::TEXT:
      *ss << label << endl;
      break;
    case ReplyStreamType::HTML:
      html::mkSection(*ss, label, heading_level);
      break;
    default:
      cerr << "Unsupported format in ReplyStream\n";
      exit(-1);
  }
}

/**
 * @brief Insert a plain chunk of text into the stream
 * @param[in] text The text to insert (html version wraps this with paragraph markers)
 */
void ReplyStream::mkText(string text) {
  switch (format) {
    case ReplyStreamType::TEXT:
      *ss << text << endl;
      break;
    case ReplyStreamType::HTML:
      *ss << "<p>" << text << "</p>\n";
      break;
    default:
      cerr << "Unsupported format in ReplyStream\n";
      exit(-1);
  }
}

/**
 * @brief Insert a 2D table into the stream and give it a label
 * @param[in] entries A vector of string pairs (one pair per row)
 * @param[in] label An optional label to put on the table
 * @param[in] highlight_top When true, use a different color to highlight first row
 */
void ReplyStream::mkTable(const vector<std::pair<string, string>> entries, string label, bool highlight_top) {

  switch (format) {
    case ReplyStreamType::TEXT:
      if (!label.empty())
        *ss << label << endl;
      for (auto &val1_val2 : entries)
        *ss << val1_val2.first << "\t" << val1_val2.second << endl;
      break;

    case ReplyStreamType::HTML:
      html::mkTable(*ss, entries, label, highlight_top);
      break;

    default:
      cerr << "Unsupported format in ReplyStream\n";
      exit(-1);
  }
}

/**
 * @brief Create a table from a list of key/values
 * @param[in] entries A key/value data structure to dump
 * @param[in] label An optional label for the table
 * @param[in] highlight_top When true, highlight the first entry
 */
void ReplyStream::mkTable(const map<string, string> entries, string label, bool highlight_top) {

  switch (format) {
    case ReplyStreamType::TEXT:
      if (label != "")
        *ss << label << endl;
      for (auto &val1_val2 : entries)
        *ss << val1_val2.first << "\t" << val1_val2.second << endl;
      break;

    case ReplyStreamType::HTML:
      html::mkTable(*ss, entries, label, highlight_top);
      break;

    default:
      cerr << "Unsupported format in ReplyStream\n";
      exit(-1);
  }

}

/**
 * @brief Make an MxN table of strings
 * @param[in] entries A vector of string vectors to dump
 * @param[in] label An optional label for the table
 * @param[in] highlight_top When true, highlight the first entry
 */
void ReplyStream::mkTable(const vector<vector<string>> entries, string label, bool highlight_top) {

  switch (format) {
    case ReplyStreamType::TEXT:
      if (!label.empty())
        *ss << label << endl;
      for (auto &row : entries) {
        for (auto &col : row) {
          *ss << col << "\t";
        }
        *ss << endl;
      }
      break;

    case ReplyStreamType::HTML:
      html::mkTable(*ss, entries, label, highlight_top);
      break;

    default:
      cerr << "Unsupported format in ReplyStream\n";
      exit(-1);
  }
}

/**
 * @brief A beginning function for manually generating a table
 * @param[in] label The section label for the table
 * @param[in] heading_level The level of the section label
 */
void ReplyStream::tableBegin(string label, int heading_level) {
  switch (format) {
    case ReplyStreamType::TEXT:
      if (label != "")
        *ss << label << endl;
      break;

    case ReplyStreamType::HTML:
      html::tableBegin(*ss, label);
      break;

    default:
      cerr << "Unsupported format in ReplyStream\n";
      exit(-1);
  }
}

/**
 * @brief When manually generating a table, use these names as column headers
 * @param[in] col_names The names of the columns
 */
void ReplyStream::tableTop(const vector<string> col_names) {
  switch (format) {
    case ReplyStreamType::TEXT:
      for (auto &col : col_names) {
        *ss << col << "\t";
      }
      *ss << endl;
      break;

    case ReplyStreamType::HTML:
      html::tableTop(*ss, col_names);
      break;

    default:
      cerr << "Unsupported format in ReplyStream\n";
      exit(-1);
  }
}

/**
 * @brief When manually generating a table, use this to append the next row
 * @param[in] cols The names of the columns
 */
void ReplyStream::tableRow(const vector<string> cols) {
  switch (format) {
    case ReplyStreamType::TEXT:
      for (auto &col : cols) {
        *ss << col << "\t";
      }
      *ss << endl;
      break;

    case ReplyStreamType::HTML:
      html::tableRow(*ss, cols);
      break;

    default:
      cerr << "Unsupported format in ReplyStream\n";
      exit(-1);
  }

}

/**
 * @brief When manually generating a table, use this to end the table
 */
void ReplyStream::tableEnd() {
  switch (format) {
    case ReplyStreamType::TEXT:
      *ss << endl;
      break;

    case ReplyStreamType::HTML:
      html::tableEnd(*ss);
      break;

    default:
      cerr << "Unsupported format in ReplyStream\n";
      exit(-1);
  }
}


/**
 * @brief Append the replystream with a (uniform) list
 * @param[in] entries The list entries
 * @param[in] label The section label for this list
 */
void ReplyStream::mkList(const vector<string> &entries, const string &label) {

  switch (format) {
    case ReplyStreamType::TEXT:
      if (!label.empty())
        *ss << label << endl;
      for (auto &val : entries)
        *ss << val << endl;
      break;

    case ReplyStreamType::HTML:
      html::mkList(*ss, entries, label);
      break;

    default:
      cerr << "Unsupported format in ReplyStream\n";
      exit(-1);
  }

}

/**
 * @brief Close out a replystream (appends any footer markup)
 */
void ReplyStream::Finish() {

  switch (format) {
    case ReplyStreamType::TEXT:
      break;
    case ReplyStreamType::HTML:
      html::mkFooter(*ss);
      break;
    default:
      cerr << "Unsupported format in ReplyStream\n";
      exit(-1);
  }

}


} // namespace faodel