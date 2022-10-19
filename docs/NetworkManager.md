# NetworkManager

## Introduction

NetworkManager is used by IOManager to configure and manage network connections (similar to how QueueRegistry configures and manages Queues).

### NetworkManager

Manages active connections within the application, as well as communicating the the ConfigClient to talk to the ConnectivityService

### ConfigClient

Wrapper around the HTTP API for the ConnectivityService to perform network connection registration and lookup

## API Description

![Class Diagrams](https://github.com/DUNE-DAQ/iomanager/raw/develop/docs/NetworkManager.png)