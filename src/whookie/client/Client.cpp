// Copyright 2023 National Technology & Engineering Solutions of Sandia, LLC
// (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.

#include <iostream>
#include <istream>
#include <ostream>
#include <sstream>
#include <string>
#include <boost/asio.hpp>

#include "whookie/client/Client.hh"

using namespace std;
using boost::asio::ip::tcp;

namespace whookie {

/**
 * @brief Connect to a destination and request data
 * @param[in] nid The Faodel node id to connect to
 * @param[in] path The path in the url
 * @param[out] data The returned data string
 * @retval 0 Success
 * @retval -1 Server did not speak http protocol
 * @retval -2 Server did not reply with a status code of 200
 * @retval -3 Server spoke http, but there was an exception during the reply
 */
int retrieveData(faodel::nodeid_t nid, const std::string &path, std::string *data){
  string server,port;
  nid.GetIPPort(&server,&port);
  return retrieveData(server, port, path, data);
}

/**
 * @brief Connect to a destination and request data
 * @param[in] server The IP address of the server
 * @param[in] port The TCP port to use
 * @param[in] path The path part of the url
 * @param[out] data The returned data string
 * @retval 0 Success
 * @retval -1 Server did not speak http protocol
 * @retval -2 Server did not reply with a status code of 200
 * @retval -3 Server spoke http, but there was an exception during the reply
 */
int retrieveData(const string &server, const string &port, const string &path, string *data){

  stringstream result_stream;

  try {
    boost::asio::io_service io_service;

    // Get a list of endpoints corresponding to the server name.
    tcp::resolver resolver(io_service);
    tcp::resolver::query query(server, port);
    tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
  
    // Try each endpoint until we successfully establish a connection.
    tcp::socket socket(io_service);
    boost::asio::connect(socket, endpoint_iterator);

    // Form the request. We specify the "Connection: close" header so that the
    // server will close the socket after transmitting the response. This will
    // allow us to treat all data up until the EOF as the content.
    boost::asio::streambuf request_buf;
    std::ostream request_stream(&request_buf);
    request_stream << "GET " << path << " HTTP/1.0\r\n";
    request_stream << "Host: " << server << "\r\n";
    request_stream << "Accept: */*\r\n";
    request_stream << "Connection: close\r\n\r\n";

    // Send the request.
    boost::asio::write(socket, request_buf);
    
    // Read the response status line. The response streambuf will automatically
    // grow to accommodate the entire line. The growth may be limited by passing
    // a maximum size to the streambuf constructor.
    boost::asio::streambuf response;
    boost::asio::read_until(socket, response, "\r\n");

    // Check that response is OK.
    std::istream response_stream(&response);
    std::string http_version;
    response_stream >> http_version;
    unsigned int status_code;
    response_stream >> status_code;
    std::string status_message;
    std::getline(response_stream, status_message);
    if (!response_stream || http_version.substr(0, 5) != "HTTP/")    {
      //std::cout << "Invalid response\n";
      return -1;
    }
    if (status_code != 200) {
      //std::cout << "Response returned with status code " << status_code << "\n";
      return -2;
    }

    // Read the response headers, which are terminated by a blank line.
    boost::asio::read_until(socket, response, "\r\n\r\n");

    // Process the response headers.
    std::string header;
    while (std::getline(response_stream, header) && header != "\r"){ }
      //  std::cout << header << "\n";
    //std::cout << "\n";
  
    // Write whatever content we already have to output.
    if (response.size() > 0)
      result_stream << &response;
    
    // Read until EOF, writing data to output as we go.
    boost::system::error_code error;
    while (boost::asio::read(socket, response,
                             boost::asio::transfer_at_least(1), error))
      result_stream << &response;
  } catch (exception &e){
    return -3;
  }

  if(data!=nullptr) *data=result_stream.str();
  
  return 0;
}

/**
 * @brief Connect to a destination and request data
 * @param[in] server The IP address of the server
 * @param[in] port The TCP port to use
 * @param[in] path The path part of the url
 * @param[out] data The returned data string
 * @retval 0 Success
 * @retval -1 Server did not speak http protocol
 * @retval -2 Server did not reply with a status code of 200
 * @retval -3 Server spoke http, but there was an exception during the reply
 */
int retrieveData(const std::string &server, unsigned int port, const string &path, string *data){
  stringstream ss;  ss<<port;
  string sport = ss.str();
  return  retrieveData(server, sport, path, data);
}



} // namespace whookie
