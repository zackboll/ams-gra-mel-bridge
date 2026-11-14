with Ada.Containers.Vectors;
with AMS.MEL.IR.Channel;

package AMS.MEL.IR.Discovery is
   --  Enumerate advertised identities before choosing channels to attach.
   --  Discovery neither attaches channels nor establishes that their advertised
   --  sensor operations are functional. Serialize calls against Session closure
   --  and other provider operations; no concurrent-close protocol is provided.

   package Capability_Vectors is new
     Ada.Containers.Vectors
       (Index_Type   => Positive,
        Element_Type => AMS.MEL.IR.Channel.Channel_Capability,
        "="          => AMS.MEL.IR.Channel."=");

   function Capabilities (Session : AMS.MEL.Session) return Capability_Vectors.Vector;
   --  Copy the complete Control capability list in provider enumeration order.
   --  An empty vector is valid. Zero image dimensions, bit depth, band count
   --  and sizes remain zero: an unconfigured sensor need not advertise a stream
   --  layout yet. All nested values are Ada-owned and remain usable after the
   --  Session closes and the provider library unloads.
   --  @param Session Initialized, open Session; serialize against Close.
   --  @return Independent Ada-owned capability values, not native owners.
   --  @exception Provider_Error Closed Session, malformed capability data,
   --  provider exception or native allocation failure.
   --  Ada allocation or representation failures propagate after native cleanup.
end AMS.MEL.IR.Discovery;
