/**
 * @file QueueRegistry.cpp
 *
 * The QueueRegistry class implementation
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "iomanager/queue/QueueRegistry.hpp"

#include <map>
#include <memory>
#include <string>

namespace dunedaq::iomanager {

std::unique_ptr<QueueRegistry> QueueRegistry::s_instance = nullptr;

QueueRegistry&
QueueRegistry::get()
{
  if (!s_instance) {
    s_instance.reset(new QueueRegistry());
  }
  return *s_instance;
}

void
QueueRegistry::configure(const std::vector<QueueConfig>& configs, opmonlib::OpMonManager & mgr)
{
  if (m_configured) {
    throw QueueRegistryConfigured(ERS_HERE);
  }

  m_queue_configs = configs;

  mgr.register_node("queues", m_opmon_link);
  
  m_configured = true;
}


bool
QueueRegistry::has_queue(const std::string& uid, const std::string& data_type) const
{
  for (auto& config : m_queue_configs) {
    if (config.id.uid == uid && config.id.data_type == data_type) {
      return true;
    }
  }

  return false;
}

std::set<std::string>
QueueRegistry::get_datatypes(const std::string& uid) const
{
  std::set<std::string> output;

  for (auto& config : m_queue_configs) {
    if (config.id.uid == uid) {
      output.insert(config.id.data_type);
    }
  }

  return output;
}

} // namespace dunedaq::iomanager
