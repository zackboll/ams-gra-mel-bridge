with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.IR.C2;
with AMS.MEL.IR.C2.Common;
with AMS.MEL.IR.C2.Metadata;
with AMS.MEL.IR.Channel;
with AMS.MEL.IR.Channel.Metadata;
with AMS.MEL.IR.Health_Status;
with AMS.MEL.IR.Image;
with AMS.MEL.IR.Stacked_Image;
with Interfaces;

package body AMS_MEL_IR_Comms_Metadata_Tests is
   package IR renames AMS.MEL.IR;
   package V renames IR.Channel;
   package M renames V.Metadata;
   package CM renames IR.C2.Metadata;
   package CC renames IR.C2.Common;
   use type V.Comms_Test_Report;
   use type V.Outcome;
   use type V.Command_ID;
   use type V.Comms_Request_ID;
   use type CM.Metadata_Kind;
   use type IR.C2.Outcome;
   use type IR.Counter;
   use type Interfaces.Unsigned_32;

   ID       : constant IR.UCI_ID := IR.Create_UCI_ID ([others => 0], "comms");
   Location : constant IR.Component_Location :=
     IR.Create_Component_Location (0.0, 0.0, 0.0, "", "");

   procedure Require (Condition : Boolean; Text : String) is
   begin
      if not Condition then
         raise Program_Error with "common metadata: " & Text;
      end if;
   end Require;

   --  Pure safe Ada parity: two actual callbacks per required destination,
   --  eight independent future results. No image frame or sensor telemetry.
   procedure Parity (Provider_Path : String) is
      Session    : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "common-meta-parity");
      C2         : IR.C2.Control_Channel :=
        IR.C2.Open (Session, IR.C2.Create_Config (ID, ID, Location));
      Health     : IR.Health_Status.Health_Channel :=
        IR.Health_Status.Open (Session, IR.Health_Status.Create_Config (ID, ID, Location));
      Image      : IR.Image_Stream :=
        IR.Open_Image_Control (Session, IR.Create_Image_Control_Config (ID, ID, Location));
      Stacked    : IR.Stacked_Image.Stacked_Channel :=
        IR.Stacked_Image.Open (Session, IR.Stacked_Image.Create_Config (ID, ID, Location));
      CV         : V.View := IR.C2.As_Channel (C2);
      HV         : V.View := IR.Health_Status.As_Channel (Health);
      IV         : V.View := IR.Image.As_Channel (Image);
      SV         : V.View := IR.Stacked_Image.As_Channel (Stacked);
      C_Metadata : CM.Metadata_Stream := CM.Open (C2);
      H_Metadata : M.Comms_Metadata_Stream := M.Open (HV);
      I_Metadata : M.Comms_Metadata_Stream := M.Open (IV);
      S_Metadata : M.Comms_Metadata_Stream := M.Open (SV);
      Total      : Natural := 0;

      procedure Check_Future (Request : V.Comms_Request; Expected : V.Comms_Test_Report) is
         Result : constant V.Comms_Result := V.Wait (Request, 1_000);
      begin
         Require (V.Status (Result) = V.Success, "future status");
         Require (V.Report (Result) = Expected, "future IDs independently checked");
      end Check_Future;

      procedure Destination
        (View : V.View; Selector : V.Comms_Channel_ID; Stream : M.Comms_Metadata_Stream) is
      begin
         for Routed in Boolean loop
            declare
               Command    : constant V.Command_ID := 16#8000_0000# + V.Command_ID (Total);
               Request_ID : constant V.Comms_Request_ID :=
                 16#F000_0000# + V.Comms_Request_ID (Total);
               Expected   : constant V.Comms_Test_Report := (Command, Request_ID);
            begin
               if Routed then
                  declare
                     Request : CC.Comms_Request :=
                       CC.Submit_Comms_Test (C2, Selector, Command, Request_ID);
                     Result  : constant CC.Comms_Result := CC.Wait (Request, 1_000);
                  begin
                     Require
                       (M.Receive (Stream, 1_000) = Expected,
                        "routed callback destination and IDs");
                     Require
                       (CC.Status (Result) = IR.C2.Success and then CC.Report (Result) = Expected,
                        "routed future independently checked");
                     CC.Close (Request);
                  end;
               else
                  declare
                     Request : V.Comms_Request :=
                       V.Submit_Comms_Test (View, Selector, Command, Request_ID);
                  begin
                     Require
                       (M.Receive (Stream, 1_000) = Expected,
                        "direct callback destination and IDs");
                     Check_Future (Request, Expected);
                     V.Close (Request);
                  end;
               end if;
               Total := Total + 1;
            end;
         end loop;
         Require (M.Counters (Stream).Events_Received = 2, "exactly two callbacks");
         begin
            declare
               Extra : constant V.Comms_Test_Report := M.Receive (Stream);
            begin
               raise Program_Error with "extra callback" & Extra.Command_ID'Image;
            end;
         exception
            when IR.Timeout_Error =>
               null;
         end;
      end Destination;
   begin
      CM.Enable_Comms_Test_Events (C_Metadata);
      --  Attached and Enabled registrations are covered separately below.
      IR.C2.Enable (C2);
      IR.Health_Status.Enable (Health);
      IR.Start (Image);
      IR.Stacked_Image.Enable (Stacked);
      for Routed in Boolean loop
         declare
            Command    : constant V.Command_ID := 16#8000_0000# + V.Command_ID (Total);
            Request_ID : constant V.Comms_Request_ID := 16#F000_0000# + V.Comms_Request_ID (Total);
            Expected   : constant V.Comms_Test_Report := (Command, Request_ID);
         begin
            if Routed then
               declare
                  Request : CC.Comms_Request := CC.Submit_Comms_Test (C2, 2, Command, Request_ID);
                  Event   : constant CM.Metadata_Event := CM.Receive (C_Metadata, 1_000);
                  Result  : constant CC.Comms_Result := CC.Wait (Request, 1_000);
               begin
                  Require (CM.Kind (Event) = CM.Channel_Comms_Test_Event, "C2 routed event kind");
                  Require (CM.Comms_Test (Event) = Expected, "C2 routed callback IDs");
                  Require
                    (CC.Status (Result) = IR.C2.Success and then CC.Report (Result) = Expected,
                     "C2 routed future IDs");
                  CC.Close (Request);
               end;
            else
               declare
                  Request : V.Comms_Request := V.Submit_Comms_Test (CV, 2, Command, Request_ID);
                  Event   : constant CM.Metadata_Event := CM.Receive (C_Metadata, 1_000);
               begin
                  Require (CM.Kind (Event) = CM.Channel_Comms_Test_Event, "C2 direct event kind");
                  Require (CM.Comms_Test (Event) = Expected, "C2 direct callback IDs");
                  Check_Future (Request, Expected);
                  V.Close (Request);
               end;
            end if;
            Total := Total + 1;
         end;
      end loop;
      Destination (HV, 4, H_Metadata);
      Destination (IV, 1, I_Metadata);
      Destination (SV, 6, S_Metadata);
      Require (Total = 8, "exactly eight correlated callbacks");
      Require (CM.Counters (C_Metadata).Events_Received = 2, "exactly two C2 callbacks");
      --  Weak views close first; registrations remain attached to typed owners.
      V.Close (CV);
      V.Close (HV);
      V.Close (IV);
      V.Close (SV);
      CM.Close (C_Metadata);
      M.Close (H_Metadata);
      M.Close (I_Metadata);
      M.Close (S_Metadata);
      IR.C2.Close (C2);
      IR.Health_Status.Close (Health);
      IR.Close (Image);
      IR.Stacked_Image.Close (Stacked);
      AMS.MEL.Close (Session);
   end Parity;

   procedure Cases (Provider_Path, Scenario : String) is
      Session : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "common-meta-" & Scenario);
      Health  : IR.Health_Status.Health_Channel :=
        IR.Health_Status.Open (Session, IR.Health_Status.Create_Config (ID, ID, Location));
      View    : V.View := IR.Health_Status.As_Channel (Health);
      Second  : V.View := IR.Health_Status.As_Channel (Health);
      Failed  : constant Boolean :=
        Scenario = "refuse" or Scenario = "fail" or Scenario = "partial" or Scenario = "throw";
   begin
      IR.Health_Status.Enable (Health);
      begin
         declare
            Stream : M.Comms_Metadata_Stream := M.Open (View, 2);
         begin
            Require (not Failed and M.Is_Open (Stream), "registration outcome");
            if Scenario = "sync" then
               IR.Health_Status.Close (Health);
               Require (M.Receive (Stream) = (16#ABCD_EF01#, 16#FEDC_BA98#), "sync after close");
               begin
                  declare
                     Extra : constant V.Comms_Test_Report := M.Receive (Stream);
                  begin
                     raise Program_Error with Extra.Command_ID'Image;
                  end;
               exception
                  when IR.Stream_Stopped =>
                     null;
               end;
            elsif Scenario = "null" then
               Require (M.Counters (Stream).Malformed_Or_Unsupported = 1, "null report");
            elsif Scenario = "overflow" then
               Require (M.Counters (Stream).Events_Dropped_Queue_Full = 4, "drop incoming");
               Require (M.Receive (Stream) = (16#8000_0000#, 16#F000_0000#), "FIFO first");
               Require (M.Receive (Stream) = (16#8000_0001#, 16#F000_0001#), "FIFO second");
            else
               if Scenario = "future-only" then
                  declare
                     Request : V.Comms_Request :=
                       V.Submit_Comms_Test (View, 4, 16#8000_0001#, 16#FFFF_FFFF#);
                     Result  : constant V.Comms_Result := V.Wait (Request, 1_000);
                  begin
                     Require
                       (V.Status (Result) = V.Success
                        and then V.Report (Result) = (16#8000_0001#, 16#FFFF_FFFF#),
                        "future completed without a callback");
                     Require
                       (M.Counters (Stream).Events_Received = 0,
                        "successful future must not manufacture metadata");
                     V.Close (Request);
                  end;
               end if;
               for Timeout in 0 .. 1 loop
                  begin
                     declare
                        Extra : constant V.Comms_Test_Report := M.Receive (Stream, Timeout);
                     begin
                        raise Program_Error with Extra.Command_ID'Image;
                     end;
                  exception
                     when IR.Timeout_Error =>
                        null;
                  end;
               end loop;
            end if;
            M.Close (Stream);
            M.Close (Stream);
         end;
      exception
         when AMS.MEL.Provider_Error =>
            Require (Failed, "unexpected registration failure");
      end;
      begin
         declare
            Retry : M.Comms_Metadata_Stream := M.Open (Second);
         begin
            M.Close (Retry);
            raise Program_Error with "registration retried through second View";
         end;
      exception
         when AMS.MEL.Provider_Error =>
            null;
      end;
      V.Close (View);
      V.Close (Second);
      IR.Health_Status.Close (Health);
      AMS.MEL.Close (Session);
   end Cases;

   procedure Zero_Layout_Image (Provider_Path : String) is
      Session    : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "common-meta-zero");
      Image      : IR.Image_Stream :=
        IR.Open_Image_Control (Session, IR.Create_Image_Control_Config (ID, ID, Location));
      View       : V.View := IR.Image.As_Channel (Image);
      Capability : constant V.Channel_Capability := V.Capabilities (View);
   begin
      Require
        (V.Width (Capability) = 0
         and then V.Height (Capability) = 0
         and then V.Bit_Depth (Capability) = 0,
         "control-only zero layout");
      IR.Start (Image);
      declare
         Stream  : M.Comms_Metadata_Stream := M.Open (View);
         Request : V.Comms_Request := V.Submit_Comms_Test (View, 1, 16#FFFF_FFFF#, 16#8000_0000#);
         Result  : constant V.Comms_Result := V.Wait (Request, 1_000);
      begin
         Require
           (M.Receive (Stream, 1_000) = (16#FFFF_FFFF#, 16#8000_0000#),
            "enabled zero-layout callback");
         Require
           (V.Status (Result) = V.Success
            and then V.Report (Result) = (16#FFFF_FFFF#, 16#8000_0000#),
            "zero-layout future");
         V.Close (Request);
         M.Close (Stream);
      end;
      V.Close (View);
      IR.Close (Image);
      AMS.MEL.Close (Session);
   end Zero_Layout_Image;

   procedure Run (Provider_Path : String) is
   begin
      Parity (Provider_Path);
      Zero_Layout_Image (Provider_Path);
      Cases (Provider_Path, "sync");
      Cases (Provider_Path, "refuse");
      Cases (Provider_Path, "fail");
      Cases (Provider_Path, "partial");
      Cases (Provider_Path, "throw");
      Cases (Provider_Path, "null");
      Cases (Provider_Path, "overflow");
      Cases (Provider_Path, "parity");
      Cases (Provider_Path, "future-only");
   end Run;
end AMS_MEL_IR_Comms_Metadata_Tests;
