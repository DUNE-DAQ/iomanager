# NetworkManager

## Introduction

NetworkManager is used by IOManager to configure and manage network connections (similar to how QueueRegistry configures and manages Queues).

### NetworkManager

Manages active connections within the application, as well as communicating the the ConfigClient to talk to the ConnectivityService

### ConfigClient

Wrapper around the HTTP API for the ConnectivityService to perform network connection registration and lookup

### NetworkReceiverModel

Represents the receive end of a network connection, implementation of ReceiverConcept and exposed to DAQModules via `IOManager::get_receiver<T>`

### NetworkSenderModel

Represents the send end of a network connection, implementation of SenderConcept and exposed to DAQModules via `IOManager::get_sender<T>`

## API Description

![Class Diagrams](https://github.com/DUNE-DAQ/iomanager/raw/develop/docs/iomanager-network.png)