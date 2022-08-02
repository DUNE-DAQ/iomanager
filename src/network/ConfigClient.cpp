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

ConnectionResponse
ConfigClient::resolveEndpoint(ConnectionRequest const&)
{
  json query;
  query["data_type"]=endpoint.data_type;
  query["app_name"]=endpoint.app_name;
  query["module_name"]=endpoint.module_name;
  if (endpoint.source_id.subsystem != Subsystem::kUnknown) {
    std::ostringstream source_id;
    source_id << str(endpoint.source_id.subsystem) << "_" << endpoint.source_id.id;
    query["source_id"]=source_id.str();
  }

  std::string target = "/getendpoint/" + m_partition;
  std::string params = "endpoint=" + query.dump();
  json result=json::parse(get(target, params));
  std::cout << "result: " << result.dump() << std::endl;
  ConnectionResponse output;
  for (auto uri: result["uris"]) {
    output.uris.emplace_back(uri.dump());
  }
  output.connection_type=result["connection_type"].dump();
  return output;
}

std::string
ConfigClient::resolveConnection(const Connection& connection){
  json query=jsonify(connection);
  std::string target = "/getconnection/" + m_partition;
  std::string params="endpoint=" + query["bind_endpoint"].dump();
  json result=json::parse(get(target,params));
  return result["uri"].dump();
}

void
ConfigClient::publishEndpoint(const Endpoint& endpoint, const std::string& uri, const std::string& connection_type)
{
  json content;
  content["data_type"]=endpoint.data_type;
  content["app_name"]=endpoint.app_name;
  content["module_name"]=endpoint.module_name;
  if (endpoint.source_id.subsystem != Subsystem::kUnknown) {
    std::ostringstream source_id;
    source_id << str(endpoint.source_id.subsystem) << "_" << endpoint.source_id.id;
    content["source_id"]=source_id.str();
  }
  if (connection_type=="") {
    publish("&endpoint=" + content.dump() + "&uri=" + uri);
  }
  else {
    publish("&endpoint=" + content.dump() + "&uri=" + uri +
            "&connection_type=" + connection_type);
  }
}

void
ConfigClient::publishConnection(const Connection& connection)
{
  json content=jsonify(connection);
  publish("&connection="+content.dump());
}
void
ConfigClient::publish(const std::string& content)
{

  std::string target = "/publish";
  http::request<http::string_body> req{ http::verb::post, target, 11 };
  req.set(http::field::content_type, "application/x-www-form-urlencoded");

  req.body() = "partition=" + m_partition + content;
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

void ConfigClient::retract(const Endpoint& endpoint) {
  json content;
  content["data_type"]=endpoint.data_type;
  content["app_name"]=endpoint.app_name;
  content["module_name"]=endpoint.module_name;
  if (endpoint.source_id.subsystem != Subsystem::kUnknown) {
    std::ostringstream source_id;
    source_id << str(endpoint.source_id.subsystem) << "_" << endpoint.source_id.id;
    content["source_id"]=source_id.str();
  }
  retract("&endpoint=" + content.dump());
}

void ConfigClient::retract(const Connection& connection){
  json content=jsonify(connection);
  retract("&connection=" + content.dump());
}


void
ConfigClient::retract(const std::string& content)
{
  std::string target = "/retract";
  http::request<http::string_body> req{ http::verb::post, target, 11 };
  req.set(http::field::content_type, "application/x-www-form-urlencoded");
  req.body() = "partition=" + m_partition + content;
  req.prepare_payload();
  m_stream.connect(m_addr);
  http::write(m_stream, req);
  http::response<http::string_body> response;
  http::read(m_stream, m_buffer, response);
  beast::error_code ec;
  m_stream.socket().shutdown(tcp::socket::shutdown_both, ec);
  if (response.result_int() != 200) {
    throw(FailedRetract(ERS_HERE, content, std::string(response.reason())));
  }
}

std::string
ConfigClient::get(const std::string& target, const std::string& params)
{
  std::cout << "Getting <" << target << ">\n";
  http::request<http::string_body> req{ http::verb::post, target, 11 };
  req.set(http::field::content_type, "application/x-www-form-urlencoded");
  req.body()=params;
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
  return response.body();
}

json 
ConfigClient::jsonify(const Connection& connection) {
  json content;
  content["bind_endpoint"]["data_type"]=connection.bind_endpoint.data_type;
  content["bind_endpoint"]["app_name"]=connection.bind_endpoint.app_name;
  content["bind_endpoint"]["module_name"]=connection.bind_endpoint.module_name;
  content["bind_endpoint"]["direction"]=str(connection.bind_endpoint.direction);
  std::ostringstream source_id;
  source_id << str(connection.bind_endpoint.source_id.subsystem)
            << "_" << connection.bind_endpoint.source_id.id;
  content["bind_endpoint"]["source_id"]=source_id.str();
  content["connection_type"]=str(connection.connection_type);
  content["uri"]=connection.uri;

  return content;
}
