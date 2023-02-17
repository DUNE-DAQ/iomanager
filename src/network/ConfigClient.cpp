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

ConfigClient::ConfigClient(const std::string& server,
                           const std::string& port,
                           std::chrono::milliseconds publish_interval)
{
  char* session = getenv("DUNEDAQ_SESSION");
  if (session) {
    m_session = std::string(session);
  } else {
    session = getenv("DUNEDAQ_PARTITION");
    if (session) {
      m_session = std::string(session);
    } else {
      throw(EnvNotFound(ERS_HERE, "DUNEDAQ_SESSION"));
    }
  }

  tcp::resolver resolver(m_ioContext);
  m_addr = resolver.resolve(server, port);
  m_active = true;
  m_thread = std::thread([this, publish_interval]() {
    while (m_active) {
      try {
        publish();
        m_connected = true;
        TLOG_DEBUG(24) << "Automatic publish complete";
      } catch (ers::Issue& ex) {
        if (m_connected)
          ers::error(ex);
        else
          TLOG() << ex;
      } catch (std::exception& ex) {
        auto ers_ex = PublishException(ERS_HERE, ex.what());
        if (m_connected)
          ers::error(ers_ex);
        else
          TLOG() << ers_ex;
      }
      std::this_thread::sleep_for(publish_interval);
    }
    retract();
    if (!m_connected) {
      ers::error(PublishException(ERS_HERE, "Publish thread was unable to publish to Connectivity Service!"));
    }
  });
}

ConfigClient::~ConfigClient()
{
  m_connected = false;
  m_active = false;
  if (m_thread.joinable()) {
    m_thread.join();
  }
  retract();
}

ConnectionResponse
ConfigClient::resolveConnection(const ConnectionRequest& query, std::string session)
{
  if (session == "") {
    session = m_session;
  }
  TLOG_DEBUG(25) << "Getting connections matching <" << query.uid_regex << "> in session " << session;
  std::string target = "/getconnection/" + session;
  http::request<http::string_body> req{ http::verb::post, target, 11 };
  req.set(http::field::content_type, "application/json");
  nlohmann::json jquery = query;
  req.body() = jquery.dump();
  req.prepare_payload();

  http::response<http::string_body> response;
  try {
    boost::beast::tcp_stream stream(m_ioContext);
    stream.connect(m_addr);
    http::write(stream, req);

    boost::beast::flat_buffer buffer;
    http::read(stream, buffer, response);

    beast::error_code ec;
    stream.socket().shutdown(tcp::socket::shutdown_both, ec);
    TLOG_DEBUG(25) << "get " << target << " response: " << response;

    if (response.result_int() != 200) {
      throw(FailedLookup(ERS_HERE, query.uid_regex, target, std::string(response.reason())));
    }
  } catch (ers::Issue const&) {
    m_connected = false;
    throw;
  } catch (std::exception const& ex) {
    m_connected = false;
    ers::error(FailedLookup(ERS_HERE, query.uid_regex, target, ex.what()));
    return ConnectionResponse();
  }
  m_connected = true;
  json result = json::parse(response.body());
  TLOG_DEBUG(25) << result.dump();
  ConnectionResponse res;
  for (auto item : result) {
    res.connections.emplace_back(item.get<ConnectionInfo>());
  }
  return res;
}

void
ConfigClient::publish(ConnectionRegistration const& connection)
{
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    TLOG_DEBUG(26) << "Adding connection with UID " << connection.uid << " and URI " << connection.uri
                   << " to publish list";

    m_registered_connections.insert(connection);
  }
}

void
ConfigClient::publish(const std::vector<ConnectionRegistration>& connections)
{
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& entry : connections) {
      TLOG_DEBUG(26) << "Adding connection with UID " << entry.uid << " and URI " << entry.uri << " to publish list";

      m_registered_connections.insert(entry);
    }
  }
}

void
ConfigClient::publish()
{
  json content{ { "partition", m_session } };
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
  http::request<http::string_body> req{ http::verb::post, "/publish", 11 };
  req.set(http::field::content_type, "application/json");
  req.body() = content.dump();
  req.prepare_payload();

  try {
    boost::beast::tcp_stream stream(m_ioContext);
    stream.connect(m_addr);
    http::write(stream, req);

    http::response<http::string_body> response;
    boost::beast::flat_buffer buffer;
    http::read(stream, buffer, response);
    beast::error_code ec;
    stream.socket().shutdown(tcp::socket::shutdown_both, ec);
    if (response.result_int() != 200) {
      throw(FailedPublish(ERS_HERE, std::string(response.reason())));
    }
  } catch (ers::Issue const&) {
    m_connected = false;
    throw;
  } catch (std::exception const& ex) {
    m_connected = false;
    throw(FailedPublish(ERS_HERE, ex.what(), ex));
  }
  m_connected = true;
}

void
ConfigClient::retract()
{
  TLOG() << "retract() called, getting connection information";
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
  TLOG() << "retract(): Retracting " << connections.size() << " connections";
  if (connections.size() > 0) {
    http::request<http::string_body> req{ http::verb::post, "/retract", 11 };
    req.set(http::field::content_type, "application/json");
    json body{ { "partition", m_session } };
    body["connections"] = connections;
    req.body() = body.dump();
    req.prepare_payload();

    try {
      boost::beast::tcp_stream stream(m_ioContext);
      stream.connect(m_addr);
      http::write(stream, req);
      http::response<http::string_body> response;
      boost::beast::flat_buffer buffer;
      http::read(stream, buffer, response);
      beast::error_code ec;
      stream.socket().shutdown(tcp::socket::shutdown_both, ec);
      if (response.result_int() != 200) {
        throw(FailedRetract(ERS_HERE, "connection Id vector", std::string(response.reason())));
      }
    } catch (ers::Issue const&) {
      m_connected = false;
      throw;
    } catch (std::exception const& ex) {
      m_connected = false;
      ers::error(FailedRetract(ERS_HERE, "connection Id vector", ex.what()));
    }
    m_connected = true;
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
  json body{ { "partition", m_session } };
  body["connections"] = connections;
  req.body() = body.dump();
  req.prepare_payload();

  try {
    boost::beast::tcp_stream stream(m_ioContext);
    stream.connect(m_addr);
    http::write(stream, req);
    http::response<http::string_body> response;
    boost::beast::flat_buffer buffer;
    http::read(stream, buffer, response);
    beast::error_code ec;
    stream.socket().shutdown(tcp::socket::shutdown_both, ec);
    if (response.result_int() != 200) {
      throw(FailedRetract(ERS_HERE, "connection Id vector", std::string(response.reason())));
    }
  } catch (ers::Issue const&) {
    m_connected = false;
    throw;
  } catch (std::exception const& ex) {
    m_connected = false;
    ers::error(FailedRetract(ERS_HERE, "connection Id vector", ex.what()));
  }
  m_connected = true;
}
