# Updating Existing Code to Use _iomanager_

## Add iomanager to CMakeLists.txt if necessary

Specifically, if you're using `DEP_PKGS appfwk` in your `daq_codegen` call, you will have to add `iomanager` to the `DEP_PKGS` list.

## Remove _nwqueueadapters_ and register Serializable data types

Since all communication is now handled by IOManager's Sender and Receiver classes in a transport-agnostic way, the NQ classes are no longer needed.

Instead, all data structs which may be transmitted over the network should have the DUNE_DAQ_SERIALIZABLE macro called:

```CPP
namespace mypackage {
struct Data
{
  int d1;
  double d2;
  std::string d3;

  Data(int i, double d, std::string s)
    : d1(i), d2(d), d3(s)
  {}

  DUNE_DAQ_SERIALIZE(Data, d1, d2, d3);
};
} // namespace mypackage

// Must be in dunedaq namespace only
DUNE_DAQ_SERIALIZABLE(mypackage::Data);

} // namespace dunedaq
```

* Note: Data types which are serialized using `DUNE_DAQ_SERIALIZE_NON_INTRUSIVE` do not need to be updated, `DUNE_DAQ_SERIALIZABLE` is called as part of that macro.

## Use IOManager::configure() instead of NetworkManager/QueueRegistry configuration in tests

Any tests that directly configure QueueRegistry and/or NetworkManager should instead configure IOManager. This translation is fairly straightforward, see [IOManager's unit test](https://github.com/DUNE-DAQ/iomanager/blob/f3a9eefe75811984b4b0864511e1ce61537ff342/unittest/IOManager_test.cxx#L117) for examples.

## Update Schema Usage

For modules which loop over `ModInit::qinfos`, they should now loop over `ModInit::conn_refs` (or update to using DAQModuleHelper, below)

## Update DAQModuleHelper Usage

Instead of `appfwk::queue_index` or `appfwk::queue_inst`, use `appfwk::connection_index` or `appfwk::connection_inst`. These methods return the iomanager::ConnectionRef objects needed to get the Sender and Receiver objects from the IOManager.

## Replace DAQSink with Sender, DAQSource with Receiver

Unfortunately, the signature changes are not easily replaceable with `sed`. 

### DAQSink

* `#include "appfwk/DAQSink.hpp` becomes `#include "iomanager/Sender.hpp"`
* `queue_.reset(new DAQSink<T>("name"));` must become `queue_ = iom.get_sender<T>(ref);`
* Instead of `std::unique_ptr<DAQSink<T>>`, use `std::shared_ptr<iomanager::SenderConcept<T>>`
* `queue_->push(std::move(obj), timeout);` becomes `queue_->send(obj, timeout);`

### DAQSource

* `#include "appfwk/DAQSource.hpp` becomes `#include "iomanager/Receiver.hpp"`
* `queue_.reset(new DAQSource<T>("name"));` must become `queue_ = iom.get_receiver<T>(ref);`
* Instead of `std::unique_ptr<DAQSource<T>>`, use `std::shared_ptr<iomanager::ReceiverConcept<T>>`
* `queue_->pop(result, timeout);` becomes `result = queue_->receive(timeout);`

## Change Queue-centric send/receive loops

`can_push()` and `can_pop()` are not supported by Sender and Receiver. Their usage should be replaced with `try..catch` blocks:
```CPP
if(queue_->can_push()) {
  queue_->push(obj);
} else {
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
}
```
becomes
```CPP
try {
  queue_->send(obj, iomanager::Sender::s_no_block);
} catch(iomanager::TimeoutExpired&) {
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
}
```

## Update NetworkManager Usage

* Replace `NetworkManager::get().send_to(conn_name, data)` with `iom.get_receiver<T>(conn_name)->send(data)`
* Similar changes for `receive_from`
* Callbacks should change signature from `ipm::Receiver::Response message` to whatever type is desired on that connection. Updating the method itself should be straightforward; simply remove the code that takes the message and deserializes it (since that is now handled internally in iomanager::Receiver classes).
  * Callback registration can be done via `iom.add_callback<T>(conn_ref, &method)` or `iom.get_receiver<T>(conn_ref)->add_callback(&method)`
