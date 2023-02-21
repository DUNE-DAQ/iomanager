// This is the network info schema used by the NetworkManager.
// It describes the information object structure passed by the application 
// for operational monitoring

local moo = import "moo.jsonnet";
local s = moo.oschema.schema("dunedaq.iomanager.networkinfo");

local info = {
   string : s.string("string", doc="Generate proper includes in InfoStructs"),
   uint8  : s.number("uint8", "u8",
                     doc="An unsigned of 8 bytes used for counters"),

   info: s.record("Info", [
       s.field("config_client_time",   self.uint8, 0, doc="Microseconds spent in ConfigClient calls" )
   ], doc="NetworkManager information")
};

moo.oschema.sort_select(info) 
