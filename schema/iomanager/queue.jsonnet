// This is the "command" schema ("cmd") used by appfwk applications.
//
// It governs the expected top-level structure of command objects.
//
// Its objects hold substructure as an "any" type which is expected to
// be interpreted by the next lower layer such as described in the
// appfwk "application" schema ("app").

local moo = import "moo.jsonnet";
local s = moo.oschema.schema("dunedaq.iomanager.queue");

// A temporary schema construction context.
local cs = {

    inst: s.string("InstName", moo.re.ident_only,
                   doc="Name of a target instance of a kind"),
    label: s.string("Label", moo.re.ident_only,
                   doc="A label hard-wired into code"),
    qkind: s.enum("QueueKind",
                  ["Unknown", "StdDeQueue", "FollySPSCQueue", "FollyMPMCQueue"],
                  doc="The kinds (types/classes) of queues"),
    capacity: s.number("QueueCapacity", dtype="u8",
                       doc="Capacity of a queue"),
                           
    qspec: s.record("QueueSpec", [
        s.field("kind", self.qkind,
                doc="The kind (type) of queue"),
        s.field("inst", self.inst,
                doc="Instance name"),
        s.field("capacity", self.capacity,
                doc="The queue capacity"),
    ], doc="Queue specification"),

    qspecs: s.sequence("QueueSpecs", self.qspec,
                       doc="A sequence of QueueSpec"),

    

    // fixme: probably should be an enum: ["unknown","input","output"]
    dir: s.string("QueueDir", pattern="(^input$)|(^output$)",
                  doc="Direction of queue from the point of view of a module holding an endpoint"),

    qinfo: s.record("QueueInfo", [
        s.field("inst", self.inst,
                doc="The queue instance name"),
        s.field("name", self.label,
                doc="The name by which this queue is known to the module"),
        s.field("dir", self.dir,
                doc="The direction of the queue as seen by the module"),
    ], doc="Information for a module to find a Queue"),
    qinfos: s.sequence("QueueInfos", self.qinfo,
                       doc="A sequence of QueueInfo"),

};

// Output a topologically sorted array.
moo.oschema.sort_select(cs)
