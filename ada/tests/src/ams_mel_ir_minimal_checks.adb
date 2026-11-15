package body AMS_MEL_IR_Minimal_Checks is
   procedure Require (Condition : Boolean; Operation : String) is
   begin
      if not Condition then
         raise Program_Error with Operation;
      end if;
   end Require;

   procedure Identity (Vendor, Description : String; API, Revision : Long_Long_Integer) is
   begin
      Require
        (Vendor = "minimal-mel-example" and API = 4 and Revision = 1 and Description'Length > 0,
         "provider identity: expected minimal-mel-example API 4 revision 1 with description");
   end Identity;

   procedure Selection (Occurrences : Counts; Descriptor_Count : Natural; Distinct : Boolean) is
   begin
      for I in Occurrences'Range loop
         Require (Occurrences (I) = 1, "missing or ambiguous required capability" & I'Image);
      end loop;
      Require (Descriptor_Count = 4 and Distinct, "duplicate or unexpected discovery identity");
   end Selection;

   procedure Correlation
     (Actual, Expected : AMS.MEL.IR.Channel.Comms_Test_Report; Operation : String)
   is
      use type AMS.MEL.IR.Channel.Comms_Test_Report;
   begin
      Require (Actual = Expected, Operation & ": command/request correlation");
   end Correlation;

   procedure Complete (Observed : Evidence) is
   begin
      Require (Observed.Registrations = 17, "expected 17 metadata registrations");
      Require (Observed.Keep_Alives = 4, "expected four KeepAlive responses");
      Require (Observed.Futures = 8, "expected eight independent CommsTest futures");
      Require (Observed.Callbacks = [2, 2, 2, 2], "expected two independent callbacks per queue");
      Require (Observed.BIT_Expected, "unexpected BIT outcome");
      Require (Observed.Config_Expected, "unexpected ConfigSet outcome");
      Require (Observed.Mode_Expected, "unexpected Mode outcome");
      Require (Observed.Navigation_Expected, "unexpected NavigationReport outcome");
      Require (Observed.Quiet, "unexpected sensor telemetry or image delivery");
      Require (Observed.Cleanup_Complete, "incomplete explicit cleanup");
   end Complete;
end AMS_MEL_IR_Minimal_Checks;
