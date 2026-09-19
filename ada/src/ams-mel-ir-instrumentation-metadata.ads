private with Ada.Finalization;
private with AMS.MEL_C_API;

--  Bounded owned polling of the conditionally required InstrumentationReport
--  metadata callback. The provider callback never invokes Ada: it validates
--  and copies a complete report into a bounded native DROP-INCOMING queue that
--  this package polls. Receive returns a wholly Ada-owned value; no native
--  event handle is ever exposed.
--
--  Registration lifetime: upstream has no unregister operation. The callback
--  state belongs to the Instrumentation channel, not to Metadata_Channel.
--  Close only deactivates public consumption; provider channel destruction
--  remains the callback-quiescence boundary.

package AMS.MEL.IR.Instrumentation.Metadata is
   type Metadata_Channel is limited private;
   function Open
     (Channel : Instrumentation_Channel; Queue_Capacity : Positive := 16) return Metadata_Channel;
   function Is_Open (Stream : Metadata_Channel) return Boolean;

   --  Timeout_Error is inherited from AMS.MEL.IR.
   function Receive
     (Stream : Metadata_Channel; Timeout_Milliseconds : Natural := 0) return Instrumentation_Report;

   type Metadata_Counters is record
      Events_Received, Events_Dropped_Queue_Full, Malformed_Or_Unsupported : Counter;
   end record;
   function Counters (Stream : Metadata_Channel) return Metadata_Counters;
   procedure Close (Stream : in out Metadata_Channel);

private
   type Metadata_Channel is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.Instrumentation_Metadata_Handle :=
        AMS.MEL_C_API.Null_Instrumentation_Metadata;
   end record;
   overriding
   procedure Finalize (Stream : in out Metadata_Channel);
end AMS.MEL.IR.Instrumentation.Metadata;
