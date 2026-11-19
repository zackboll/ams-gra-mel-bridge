with AMS.MEL.IR.Scheduling.Commands;

package AMS_MEL_Scan_Fixture is
   procedure Open (Provider_Path : String);
   procedure Release_Pending (Count : Positive);
   procedure Verify (Value : AMS.MEL.IR.Scheduling.Commands.Scan_Task);
   procedure Drain;
   procedure Close;
end AMS_MEL_Scan_Fixture;
