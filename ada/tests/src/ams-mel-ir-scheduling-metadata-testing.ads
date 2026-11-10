with Interfaces;

--  Test-only compiled C/Ada ExecuteTaskAck and TaskExecutingRep layout checks, plus the
--  private checked native-to-Ada TaskExecutingRep conversion seam.

package AMS.MEL.IR.Scheduling.Metadata.Testing is
   procedure Check_Representation;
   procedure Check_Task_Executing_Representation;

   type Raw_Task_Executing is record
      Command_ID, Activity_ID, Request_ID, Task_Value : Interfaces.Unsigned_32 := 0;
      Estimated_Duration_NS                           : Interfaces.Integer_64 := 0;
      Track_ID                                        : Interfaces.Integer_32 := 0;
      Previous_Activity_ID, Previous_Request_ID       : Interfaces.Unsigned_32 := 0;
      Previous_Task, Previous_Task_Status             : Interfaces.Unsigned_32 := 0;
   end record
   with Convention => C;
   --  Test description of a raw native record (the same C layout the mock delivery hook
   --  accepts), mapped field by field onto the private C type.

   function Convert (Raw : Raw_Task_Executing) return Task_Executing_Report;
   --  Run the package's private checked conversion on a raw native record.
end AMS.MEL.IR.Scheduling.Metadata.Testing;
