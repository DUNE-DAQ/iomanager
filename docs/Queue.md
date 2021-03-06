# iomanager Queues

A Queue represents the internal communication channel between two DAQModules. Queues are typed based on the data transmitted using the queue.

There are currently three supported Queue types: FollySPSC, FollyMPMC, and StdDeQueue. They are named after their underlying data transport mechanism.

## API Description

### QueueBase

Base class for all queues. Non-templated for storage within the QueueRegistry

### Queue

Templated base class for queues. Provides abstract interface for Queue implementations.

### QueueConfig

Helper class which translates between configuration schema and Queue instances. QueueConfig::stoqk gives more flexibility for implementation of configuration interpretation than direct moo-generated struct.

### QueueRegistry

Singleton registry of all Queue instances in the current application. Configured and queried by IOManager.

### API Diagram

![Class Diagrams](https://github.com/DUNE-DAQ/iomanager/raw/develop/docs/Queue.png)