with Ada.Exceptions;
with Ada.Text_IO;
with Ada.Unchecked_Conversion;
with Interfaces.C;
with System;
with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.IR.Channel;
with AMS.MEL.IR.Discovery;
with AMS.MEL.IR.Stacked_Image;
with Interfaces;

--  IRMEL-ADA-016: safe Ada StackedImage ownership/common operations. The
--  separately compiled mock checks exact configuration propagation.

package body AMS_MEL_IR_Stacked_Tests is
   package Sch renames AMS.MEL.IR.Stacked_Image;
   package V renames AMS.MEL.IR.Channel;
   use type V.Channel_Type;
   use type V.Metadata_Capability;
   use type V.Outcome;
   use type V.Command_Return;
   use type V.Command_ID;
   use type V.Comms_Request_ID;
   use type Interfaces.Unsigned_8;
   use type Interfaces.Unsigned_32;
   use type V.Comms_Test_Report;

   High_Channel : constant V.Comms_Channel_ID := 16#F000_0002#;
   High_Command : constant V.Command_ID := 16#8000_0001#;
   High_Request : constant V.Comms_Request_ID := 16#E000_0003#;

   function Make_UUID (Base : Interfaces.Unsigned_8) return AMS.MEL.IR.UUID is
      Result : AMS.MEL.IR.UUID;
   begin
      for I in Result'Range loop
         Result (I) := Base + Interfaces.Unsigned_8 (I - Result'First);
      end loop;
      return Result;
   end Make_UUID;

   --  Exactly the identities and location the "stacked-config" mock verifies.
   Channel_ID : constant AMS.MEL.IR.UCI_ID :=
     AMS.MEL.IR.Create_UCI_ID
       (Make_UUID (16#A0#),
        --  Exact UTF-8 bytes of U+03BC, as the C11 test and the mock expect.
        "IR stacked " & Character'Val (16#CE#) & Character'Val (16#BC#) & " channel");
   Platform   : constant AMS.MEL.IR.UCI_ID :=
     AMS.MEL.IR.Create_UCI_ID (Make_UUID (16#F0#), "test platform");
   Location   : constant AMS.MEL.IR.Component_Location :=
     AMS.MEL.IR.Create_Component_Location (1.25, -2.5, 3.75, "station-1", "mock-aircraft");
   Config     : constant Sch.Stacked_Config := Sch.Create_Config (Channel_ID, Platform, Location);

   procedure Require (Condition : Boolean; Message : String) is
   begin
      if not Condition then
         raise Program_Error with "Ada Stacked: " & Message;
      end if;
   end Require;

   procedure Check_Capability (Value : V.Channel_Capability; Depth : Interfaces.Unsigned_32) is
   begin
      Require (V.Channel_Type_Count (Value) = 2, "channel type count");
      Require (V.Channel_Type_At (Value, 1) = V.Stacked_Image, "Stacked channel type");
      Require (V.Channel_Type_At (Value, 1)'Enum_Rep = 6, "Stacked numeric value");
      Require (V.Channel_Type_At (Value, 2) = V.Reserved_1, "second channel type");
      Require (V.Task_Schedule_Depth (Value) = Depth, "TaskScheduleDepth");
      Require (V.Metadata_Capability_Count (Value) = 1, "metadata count");
      Require (V.Metadata_Capability_At (Value, 1) = V.Channel_Comms_Test_Rep, "metadata type");
      Require
        (V.Width (Value) = 1920 and then V.Height (Value) = 1080 and then V.Bit_Depth (Value) = 12,
         "shared capability scalars");
      Require
        (AMS.MEL.IR.Descriptive_Label (V.Channel_ID (Value)) = "channel-α",
         "capability channel label");
      Require
        (V.Image_Band_Count (Value) = 2 and then V.Nav_Frame_Count (Value) = 2,
         "capability collections");
   end Check_Capability;

   --  Complete Config propagation (verified by the mock), Attached capability,
   --  explicit and idempotent Enable, Enabled capability, idempotent Close,
   --  and a snapshot that outlives owner, Session and provider unload.
   procedure Test_Open_Enable_Close (Provider_Path : String) is
      Parent   : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "stacked-config");
      Channel  : Sch.Stacked_Channel := Sch.Open (Parent, Config);
      Attached : constant V.Channel_Capability := Sch.Capabilities (Channel);
   begin
      Require (Sch.Is_Open (Channel), "Open");
      Check_Capability (Attached, 23);
      Sch.Enable (Channel);
      Sch.Enable (Channel);
      declare
         Enabled : constant V.Channel_Capability := Sch.Capabilities (Channel);
      begin
         Check_Capability (Enabled, 23);
         Sch.Close (Channel);
         Require (not Sch.Is_Open (Channel), "Close did not clear the owner");
         Sch.Close (Channel);
         AMS.MEL.Close (Parent);
         Check_Capability (Enabled, 23);
      end;
      Check_Capability (Attached, 23);
   end Test_Open_Enable_Close;

   --  Unrelated capability scalars are copied verbatim, not treated as layout.
   procedure Test_Depth (Provider_Path : String; Scenario : String; Depth : Interfaces.Unsigned_32)
   is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, Scenario);
      Channel : Sch.Stacked_Channel := Sch.Open (Parent, Config);
   begin
      Check_Capability (Sch.Capabilities (Channel), Depth);
      Sch.Enable (Channel);
      Check_Capability (Sch.Capabilities (Channel), Depth);
      Sch.Close (Channel);
      AMS.MEL.Close (Parent);
   end Test_Depth;

   procedure Test_Parent_First (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "stacked-basic");
      Channel : Sch.Stacked_Channel := Sch.Open (Parent, Config);
   begin
      Sch.Enable (Channel);
      AMS.MEL.Close (Parent);
      Require (not AMS.MEL.Is_Open (Parent), "parent remained open");
      Check_Capability (Sch.Capabilities (Channel), 23);
      Sch.Close (Channel);
   end Test_Parent_First;

   --  Scope-only finalization closes an un-closed owner without raising.
   procedure Test_Finalization (Provider_Path : String) is
      Parent : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "stacked-basic");
   begin
      declare
         Channel : Sch.Stacked_Channel := Sch.Open (Parent, Config);
      begin
         Sch.Enable (Channel);
      end;
      AMS.MEL.Close (Parent);
   end Test_Finalization;

   procedure Test_Enable_Failure (Provider_Path : String; Scenario : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, Scenario);
      Channel : Sch.Stacked_Channel := Sch.Open (Parent, Config);
   begin
      for Attempt in 1 .. 2 loop
         begin
            Sch.Enable (Channel);
            raise Program_Error with "Ada Stacked enable failure accepted" & Attempt'Image;
         exception
            when AMS.MEL.Provider_Error =>
               null;
         end;
      end loop;
      begin
         declare
            Unexpected : constant V.Channel_Capability := Sch.Capabilities (Channel);
         begin
            raise Program_Error
              with "failed Stacked capability returned" & V.Width (Unexpected)'Image;
         end;
      exception
         when AMS.MEL.Provider_Error =>
            null;
      end;
      Sch.Close (Channel);
      AMS.MEL.Close (Parent);
   end Test_Enable_Failure;

   --  Disable failure after a proven detach consumes the handle but raises.
   procedure Test_Disable_Failure (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "stacked-disable-fail");
      Channel : Sch.Stacked_Channel := Sch.Open (Parent, Config);
   begin
      Sch.Enable (Channel);
      begin
         Sch.Close (Channel);
         raise Program_Error with "Ada Stacked disable failure accepted";
      exception
         when AMS.MEL.Provider_Error =>
            null;
      end;
      Require (not Sch.Is_Open (Channel), "disable failure retained the handle");
      AMS.MEL.Close (Parent);
   end Test_Disable_Failure;

   --  Detach failure retains the owner; a later Close retries successfully.
   procedure Test_Detach_Failure (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "stacked-detach-fail");
      Channel : Sch.Stacked_Channel := Sch.Open (Parent, Config);
   begin
      begin
         Sch.Close (Channel);
         raise Program_Error with "Ada Stacked detach failure accepted";
      exception
         when Error : AMS.MEL.Provider_Error =>
            Require
              (Ada.Exceptions.Exception_Message (Error)
               = "Stacked detach failed; provider state retained",
               "detach diagnostic");
      end;
      Require (Sch.Is_Open (Channel), "detach failure dropped the handle");
      AMS.MEL.Close (Parent);
      Sch.Close (Channel);
      Require (not Sch.Is_Open (Channel), "retry Close");
   end Test_Detach_Failure;

   procedure Test_Open_Failure (Provider_Path : String; Scenario : String; Text : String) is
      Parent : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, Scenario);
   begin
      begin
         declare
            Channel : Sch.Stacked_Channel := Sch.Open (Parent, Config);
         begin
            Sch.Close (Channel);
         end;
         raise Program_Error with "Ada Stacked open failure accepted: " & Scenario;
      exception
         when Error : AMS.MEL.Provider_Error =>
            Require (Ada.Exceptions.Exception_Message (Error) = Text, Scenario & " diagnostic");
      end;
      --  Conditional absence is not fatal: the Session stays open and usable.
      Require (AMS.MEL.Is_Open (Parent), Scenario & " closed the Session");
      AMS.MEL.Close (Parent);
   end Test_Open_Failure;

   --  Weak view: typed and weak capabilities agree, KeepAlive and CommsTest
   --  (with cached repeated Wait) succeed while Attached and Enabled, view
   --  Close leaves the typed owner usable, and after typed Close an existing
   --  view refuses every operation.
   procedure Test_Common_Channel (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "stacked-ada-common");
      Channel : Sch.Stacked_Channel := Sch.Open (Parent, Config);
      View    : V.View := Sch.As_Channel (Channel);
   begin
      for Enabled in Boolean loop
         if Enabled then
            Sch.Enable (Channel);
         end if;
         Check_Capability (V.Capabilities (View), 23);
         Check_Capability (Sch.Capabilities (Channel), 23);
         declare
            Request : V.Return_Request := V.Send_Keep_Alive (View);
            Result  : constant V.Return_Result := V.Wait (Request, 5_000);
         begin
            Require
              (V.Status (Result) = V.Success
               and then V.Value (Result) = V.Return_Success
               and then V.Value (V.Wait (Request, 0)) = V.Return_Success,
               "KeepAlive");
            V.Close (Request);
         end;
         declare
            Request : V.Comms_Request :=
              V.Submit_Comms_Test (View, High_Channel, High_Command, High_Request);
            Report  : constant V.Comms_Test_Report := V.Report (V.Wait (Request, 5_000));
         begin
            Require
              (Report.Command_ID = High_Command
               and then Report.Request_ID = High_Request
               and then V.Report (V.Wait (Request, 0)) = Report,
               "CommsTest");
            V.Close (Request);
         end;
      end loop;
      V.Close (View);
      V.Close (View);
      Require (not V.Is_Open (View) and then Sch.Is_Open (Channel), "view Close");
      Check_Capability (Sch.Capabilities (Channel), 23);
      declare
         Later : V.View := Sch.As_Channel (Channel);
      begin
         Sch.Close (Channel);
         begin
            declare
               Unexpected : V.Return_Request := V.Send_Keep_Alive (Later);
            begin
               V.Close (Unexpected);
               raise Program_Error with "expired Stacked view accepted KeepAlive";
            end;
         exception
            when AMS.MEL.Provider_Error =>
               null;
         end;
         begin
            declare
               Unexpected : constant V.Channel_Capability := V.Capabilities (Later);
            begin
               raise Program_Error
                 with "expired Stacked view returned capabilities" & V.Width (Unexpected)'Image;
            end;
         exception
            when AMS.MEL.Provider_Error =>
               null;
         end;
         V.Close (Later);
      end;
      --  As_Channel on a closed owner raises.
      begin
         declare
            Unexpected : constant V.View := Sch.As_Channel (Channel);
         begin
            raise Program_Error
              with "As_Channel on closed Stacked owner" & V.Is_Open (Unexpected)'Image;
         end;
      exception
         when AMS.MEL.Provider_Error =>
            null;
      end;
      AMS.MEL.Close (Parent);
   end Test_Common_Channel;

   --  Operations on a closed owner raise rather than reaching the provider.
   procedure Test_Closed_Owner (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "stacked-basic");
      Channel : Sch.Stacked_Channel := Sch.Open (Parent, Config);
   begin
      Sch.Close (Channel);
      begin
         Sch.Enable (Channel);
         raise Program_Error with "Enable on closed Stacked owner accepted";
      exception
         when AMS.MEL.Provider_Error =>
            null;
      end;
      AMS.MEL.Close (Parent);
      begin
         declare
            Orphan : constant Sch.Stacked_Channel := Sch.Open (Parent, Config);
         begin
            raise Program_Error with "Open on closed Session accepted" & Sch.Is_Open (Orphan)'Image;
         end;
      exception
         when AMS.MEL.Provider_Error =>
            null;
      end;
   end Test_Closed_Owner;

   --  Discover the required channel without treating zero layout as absence,
   --  then build the configuration from its exact identity and compare every
   --  capability field through the owned-value equality operation.
   procedure Test_Discovery_Zero (Provider_Path : String) is
      Parent     : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "stacked-zero");
      Found      : constant AMS.MEL.IR.Discovery.Capability_Vectors.Vector :=
        AMS.MEL.IR.Discovery.Capabilities (Parent);
      Discovered : constant V.Channel_Capability := Found.Element (1);
      Channel    : Sch.Stacked_Channel :=
        Sch.Open
          (Parent,
           Sch.Create_Config
             (V.Channel_ID (Discovered),
              V.Platform_ID (Discovered),
              V.Sensor_Location (Discovered)));
      Attached   : constant V.Channel_Capability := Sch.Capabilities (Channel);
      use type V.Channel_Capability;
   begin
      Require (V.Channel_Type_At (Discovered, 1) = V.Stacked_Image, "discovered type");
      Require (Attached = Discovered, "all discovered capability fields preserved");
      Require
        (V.Width (Attached) = 0
         and then V.Height (Attached) = 0
         and then V.Bit_Depth (Attached) = 0
         and then V.Row_Pitch (Attached) = 0
         and then V.Buffer_Size (Attached) = 0
         and then V.Image_Band_Count (Attached) = 0
         and then V.Nav_Frame_Count (Attached) = 0,
         "zero layout and empty collections");
      Sch.Enable (Channel);
      Sch.Close (Channel);
      AMS.MEL.Close (Parent);
      Require (Attached = Discovered, "snapshot after closure");
   end Test_Discovery_Zero;

   --  Keep both actual provider futures pending across every public closure.
   --  Explicit promise release, not timing, determines completion order.
   procedure Test_Pending (Provider_Path : String) is
      package IC renames Interfaces.C;
      use type IC.int;
      use type System.Address;
      function Dlopen (Path : IC.char_array; Flags : IC.int) return System.Address
      with Import, Convention => C, External_Name => "dlopen";
      function Dlsym (Handle : System.Address; Name : IC.char_array) return System.Address
      with Import, Convention => C, External_Name => "dlsym";
      function Dlclose (Handle : System.Address) return IC.int
      with Import, Convention => C, External_Name => "dlclose";
      type Release_Function is access function (Family, Amount : IC.unsigned) return IC.int
      with Convention => C;
      function To_Release is new Ada.Unchecked_Conversion (System.Address, Release_Function);
      Pin     : constant System.Address := Dlopen (IC.To_C (Provider_Path), 2);
      Release : constant Release_Function :=
        To_Release (Dlsym (Pin, IC.To_C ("mock_completion_release_family")));
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "stacked-basic");
      Channel : Sch.Stacked_Channel := Sch.Open (Parent, Config);
      View    : V.View := Sch.As_Channel (Channel);
      Keep    : V.Return_Request := V.Send_Keep_Alive (View);
      Comms   : V.Comms_Request :=
        V.Submit_Comms_Test (View, High_Channel, High_Command, High_Request);
   begin
      Require (Pin /= System.Null_Address and then Release /= null, "pending provider gate");
      begin
         declare
            Unexpected : constant V.Return_Result := V.Wait (Keep, 0);
         begin
            raise Program_Error with "pending keepalive completed " & V.Status (Unexpected)'Image;
         end;
      exception
         when AMS.MEL.IR.Timeout_Error =>
            null;
      end;
      begin
         declare
            Unexpected : constant V.Comms_Result := V.Wait (Comms, 0);
         begin
            raise Program_Error with "pending comms completed " & V.Status (Unexpected)'Image;
         end;
      exception
         when AMS.MEL.IR.Timeout_Error =>
            null;
      end;
      Sch.Close (Channel);
      V.Close (View);
      AMS.MEL.Close (Parent);
      Require (Release (14, 1) /= 0, "release comms");
      declare
         Report : constant V.Comms_Result := V.Wait (Comms, 5_000);
      begin
         Require (V.Status (Report) = V.Success, "comms after closure");
         Require
           (V.Report (Report).Command_ID = High_Command
            and then V.Report (Report).Request_ID = High_Request,
            "pending correlation");
      end;
      Require (Release (13, 1) /= 0, "release keepalive");
      Require (V.Value (V.Wait (Keep, 5_000)) = V.Return_Success, "keepalive after closure");
      V.Close (Comms);
      V.Close (Keep);
      Require (Dlclose (Pin) = 0, "pending provider pin release");
   end Test_Pending;

   procedure Run (Provider_Path : String) is
   begin
      Test_Pending (Provider_Path);
      Test_Discovery_Zero (Provider_Path);
      Test_Open_Enable_Close (Provider_Path);
      Test_Depth (Provider_Path, "stacked-depth-zero", 0);
      Test_Depth (Provider_Path, "stacked-depth-max", Interfaces.Unsigned_32'Last);
      Test_Parent_First (Provider_Path);
      Test_Finalization (Provider_Path);
      Test_Enable_Failure (Provider_Path, "stacked-enable-fail");
      Test_Enable_Failure (Provider_Path, "stacked-enable-throw");
      Test_Disable_Failure (Provider_Path);
      Test_Detach_Failure (Provider_Path);
      Test_Open_Failure
        (Provider_Path, "stacked-not-advertised", "provider does not advertise Stacked");
      Test_Open_Failure (Provider_Path, "stacked-attach-null", "attachChannel returned null");
      Test_Open_Failure
        (Provider_Path, "stacked-wrong-concrete", "attached channel is not a StackedImageChannel");
      Test_Open_Failure
        (Provider_Path, "stacked-capability-wrong", "Stacked channel capability omits Stacked");
      Test_Open_Failure
        (Provider_Path, "stacked-attach-throw", "provider exception during Stacked open");
      Test_Common_Channel (Provider_Path);
      Test_Closed_Owner (Provider_Path);
      Ada.Text_IO.Put_Line ("PASS: Ada IR Stacked channel foundation contract");
   end Run;
end AMS_MEL_IR_Stacked_Tests;
