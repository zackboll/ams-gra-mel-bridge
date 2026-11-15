package AMS_MEL_Abort_Fixture is
   procedure Open (Provider_Path : String);
   procedure Release_Pending (Count : Positive);
   procedure Drain;
   procedure Close;
end AMS_MEL_Abort_Fixture;
