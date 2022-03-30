/**
 * @file IOManager.hpp
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef IOMANAGER_INCLUDE_IOMANAGER_IOMANAGER_HPP_
#define IOMANAGER_INCLUDE_IOMANAGER_IOMANAGER_HPP_

#include "nlohmann/json.hpp"

#include "iomanager/ConnectionId.hpp"
#include "iomanager/Receiver.hpp"
#include "iomanager/Sender.hpp"
#include "iomanager/SerializerRegistry.hpp"

#include <map>
#include <memory>

namespace dunedaq {
ERS_DECLARE_ISSUE(iomanager, ConnectionNotFound, "Connection with uid " << conn_uid << " not found!", ((std::string)conn_uid))

namespace iomanager {

/**
 * @class IOManager
 * Wrapper class for sockets and SPSC circular buffers.
 *   Makes the communication between DAQ processes easier and scalable.
 */
class IOManager
{

public:
  IOManager() {}

  IOManager(const IOManager&) = delete;            ///< IOManager is not copy-constructible
  IOManager& operator=(const IOManager&) = delete; ///< IOManager is not copy-assignable
  IOManager(IOManager&&) = delete;                 ///< IOManager is not move-constructible
  IOManager& operator=(IOManager&&) = delete;      ///< IOManager is not move-assignable

  void configure(ConnectionIds_t connections) { m_connections = connections; }

  template<typename Datatype>
  std::shared_ptr<SenderConcept<Datatype>> get_sender(ConnectionRef const& conn_ref)
  {
    if (!m_senders.count(conn_ref)) {
      // create from lookup service's factory function
      // based on connID we know if it's queue or network
      auto conn_id = ref_to_id(conn_ref);
      if (conn_id.service_type == ServiceType::kQueue) { // if queue
        TLOG() << "Creating QueueSenderModel for service_name " << conn_id.uid;
        m_senders[conn_ref] = std::make_shared<QueueSenderModel<Datatype>>(QueueSenderModel<Datatype>(conn_id, conn_ref));
      } else {
        TLOG() << "Creating NetworkSenderModel for service_name " << conn_id.uid;
        m_senders[conn_ref] = std::make_shared<NetworkSenderModel<Datatype>>(NetworkSenderModel<Datatype>(conn_id, conn_ref));
      }
    }
    return std::dynamic_pointer_cast<SenderConcept<Datatype>>(m_senders[conn_ref]);
  }

  template<typename Datatype>
  std::shared_ptr<ReceiverConcept<Datatype>> get_receiver(ConnectionRef const& conn_ref)
  {
    if (!m_receivers.count(conn_ref)) {
      auto conn_id = ref_to_id(conn_ref);
      if (conn_id.service_type == ServiceType::kQueue) { // if queue
        TLOG() << "Creating QueueReceiverModel for service_name " << conn_id.uid;
        m_receivers[conn_ref] = std::make_shared<QueueReceiverModel<Datatype>>(QueueReceiverModel<Datatype>(conn_id, conn_ref));
      } else {
        TLOG() << "Creating NetworkReceiverModel for service_name " << conn_id.uid;
        m_receivers[conn_ref] =
          std::make_shared<NetworkReceiverModel<Datatype>>(NetworkReceiverModel<Datatype>(conn_id, conn_ref));
      }
    }
    return std::dynamic_pointer_cast<ReceiverConcept<Datatype>>(m_receivers[conn_ref]); // NOLINT
  }

  template<typename Datatype>
  void add_callback(ConnectionRef const& conn_ref, std::function<void(Datatype&)> callback)
  {
    auto receiver = get_receiver<Datatype>(conn_ref);
    receiver->add_callback(callback);
  }

  template<typename Datatype>
  void remove_callback(ConnectionRef const& conn_ref)
  {
    auto receiver = get_receiver<Datatype>(conn_ref);
    receiver->remove_callback();
  }

  private:
      // TODO: Eric Flumerfelt <eflumerf@github.com>, 30-Mar-2022: Replace with service lookup
    ConnectionId ref_to_id(ConnectionRef const& ref)
  {
    for (auto& conn : m_connections) {
      if (conn.uid == ref.uid)
        return conn;
    }

    throw ConnectionNotFound(ERS_HERE, ref.uid);
  }

  using SenderMap = std::map<ConnectionRef, std::shared_ptr<Sender>>;
  using ReceiverMap = std::map<ConnectionRef, std::shared_ptr<Receiver>>;
  ConnectionIds_t m_connections;
  SenderMap m_senders;
  ReceiverMap m_receivers;
  SerializerRegistry m_serdes_reg;
};

} // namespace iomanager
} // namespace dunedaq

#endif // IOMANAGER_INCLUDE_IOMANAGER_IOMANAGER_HPP_
