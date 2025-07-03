/**
 * @file IOManager.cpp
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "iomanager/IOManager.hpp"

#include <memory>
#include <set>
#include <string>
#include <vector>

namespace dunedaq::iomanager {

std::shared_ptr<IOManager> IOManager::s_instance = nullptr;

void
IOManager::configure(std::string session,
                                         std::vector<const confmodel::Queue*> queues,
                                         std::vector<const confmodel::NetworkConnection*> connections,
                                         const confmodel::ConnectivityService* connection_service,
                                         dunedaq::opmonlib::OpMonManager& opmgr)
{
  m_session = session;

  QueueRegistry::get().configure(queues, opmgr);
  NetworkManager::get().configure(session, connections, connection_service, opmgr);
}

void
IOManager::shutdown()
{
  QueueRegistry::get().shutdown();
  NetworkManager::get().shutdown();
  m_senders.clear();
  m_receivers.clear();
}

void
IOManager::reset()
{
  QueueRegistry::get().reset();
  NetworkManager::get().reset();
  m_senders.clear();
  m_receivers.clear();
  s_instance = nullptr;
}

std::set<std::string>
IOManager::get_datatypes(std::string const& uid)
{
  auto output = QueueRegistry::get().get_datatypes(uid);
  auto networks = NetworkManager::get().get_datatypes(uid);
  for (auto& dt : networks) {
    output.insert(dt);
  }
  return output;
}

} // namespace dunedaq::iomanager
