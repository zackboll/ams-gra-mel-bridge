with AMS.MEL.IR.Channel;

--  Application qualification rules, not a production binding interface.

package AMS_MEL_IR_Minimal_Checks is
   type Counts is array (Positive range 1 .. 4) of Natural;
   type Evidence is record
      Registrations                                                     : Natural := 0;
      Keep_Alives                                                       : Natural := 0;
      Futures                                                           : Natural := 0;
      Callbacks                                                         : Counts := [others => 0];
      BIT_Expected, Config_Expected, Mode_Expected, Navigation_Expected : Boolean := False;
      Quiet, Cleanup_Complete                                           : Boolean := False;
   end record;

   procedure Require (Condition : Boolean; Operation : String);
   procedure Identity (Vendor, Description : String; API, Revision : Long_Long_Integer);
   procedure Selection (Occurrences : Counts; Descriptor_Count : Natural; Distinct : Boolean);
   procedure Correlation
     (Actual, Expected : AMS.MEL.IR.Channel.Comms_Test_Report; Operation : String);
   procedure Complete (Observed : Evidence);
end AMS_MEL_IR_Minimal_Checks;
