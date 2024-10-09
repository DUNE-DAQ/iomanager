/**
 * @file IOManager.hpp
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef IOMANAGER_INCLUDE_IOMANAGER_IOMANAGER_HPP_
#define IOMANAGER_INCLUDE_IOMANAGER_IOMANAGER_HPP_

#include "iomanager/Receiver.hpp"
#include "iomanager/SchemaUtils.hpp"
#include "iomanager/Sender.hpp"
#include "opmonlib/OpMonManager.hpp"

#include "confmodel/ConnectivityService.hpp"
#include "confmodel/NetworkConnection.hpp"
#include "confmodel/Queue.hpp"

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <string>

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
  static std::shared_ptr<IOManager> get()
  {
    if (!s_instance)
      s_instance = std::shared_ptr<IOManager>(new IOManager());

    return s_instance;
  }

  IOManager(const IOManager&) = delete;            ///< IOManager is not copy-constructible
  IOManager& operator=(const IOManager&) = delete; ///< IOManager is not copy-assignable
  IOManager(IOManager&&) = delete;                 ///< IOManager is not move-constructible
  IOManager& operator=(IOManager&&) = delete;      ///< IOManager is not move-assignable

  void configure(std::string session,
                 std::vector<const confmodel::Queue*> queues,
                 std::vector<const confmodel::NetworkConnection*> connections,
                 const confmodel::ConnectivityService* connection_service,
                 opmonlib::OpMonManager&);

  void reset();
  void shutdown();

  template<typename Datatype>
  std::shared_ptr<SenderConcept<Datatype>> get_sender(ConnectionId id);

  template<typename Datatype>
  std::shared_ptr<SenderConcept<Datatype>> get_sender(std::string const& uid);

  template<typename Datatype>
  std::shared_ptr<ReceiverConcept<Datatype>> get_receiver(ConnectionId id);

  template<typename Datatype>
  std::shared_ptr<ReceiverConcept<Datatype>> get_receiver(std::string const& uid);

  template<typename Datatype>
  void add_callback(ConnectionId const& id, std::function<void(Datatype&)> callback);

  template<typename Datatype>
  void add_callback(std::string const& uid, std::function<void(Datatype&)> callback);

  template<typename Datatype>
  void remove_callback(ConnectionId const& id);

  template<typename Datatype>
  void remove_callback(std::string const& uid);

  std::set<std::string> get_datatypes(std::string const& uid);

private:
  IOManager() {}

  using SenderMap = std::map<ConnectionId, std::shared_ptr<Sender>>;
  using ReceiverMap = std::map<ConnectionId, std::shared_ptr<Receiver>>;
  SenderMap m_senders;
  ReceiverMap m_receivers;
  std::string m_session;

  static std::shared_ptr<IOManager> s_instance;
};

} // namespace iomanager

} // namespace dunedaq

#include "detail/IOManager.hxx"

#endif // IOMANAGER_INCLUDE_IOMANAGER_IOMANAGER_HPP_
