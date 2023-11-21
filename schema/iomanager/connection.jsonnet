// The schema used by Connections

local moo = import "moo.jsonnet";

// A schema builder in the given path (namespace)
local s = moo.oschema.schema("dunedaq.iomanager.connection");

// Object structure used by ConnectionMgr
local c = {
    uid: s.string("Uid_t", doc="An identifier"),
    datatype: s.string("DataType_t", doc="Name of a data type"),

    qtype: s.enum("QueueType", ["kUnknown", "kStdDeQueue", "kFollySPSCQueue", "kFollyMPMCQueue", "kFollyHPSPSCQueue"], doc="The kinds (types/classes) of queues"),
    capacity: s.number("Capacity_t", dtype="u8", doc="Capacity of a queue"),

    uri: s.string("Uri_t", doc="Location of a resource"),
    connection_type: s.enum("ConnectionType", ["kSendRecv", "kPubSub"]),
    
    ConnectionId: s.record("ConnectionId",[
        s.field("uid", self.uid, doc="Identifier for the Connection instance"),
        s.field("data_type", self.datatype, doc="Name of the expected data type"),
        s.field("session", self.uid, default="", doc="Name of the DAQ session this Connection lives in")
    ]),

    Queue: s.record("QueueConfig", [
        s.field("id", self.ConnectionId, doc="Queue identifier"),
        s.field("queue_type", self.qtype, doc="Type of the Queue" ),
        s.field("capacity", self.capacity, doc="Capacity of the queue")
    ], doc="Internal representation for a Queue"),
    queues: s.sequence("Queues_t", self.Queue, doc="List of Queues (for configuring IOManager)"),

    Connection: s.record("Connection",[
        s.field("id", self.ConnectionId, doc="Connection identifier"),
        s.field("uri", self.uri, doc="ZMQ address for the connection"),
        s.field("connection_type", self.connection_type, doc="Type of the connection"),
    ]),
    connections: s.sequence("Connections_t", self.Connection, doc="List of Connections (for scale-down configuring IOManager)"),
};

moo.oschema.sort_select(c)

