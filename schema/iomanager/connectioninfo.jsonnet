// This is the application info schema used by the network manager.
// It describes the information object structure passed by the application 
// for operational monitoring

local moo = import "moo.jsonnet";
local s = moo.oschema.schema("dunedaq.iomanager.connectioninfo");

local info = {

   count  : s.number("count", "u8", doc="An unsigned of 8 bytes"),

   info: s.record("Info", [
       s.field("sent_bytes", self.count, 0, doc="Bytes sent via a connection of the networkmanager"),
       s.field("received_bytes", self.count, 0, doc="Bytes received via a connection of the networkmanager"),
       s.field("sent_messages", self.count, 0, doc="Messages sent via a connection of the networkmanager"),
       s.field("received_messages", self.count, 0, doc="Messages received via a connection of the networkmanager")
   ], doc="IOManager information")
};

moo.oschema.sort_select(info) 
