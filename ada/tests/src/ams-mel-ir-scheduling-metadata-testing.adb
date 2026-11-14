with AMS.MEL_C_API;

package body AMS.MEL.IR.Scheduling.Metadata.Testing is
   procedure Check_Representation is
      package C renames AMS.MEL_C_API;
      Value : constant C.IR_Execute_Task_Ack_V1 := (Command_ID => 0, Request_ID => 0);
   begin
      --  Same fixed size, alignment and offsets as the compiled C11 layout probe.
      if C.IR_Execute_Task_Ack_V1'Object_Size /= 64
        or else C.IR_Execute_Task_Ack_V1'Alignment /= 4
        or else Value.Command_ID'Position /= 0
        or else Value.Request_ID'Position /= 4
      then
         raise Program_Error with "ExecuteTaskAck C/Ada layout mismatch";
      end if;
   end Check_Representation;

   procedure Check_Task_Executing_Representation is
      package C renames AMS.MEL_C_API;
      Value : constant C.IR_Task_Executing_Rep_V1 :=
        (Estimated_Duration_NS => 0, Track_ID => 0, others => 0);
   begin
      --  Same 48-byte size, 8-byte alignment and offsets as the compiled C11 layout probe, and
      --  the same numeric values for every Task / TaskCompStatus constant.
      if C.IR_Task_Executing_Rep_V1'Object_Size /= 384
        or else C.IR_Task_Executing_Rep_V1'Alignment /= 8
        or else Value.Command_ID'Position /= 0
        or else Value.Activity_ID'Position /= 4
        or else Value.Request_ID'Position /= 8
        or else Value.Task_Value'Position /= 12
        or else Value.Estimated_Duration_NS'Position /= 16
        or else Value.Track_ID'Position /= 24
        or else Value.Previous_Activity_ID'Position /= 28
        or else Value.Previous_Request_ID'Position /= 32
        or else Value.Previous_Task'Position /= 36
        or else Value.Previous_Task_Status'Position /= 40
      then
         raise Program_Error with "TaskExecutingRep C/Ada layout mismatch";
      end if;
      if Task_Kind'Enum_Rep (Task_Kind'First) /= 0
        or else Task_Kind'Enum_Rep (IRST_Update) /= 10
        or else Task_Kind'Enum_Rep (Reserved_1) /= 11
        or else Task_Kind'Enum_Rep (Task_Kind'Last) /= 16
        or else Task_Kind'Pos (Task_Kind'Last) /= 16
        or else Operational_Task_Kind'Last /= IRST_Update
        or else Task_Completion_Status'Enum_Rep (Task_Completion_Status'Last) /= 4
      then
         raise Program_Error with "Task/TaskCompStatus representation mismatch";
      end if;
   end Check_Task_Executing_Representation;

   function Convert (Raw : Raw_Task_Executing) return Task_Executing_Report is
      Native : constant AMS.MEL_C_API.IR_Task_Executing_Rep_V1 :=
        (Command_ID            => Raw.Command_ID,
         Activity_ID           => Raw.Activity_ID,
         Request_ID            => Raw.Request_ID,
         Task_Value            => Raw.Task_Value,
         Estimated_Duration_NS => Raw.Estimated_Duration_NS,
         Track_ID              => Raw.Track_ID,
         Previous_Activity_ID  => Raw.Previous_Activity_ID,
         Previous_Request_ID   => Raw.Previous_Request_ID,
         Previous_Task         => Raw.Previous_Task,
         Previous_Task_Status  => Raw.Previous_Task_Status);
   begin
      return To_Report (Native);
   end Convert;
end AMS.MEL.IR.Scheduling.Metadata.Testing;
