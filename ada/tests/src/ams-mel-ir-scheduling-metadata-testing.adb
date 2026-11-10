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
end AMS.MEL.IR.Scheduling.Metadata.Testing;
