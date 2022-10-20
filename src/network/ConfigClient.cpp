/**
 * @file ConfigClient.cpp
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "iomanager/network/ConfigClient.hpp"
#include "iomanager/network/NetworkIssues.hpp"

#include "logging/Logging.hpp"

#include <boost/beast/http.hpp>

#include <chrono>
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
  m_active = true;
  m_thread = std::thread([this]() {
    while (m_active) {
      try {
        publish();
      } catch (std::exception& ex) {
        ers::warning(PublishException(ERS_HERE, ex.what()));
      }
      std::this_thread::sleep_for(1s);
    }
  });
}

ConfigClient::~ConfigClient()
{
  m_active = false;
  retract();
  if (m_thread.joinable()) {
    m_thread.join();
  }
}

ConnectionResponse
ConfigClient::resolveConnection(const ConnectionRequest& query)
{
  TLOG_DEBUG(25) << "Getting connections matching <" << query.uid_regex << "> in partition " << m_partition;
  std::string target = "/getconnection/" + m_partition;
  http::request<http::string_body> req{ http::verb::post, target, 11 };
  req.set(http::field::content_type, "application/json");
  nlohmann::json jquery = query;
  req.body() = jquery.dump();
  req.prepare_payload();

  http::response<http::string_body> response;
  try {
    m_stream.connect(m_addr);
    http::write(m_stream, req);

    http::read(m_stream, m_buffer, response);

    beast::error_code ec;
    m_stream.socket().shutdown(tcp::socket::shutdown_both, ec);
    TLOG_DEBUG(25) << "get " << target << " response: " << response;

    if (response.result_int() != 200) {
      throw(FailedLookup(ERS_HERE, query.uid_regex, target, std::string(response.reason())));
    }
  } catch (std::exception const& ex) {
    ers::error(FailedLookup(ERS_HERE, query.uid_regex, target, ex.what()));
    return ConnectionResponse();
  }
  json result = json::parse(response.body());
  TLOG_DEBUG(25) << result.dump();
  ConnectionResponse res = result.get<ConnectionResponse>();
  return res;
}

void
ConfigClient::publish(ConnectionRegistration const& connection)
{
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_registered_connections.insert(connection);
  }
  http::request<http::string_body> req{ http::verb::post, "/publish", 11 };
  req.set(http::field::content_type, "application/json");

  nlohmann::json jconn = connection;
  req.body() = jconn.dump();
  req.prepare_payload();

  http::response<http::string_body> response;
  try {
    m_stream.connect(m_addr);
    http::write(m_stream, req);

    http::read(m_stream, m_buffer, response);
    beast::error_code ec;
    m_stream.socket().shutdown(tcp::socket::shutdown_both, ec);

    if (response.result_int() != 200) {
      throw(FailedPublish(ERS_HERE, std::string(response.reason())));
    }
  } catch (std::exception const& ex) {
    ers::error(FailedPublish(ERS_HERE, ex.what()));
  }
}

void
ConfigClient::publish(const std::vector<ConnectionRegistration>& connections)
{
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& entry : connections) {
      m_registered_connections.insert(entry);
    }
  }
  publish();
}

void
ConfigClient::publish()
{
  json content{ { "partition", m_partition } };
  json connections = json::array();
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& entry : m_registered_connections) {
      json item = entry;
      connections.push_back(item);
    }
    if (connections.size() == 0) {
      return;
    }
  }
  content["connections"] = connections;
  http::request<http::string_body> req{ http::verb::post, "/publishM", 11 };
  req.set(http::field::content_type, "application/json");
  req.body() = content.dump();
  req.prepare_payload();

  try {
    m_stream.connect(m_addr);
    http::write(m_stream, req);

    http::response<http::string_body> response;
    http::read(m_stream, m_buffer, response);
    beast::error_code ec;
    m_stream.socket().shutdown(tcp::socket::shutdown_both, ec);
    if (response.result_int() != 200) {
      throw(FailedPublish(ERS_HERE, std::string(response.reason())));
    }
  } catch (std::exception const& ex) {
    ers::error(FailedPublish(ERS_HERE, ex.what()));
  }
}

void
ConfigClient::retract()
{
  json connections = json::array();
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& con : m_registered_connections) {
      json item;
      item["connection_id"] = con.uid;
      item["data_type"] = con.data_type;
      connections.push_back(item);
    }
    m_registered_connections.clear();
  }
  if (connections.size() > 0) {
    http::request<http::string_body> req{ http::verb::post, "/retract", 11 };
    req.set(http::field::content_type, "application/json");
    json body{ { "partition", m_partition } };
    body["connections"] = connections;
    req.body() = body.dump();
    req.prepare_payload();

    try {
      m_stream.connect(m_addr);
      http::write(m_stream, req);
      http::response<http::string_body> response;
      http::read(m_stream, m_buffer, response);
      beast::error_code ec;
      m_stream.socket().shutdown(tcp::socket::shutdown_both, ec);
      if (response.result_int() != 200) {
        throw(FailedRetract(ERS_HERE, "connection Id vector", std::string(response.reason())));
      }
    } catch (std::exception const& ex) {
      ers::error(FailedRetract(ERS_HERE, "connection Id vector", ex.what()));
    }
  }
}

void
ConfigClient::retract(const ConnectionId& connectionId)
{
  retract(std::vector<ConnectionId>{ connectionId });
}

void
ConfigClient::retract(const std::vector<ConnectionId>& connectionIds)
{
  http::request<http::string_body> req{ http::verb::post, "/retract", 11 };
  req.set(http::field::content_type, "application/json");

  json connections = json::array();
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& con : connectionIds) {
      auto reg_it = m_registered_connections.begin();
      for (; reg_it != m_registered_connections.end(); ++reg_it) {
        if (con.uid == reg_it->uid && con.data_type == reg_it->data_type)
          break;
      }
      if (reg_it == m_registered_connections.end()) {
        ers::error(
          FailedRetract(ERS_HERE, con.uid + " of type " + con.data_type, "not in registered connections list"));
      } else {
        json item;
        item["connection_id"] = con.uid;
        item["data_type"] = con.data_type;
        connections.push_back(item);
        m_registered_connections.erase(reg_it);
      }
    }
  }
  json body{ { "partition", m_partition } };
  body["connections"] = connections;
  req.body() = body.dump();
  req.prepare_payload();

  try {
    m_stream.connect(m_addr);
    http::write(m_stream, req);
    http::response<http::string_body> response;
    http::read(m_stream, m_buffer, response);
    beast::error_code ec;
    m_stream.socket().shutdown(tcp::socket::shutdown_both, ec);
    if (response.result_int() != 200) {
      throw(FailedRetract(ERS_HERE, "connection Id vector", std::string(response.reason())));
    }
  } catch (std::exception const& ex) {
    ers::error(FailedRetract(ERS_HERE, "connection Id vector", ex.what()));
  }
}
