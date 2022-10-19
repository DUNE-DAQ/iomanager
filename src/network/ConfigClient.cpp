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
#include <chrono>

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
  m_active=true;
  m_thread=std::thread([this](){
    while (m_active) {
      try {
        publish();
      }
      catch (std::exception& ex) {
        ers::warning(PublishException(ERS_HERE,ex.what()));
      }
      std::this_thread::sleep_for(1s);
    }
  });
}

ConfigClient::~ConfigClient() {
  m_active=false;
  retract();
  if (m_thread.joinable()) {
    m_thread.join();
  }
}

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
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_connectionMap[connectionId]=uri;
  }
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



void
ConfigClient::publish(const std::vector<std::string>& connectionId,
                      const std::vector<std::string>& uri)
{
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (unsigned int entry=0; entry<connectionId.size(); entry++) {
      m_connectionMap[connectionId[entry]]=uri[entry];
    }
  }
  publish();
}

void
ConfigClient::publish() {
  json content{{"partition",m_partition}};
  json connections=json::array();
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto entry: m_connectionMap) {
      json item;
      item["connection_id"]=entry.first;
      item["uri"]=entry.second;
      connections.push_back(item);
    }
    if (connections.size()==0) {
      return;
    }
  }
  content["connections"]=connections;
  http::request<http::string_body> req{ http::verb::post, "/publishM", 11 };
  req.set(http::field::content_type, "application/json");
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

void ConfigClient::retract() {
  json connections=json::array();
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto con: m_connectionMap) {
      json item;
      item["connection_id"]=con.first;
      connections.push_back(item);
    }
    m_connectionMap.clear();
  }
  if (connections.size()>0) {
    http::request<http::string_body> req{ http::verb::post, "/retract", 11 };
    req.set(http::field::content_type, "application/json");
    json body{{"partition",m_partition}};
    body["connections"]=connections;
    req.body() = body.dump();
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
}  

void ConfigClient::retract(const std::string& connectionId) {
  retract(std::vector<std::string>{connectionId});
}

void ConfigClient::retract(const std::vector<std::string>& connectionId) {
  http::request<http::string_body> req{ http::verb::post, "/retract", 11 };
  req.set(http::field::content_type, "application/json");

  json connections=json::array();
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto con: connectionId) {
      json item;
      item["connection_id"]=con;
      connections.push_back(item);
      auto miter=m_connectionMap.find(con);
      m_connectionMap.erase(miter);
    }
  }
  json body{{"partition",m_partition}};
  body["connections"]=connections;
  req.body() = body.dump();
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
