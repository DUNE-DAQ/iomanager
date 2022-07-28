// The schema used by Connections

local moo = import "moo.jsonnet";

// A schema builder in the given path (namespace)
local s = moo.oschema.schema("dunedaq.iomanager.connection");

// Object structure used by ConnectionMgr
local c = {
    uid: s.string("Uid_t", doc="An identifier"),
    uri: s.string("Uri_t", doc="Location of a resource"),
    uris: s.sequence("Uris_t", self.uri, doc="List of Uris"),
    connection_type: s.enum("ConnectionType", ["kQueue", "kSendRecv", "kPubSub"]),
    direction: s.enum("Direction", ["kUnspecified", "kInput", "kOutput"]),
    datatype: s.string("DataType_t", doc="Name of a data type"),
    label: s.string("Label_t", moo.re.ident_only, doc="A label hard-wired into code"),
    qtype: s.enum("QueueType", ["kUnknown", "kStdDeQueue", "kFollySPSCQueue", "kFollyMPMCQueue"], doc="The kinds (types/classes) of queues"),
    capacity: s.number("Capacity_t", dtype="u8", doc="Capacity of a queue"),

    subsystem: s.enum("Subsystem", ["kUNDEFINED", "kDRO", "kHSI", "kTRG", "kTRB" ], doc="Subsystem types. Should be synchronized with daqdataformats/SourceID"),
    id_num: s.number("ID_t", "u4", doc="An identifier (as part of a SourceID)"),
    sourceid: s.record("SourceID_t", [
        s.field("subsystem", self.subsystem, doc="Subsystem of SourceID"),
        s.field("id", self.id_num, doc="Identifier within Subsystem")
    ], doc="SourceID representation"),

    Endpoint: s.record("Endpoint", [
        s.field("data_type", self.datatype, doc="Name of the expected data type"),
        s.field("app_name", self.label, doc="Name of this app"),
        s.field("module_name", self.label, doc="Name of this module"),
        s.field("source_id", self.sourceid, doc="SourceID associated with Endpoint"),
        s.field("direction", self.direction, doc="Direction of data flow (input/output)"),
        s.field("nickname", self.label, "", doc="Name associated with an endpoint by a DAQModule")
    ], doc="A DAQ Module's endpoint for a connection"),

    endpoints: s.sequence("Endpoints_t", self.Endpoint, doc="List of Endpoints"),

    Connection: s.record("Connection", [
        s.field("bind_endpoint", self.Endpoint, doc="Endpoint associated with bind() system call"),
        s.field("connected_endpoints", self.endpoints, doc="Endpoints connected to this connection"),
        s.field("uri", self.uri, doc="Initialization information for the connection"),
        s.field("connection_type", self.connection_type, doc="Type of the connection")
    ], doc="The abstract representation of a connection"),
    connections: s.sequence("Connections_t", self.Connection, doc="List of Connections (for scale-down configuring IOManager)"),

    Queue: s.record("QueueConfig", [
        s.field("name", self.label, doc="Name for Queue instance"),
        s.field("endpoints", self.endpoints, doc="Endpoints for this Queue"),
        s.field("queue_type", self.qtype, doc="Type of the Queue" ),
        s.field("capacity", self.capacity, doc="Capacity of the queue")
    ], doc="Internal representation for a Queue"),
    queues: s.sequence("Queues_t", self.Queue, doc="List of Queues (for configuring IOManager)"),

    ConnectionRegistration: s.record("ConnectionRegistration", [
        s.field("bind", self.Endpoint, doc="Endpoint to bind connection to"),
        s.field("uri", self.uri, doc="Uri of connection"),
        s.field("connection_type", self.connection_type, doc="Type of connection")
    ], doc="Message sent to connection registry service to register a connection"),

    ConnectionRequest: s.record("ConnectionRequest", [
        s.field("search", self.Endpoint, doc="Endpoint to find matching connection for")
    ], doc="Message sent to connection registry service to query for a connection"),

    ConnectionResponse: s.record("ConnectionResponse", [
        s.field("uris", self.uris, doc="List of matching connection Uris (used for subscribing to multiple PubSub connections)"),
        s.field("connection_type", self.connection_type, doc="Type of the matching connection")
    ], doc="Expected message from connection registry service")

};

moo.oschema.sort_select(c)

