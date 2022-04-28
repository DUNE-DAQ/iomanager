// The schema used by Connections

local moo = import "moo.jsonnet";

// A schema builder in the given path (namespace)
local s = moo.oschema.schema("dunedaq.iomanager.connection");

// Object structure used by ConnectionMgr
local c = {
  uid: s.string("Uid_t", doc="An identifier"),
  uri: s.string("Uri_t", doc="Location of a resource"),
  service_type: s.enum("ServiceType", ["kQueue", "kNetwork","kPubSub", "kUnknown"]),
  direction: s.enum("Direction", ["kUnspecified", "kInput", "kOutput"]),
  datatype: s.string("DataType_t", doc="Name of a data type"),
  topic: s.string("Topic_t", doc="Name of a topic"),
  topics: s.sequence("Topics_t", self.topic, doc="Topics used by an instance of a connection"),
  label: s.string("Label_t", moo.re.ident_only, doc="A label hard-wired into code"),

  ConnectionId: s.record("ConnectionId", [
        s.field("uid", self.uid, doc="Name of the connection"),
        s.field("service_type", self.service_type,"kUnknown", doc="Type of the connection"),
        s.field("data_type", self.datatype, doc="Name of the expected data type"),
        s.field("uri", self.uri, doc="Initialization information for the connection"),
        s.field("topics", self.topics,[], doc="Topics used by this connection (kPubSub) only"),
  ], doc="Information about a connection"),

  ref: s.record("ConnectionRef", [
        s.field("name", self.label, doc="The name by which this connection is known to the module"),
        s.field("uid", self.uid, doc="Name of the connection, or topic associated with a subscriber"),
        s.field("dir", self.direction,"kUnspecified", doc="Direction of this connection reference")
  ], doc="Reference to a connection"),

  connections: s.sequence("ConnectionIds_t", self.ConnectionId, doc="Connections used by an application/system"),
  connectionrefs: s.sequence("ConnectionRefs_t", self.ref, doc="Connections used by a DAQModule"),
  
};

moo.oschema.sort_select(c)

