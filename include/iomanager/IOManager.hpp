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

#include "iomanager/ConnectionID.hpp"
#include "iomanager/Receiver.hpp"
#include "iomanager/Sender.hpp"
#include "iomanager/SerializerRegistry.hpp"

#include <map>
#include <memory>

namespace dunedaq {
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

  template<typename Datatype>
  std::shared_ptr<SenderConcept<Datatype>> get_sender(ConnectionID conn_id)
  {
    if (!m_senders.count(conn_id)) {
      // create from lookup service's factory function
      // based on connID we know if it's queue or network
      if (conn_id.m_service_type == "queue") { // if queue
        TLOG() << "Creating QueueSenderModel for service_name " << conn_id.m_service_name;
        m_senders[conn_id] = std::make_shared<QueueSenderModel<Datatype>>(QueueSenderModel<Datatype>(conn_id));
      } else {
        TLOG() << "Creating NetworkSenderModel for service_name " << conn_id.m_service_name;
        m_senders[conn_id] = std::make_shared<NetworkSenderModel<Datatype>>(NetworkSenderModel<Datatype>(conn_id));
      }
    }
    return std::dynamic_pointer_cast<SenderConcept<Datatype>>(m_senders[conn_id]);
  }

  template<typename Datatype>
  std::shared_ptr<ReceiverConcept<Datatype>> get_receiver(ConnectionID conn_id)
  {
    if (!m_receivers.count(conn_id)) {
      if (conn_id.m_service_type == "queue") { // if queue
        TLOG() << "Creating QueueReceiverModel for service_name " << conn_id.m_service_name;
        m_receivers[conn_id] = std::make_shared<QueueReceiverModel<Datatype>>(QueueReceiverModel<Datatype>(conn_id));
      } else {
        TLOG() << "Creating NetworkReceiverModel for service_name " << conn_id.m_service_name;
        m_receivers[conn_id] =
          std::make_shared<NetworkReceiverModel<Datatype>>(NetworkReceiverModel<Datatype>(conn_id));
      }
    }
    return std::dynamic_pointer_cast<ReceiverConcept<Datatype>>(m_receivers[conn_id]); // NOLINT
  }

  template<typename Datatype>
  void add_callback(ConnectionID conn_id, std::function<void(Datatype&)> callback)
  {
    auto receiver = get_receiver<Datatype>(conn_id);
    receiver->add_callback(callback);
  }

  template<typename Datatype>
  void remove_callback(ConnectionID conn_id)
  {
    auto receiver = get_receiver<Datatype>(conn_id);
    receiver->remove_callback();
  }

  using SenderMap = std::map<ConnectionID, std::shared_ptr<Sender>>;
  using ReceiverMap = std::map<ConnectionID, std::shared_ptr<Receiver>>;
  SenderMap m_senders;
  ReceiverMap m_receivers;
  SerializerRegistry m_serdes_reg;
};

} // namespace iomanager
} // namespace dunedaq

#endif // IOMANAGER_INCLUDE_IOMANAGER_IOMANAGER_HPP_
