# iomanager

A simplified API for passing messages between DAQModules

## API Description

### IOManager

* Main point-of-contact for Unified API
* Methods to retrieve Sender/Receiver instances using ConnectionRefs or uid
* Configures Queues and NetworkManager automatically in `configure`

### ConnectionId & ConnectionRef

* ConnectionId defines a connection, with required initialization
  * uid Field uniquely identifies the connection
* ConnectionRef is a user-facing reference to a connection

### Receiver

* Receiver is base type without template (for use in IOManager::m_receivers)
* ReceiverConcept introduces template and serves as class given by IOManager::get_receiver
* QueueReceiverModel and NetworkReceiverModel implement receives and callback loop for queues and network
  * NetworkReceiverModel::read_network determines if type is serializable using template metaprogramming

### Sender

* Similar design as for Receivers
* QueueSenderModel and NetworkSenderModel implement sends for queues and network
* NetworkReceiverModel::write_network determines if type is serializable using template metaprogramming

### API Diagram

![Class Diagrams](https://github.com/DUNE-DAQ/iomanager/raw/develop/docs/IOManager.png)

## Examples

### Send

```CPP
  // Int sender
  dunedaq::iomanager::ConnectionRef cref;
  cref.uid = "bar";
  cref.topics = {};

  int msg = 5;
  std::chrono::milliseconds timeout(100);
  auto isender = iom.get_sender<int>(cref);
  isender->send(msg, timeout);
  isender->send(msg, timeout);

  // One line send
  iom.get_sender<int>(cref)->send(msg, timeout);

```

### Receive (direct)

```CPP
// String receiver
  dunedaq::iomanager::ConnectionRef cref3;
  cref3.uid = "dsa";
  cref3.topics = {};

  auto receiver = iom.get_receiver<std::string>(cref3);
  std::string got;
  try {
    got = receiver->receive(timeout);
  } catch (dunedaq::appfwk::QueueTimeoutExpired&) {
    // This is expected
  }

```

### Receive (callback)

```CPP
  // Callback receiver
  dunedaq::iomanager::ConnectionRef cref4;
  cref4.uid = "zyx";
  cref4.topics = {};

  // CB function
  std::function<void(std::string)> str_receiver_cb = [&](std::string data) {
    std::cout << "Str receiver callback called with data: " << data << '\n';
  };

  auto cbrec = iom.get_receiver<std::string>(cref4);
  cbrec->add_callback(str_receiver_cb);
  try {
    got = cbrec->receive(timeout);
  } catch (dunedaq::iomanager::ReceiveCallbackConflict&) {
    // This is expected
  }
  iom.remove_callback(cref4);

```