with AMS.MEL.IR.Scheduling.Commands;

package AMS_MEL_Track_Fixture is
   procedure Open (Provider_Path : String);
   procedure Release_Pending (Count : Positive);
   procedure Verify (Value : AMS.MEL.IR.Scheduling.Commands.Track_Task);
   procedure Drain;
   procedure Close;
end AMS_MEL_Track_Fixture;
