/**
 * @file ConfigClient.cpp
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "iomanager/network/ConfigClient.hpp"
#include "iomanager/network/ConfigClientIssues.hpp"

#include "logging/Logging.hpp"

#include <boost/beast/http.hpp>

#include <cstdlib>
#include <iostream>
#include <string>

using tcp = net::ip::tcp;     // from <boost/asio/ip/tcp.hpp>
namespace http = beast::http; // from <boost/beast/http.hpp>
using nlohmann::json;

using namespace dunedaq::iomanager;

ConfigClient::ConfigClient(const std::string& server, const std::string& port)
  : m_stream(m_ioContext)
{
  char* part = getenv("DUNEDAQ_PARTITION");
  if (part) {
    m_partition = std::string(part);
  } else {
    throw(EnvNotFound(ERS_HERE, "DUNEDAQ_PARTITION"));
  }

  tcp::resolver resolver(m_ioContext);
  m_addr = resolver.resolve(server, port);
}

ConfigClient::~ConfigClient() {}

std::vector<std::string>
ConfigClient::resolveConnection(const std::string& query){
  TLOG_DEBUG(25) << "Getting connections matching <" << query << "> in partition " << m_partition;
  std::string target = "/getconnection/" + m_partition;
  http::request<http::string_body> req{ http::verb::post, target, 11 };
  req.set(http::field::content_type, "application/x-www-form-urlencoded");
  req.body()="connection_id="+query;
  req.prepare_payload();
  m_stream.connect(m_addr);
  http::write(m_stream, req);

  http::response<http::string_body> response;
  http::read(m_stream, m_buffer, response);

  beast::error_code ec;
  m_stream.socket().shutdown(tcp::socket::shutdown_both, ec);
  TLOG_DEBUG(25) << "get " << target << " response: " << response;

  if (response.result_int() != 200) {
    throw(FailedLookup(ERS_HERE, target, std::string(response.reason())));
  }
  json result=json::parse(response.body());
  TLOG_DEBUG(25) <<  result.dump();
  std::vector<std::string> connections;
  for (std::string con: result) {
    connections.push_back(con);
  }
  return connections;
}

void
ConfigClient::publish(const std::string& connectionId,
                      const std::string& uri)
{
  http::request<http::string_body> req{ http::verb::post, "/publish", 11 };
  req.set(http::field::content_type, "application/x-www-form-urlencoded");

  req.body() = "partition=" + m_partition +
    "&connection_id=" + connectionId +
    "&uri=" +uri;
  req.prepare_payload();
  m_stream.connect(m_addr);
  http::write(m_stream, req);

  http::response<http::string_body> response;
  http::read(m_stream, m_buffer, response);
  beast::error_code ec;
  m_stream.socket().shutdown(tcp::socket::shutdown_both, ec);
  if (response.result_int() != 200) {
    throw(FailedPublish(ERS_HERE, std::string(response.reason())));
  }
}

void ConfigClient::retract(const std::string& connectionId) {
  http::request<http::string_body> req{ http::verb::post, "/retract", 11 };
  req.set(http::field::content_type, "application/x-www-form-urlencoded");
  req.body() = "partition=" + m_partition + "&connection_id=" + connectionId;
  req.prepare_payload();
  m_stream.connect(m_addr);
  http::write(m_stream, req);
  http::response<http::string_body> response;
  http::read(m_stream, m_buffer, response);
  beast::error_code ec;
  m_stream.socket().shutdown(tcp::socket::shutdown_both, ec);
  if (response.result_int() != 200) {
    throw(FailedRetract(ERS_HERE, connectionId, std::string(response.reason())));
  }
}


void
ConfigClient::publish(const std::vector<std::string>& connectionId,
                      const std::vector<std::string>& uri)
{
  http::request<http::string_body> req{ http::verb::post, "/publishM", 11 };
  req.set(http::field::content_type, "application/json");

  json content{{"partition",m_partition}};
  json connections=json::array();
  for (unsigned int entry=0; entry<connectionId.size(); entry++) {
    json item;
    item["connection_id"]=connectionId[entry];
    item["uri"]=uri[entry];
    connections.push_back(item);
  }
  content["connections"]=connections;
  req.body() = content.dump();
  req.prepare_payload();
  m_stream.connect(m_addr);
  http::write(m_stream, req);

  http::response<http::string_body> response;
  http::read(m_stream, m_buffer, response);
  beast::error_code ec;
  m_stream.socket().shutdown(tcp::socket::shutdown_both, ec);
  if (response.result_int() != 200) {
    throw(FailedPublish(ERS_HERE, std::string(response.reason())));
  }
}

void ConfigClient::retract(const std::vector<std::string>& connectionId) {
  http::request<http::string_body> req{ http::verb::post, "/retractM", 11 };
  req.set(http::field::content_type, "application/json");

  json content{{"partition",m_partition}};
  json connections=json::array();
  for (unsigned int entry=0; entry<connectionId.size(); entry++) {
    json item;
    item["connection_id"]=connectionId[entry];
    connections.push_back(item);
  }
  content["connections"]=connections;
  req.body() = content.dump();
  req.prepare_payload();
  m_stream.connect(m_addr);
  http::write(m_stream, req);
  http::response<http::string_body> response;
  http::read(m_stream, m_buffer, response);
  beast::error_code ec;
  m_stream.socket().shutdown(tcp::socket::shutdown_both, ec);
  if (response.result_int() != 200) {
    throw(FailedRetract(ERS_HERE, "connection Id vector", std::string(response.reason())));
  }
}
