with Ada.Text_IO;
with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.IR.C2;
with AMS.MEL.IR.C2.Common;
with AMS.MEL.IR.Channel;
with AMS.MEL.IR.Health_Status;
with AMS.MEL.IR.Image;
with AMS.MEL.IR.Instrumentation;
with AMS.MEL.IR.Track;
with AMS.MEL.IR.Track.System_Data;
with AMS.MEL.IR.Track.Updates;
with Interfaces;

package body AMS_MEL_IR_Channel_Tests is
   package C2 renames AMS.MEL.IR.C2;
   package V renames AMS.MEL.IR.Channel;
   package H renames AMS.MEL.IR.Health_Status;
   package Instr renames AMS.MEL.IR.Instrumentation;
   package Trk renames AMS.MEL.IR.Track;
   package Upd renames AMS.MEL.IR.Track.Updates;
   package Sys renames AMS.MEL.IR.Track.System_Data;
   use type V.Command_ID;
   use type V.Comms_Request_ID;
   use type V.Comms_Test_Report;
   use type V.Outcome;
   use type V.Error_Code;
   use type V.Command_Return;
   use type V.Channel_Type;
   use type V.Pixel_Format;
   use type V.Sensor_Type;
   use type V.Band_Type;
   use type V.Coordinate_System_Type;
   use type C2.Outcome;
   use type C2.MFA_Mode;
   use type Interfaces.Unsigned_32;

   --  Canonical high-bit identifiers validated by every *-ada-common,
   --  comms-high, and Health mock scenario.
   High_Channel : constant V.Comms_Channel_ID := 16#F000_0002#;
   High_Command : constant V.Command_ID := 16#8000_0001#;
   High_Request : constant V.Comms_Request_ID := 16#E000_0003#;

   Zero     : constant AMS.MEL.IR.UUID := [others => 0];
   Chan_ID  : constant AMS.MEL.IR.UCI_ID := AMS.MEL.IR.Create_UCI_ID (Zero, "common view channel");
   Plat_ID  : constant AMS.MEL.IR.UCI_ID := AMS.MEL.IR.Create_UCI_ID (Zero, "common platform");
   Location : constant AMS.MEL.IR.Component_Location :=
     AMS.MEL.IR.Create_Component_Location (1.25, -2.5, 3.75, "station", "mock");

   C2_Config     : constant C2.Control_Config := C2.Create_Config (Chan_ID, Plat_ID, Location);
   Image_Config  : constant AMS.MEL.IR.Image_Config :=
     AMS.MEL.IR.Create_Image_Config
       (Chan_ID, Plat_ID, Location, Buffer_Count => 3, Buffer_Size => 64, Queue_Capacity => 4);
   Health_Config : constant H.Health_Config := H.Create_Config (Chan_ID, Plat_ID, Location);
   Instr_Config  : constant Instr.Instrumentation_Config :=
     Instr.Create_Config (Chan_ID, Plat_ID, Location);
   Track_Config  : constant Trk.Track_Config := Trk.Create_Config (Chan_ID, Plat_ID, Location);

   procedure Require (Condition : Boolean; Message : String) is
   begin
      if not Condition then
         raise Program_Error with Message;
      end if;
   end Require;

   function Long_Rejection return String is
      Result : String (1 .. 613) := [others => 'x'];
   begin
      Result (511) := Character'Val (16#E2#);
      Result (512) := Character'Val (16#82#);
      Result (513) := Character'Val (16#AC#);
      Result (514 .. Result'Last) := [others => 'y'];
      return Result;
   end Long_Rejection;

   function Same (Left, Right : V.Channel_Capability) return Boolean
   is (AMS.MEL.IR.Descriptive_Label (V.Channel_ID (Left))
       = AMS.MEL.IR.Descriptive_Label (V.Channel_ID (Right))
       and then V.Width (Left) = V.Width (Right)
       and then V.Height (Left) = V.Height (Right)
       and then V.Bit_Depth (Left) = V.Bit_Depth (Right)
       and then V.Format (Left) = V.Format (Right)
       and then V.Channel_Type_Count (Left) = V.Channel_Type_Count (Right)
       and then (V.Channel_Type_Count (Left) = 0
                 or else V.Channel_Type_At (Left, 1) = V.Channel_Type_At (Right, 1))
       and then V.Metadata_Capability_Count (Left) = V.Metadata_Capability_Count (Right)
       and then V.Image_Band_Count (Left) = V.Image_Band_Count (Right)
       and then V.Nav_Frame_Count (Left) = V.Nav_Frame_Count (Right));

   --  Common KeepAlive, high-ID CommsTest, and Capabilities through one View,
   --  each with a repeated cached Wait.
   procedure Exercise (View : V.View; Typed : V.Channel_Capability; Label : String) is
   begin
      Require (V.Is_Open (View), Label & ": view is not open");
      declare
         Request : V.Return_Request := V.Send_Keep_Alive (View);
         First   : constant V.Return_Result := V.Wait (Request, 5_000);
         Second  : constant V.Return_Result := V.Wait (Request, 0);
      begin
         Require (V.Is_Open (Request), Label & ": KeepAlive request not open");
         Require
           (V.Status (First) = V.Success
            and then V.Value (First) = V.Return_Success
            and then V.Status (Second) = V.Success
            and then V.Value (Second) = V.Return_Success,
            Label & ": KeepAlive result mismatch");
         V.Close (Request);
         V.Close (Request);
         Require (not V.Is_Open (Request), Label & ": KeepAlive Close did not clear");
      end;
      declare
         Request : V.Comms_Request :=
           V.Submit_Comms_Test (View, High_Channel, High_Command, High_Request);
         First   : constant V.Comms_Result := V.Wait (Request, 5_000);
         Second  : constant V.Comms_Result := V.Wait (Request, 0);
      begin
         Require (V.Is_Open (Request), Label & ": Comms request not open");
         Require
           (V.Status (First) = V.Success
            and then V.Report (First).Command_ID = High_Command
            and then V.Report (First).Request_ID = High_Request
            and then V.Status (Second) = V.Success
            and then V.Report (Second) = V.Report (First),
            Label & ": high-ID CommsTest mismatch");
         V.Close (Request);
         V.Close (Request);
         Require (not V.Is_Open (Request), Label & ": Comms Close did not clear");
      end;
      Require (Same (V.Capabilities (View), Typed), Label & ": common capability mismatch");
   end Exercise;

   procedure Expect_Expired (View : V.View; Label : String) is
   begin
      begin
         declare
            Unexpected : V.Return_Request := V.Send_Keep_Alive (View);
         begin
            V.Close (Unexpected);
            raise Program_Error with Label & ": expired KeepAlive accepted";
         end;
      exception
         when AMS.MEL.Provider_Error =>
            null;
      end;
      begin
         declare
            Unexpected : V.Comms_Request :=
              V.Submit_Comms_Test (View, High_Channel, High_Command, High_Request);
         begin
            V.Close (Unexpected);
            raise Program_Error with Label & ": expired CommsTest accepted";
         end;
      exception
         when AMS.MEL.Provider_Error =>
            null;
      end;
      begin
         declare
            Unexpected : constant V.Channel_Capability := V.Capabilities (View);
         begin
            raise Program_Error
              with Label & ": expired Capabilities returned" & V.Width (Unexpected)'Image;
         end;
      exception
         when AMS.MEL.Provider_Error =>
            null;
      end;
   end Expect_Expired;

   --  One instantiation per typed family proves the same safe-Ada contract
   --  for C2, Image, Health, Instrumentation, and Track.
   generic
      type Owner is limited private;
      Library : String;
      Label : String;
      Scenario : String;
      with function Open (Parent : AMS.MEL.Session) return Owner;
      with function Is_Open (Object : Owner) return Boolean;
      with function As_Channel (Object : Owner) return V.View;
      with function Capabilities (Object : Owner) return V.Channel_Capability;
      with procedure Activate (Object : in out Owner);
      --  Enable, or Start for Image.
      with procedure Check_Attached (Object : Owner);
      --  Typed operations that must still refuse while only Attached.
      with procedure Check_Active (Object : Owner);
      --  Typed behavior that must be unaffected after Activate.
      with procedure Close (Object : in out Owner);
   package Family_Tests is
      procedure Run_All;
   end Family_Tests;

   package body Family_Tests is
      --  Common operations while Attached and again after Enable/Start.
      procedure Lifecycle is
         Parent : AMS.MEL.Session := AMS.MEL.Open (Library, Scenario);
         Object : Owner := Open (Parent);
         View   : V.View := As_Channel (Object);
      begin
         Exercise (View, Capabilities (Object), Label & " attached");
         Check_Attached (Object);
         Activate (Object);
         Exercise (View, Capabilities (Object), Label & " active");
         Check_Active (Object);
         V.Close (View);
         Close (Object);
         AMS.MEL.Close (Parent);
      end Lifecycle;

      --  Typed owner and Session close first; the safe View stays alive but
      --  expired. No hidden Ada strong owner keeps the family reachable.
      procedure Weak is
         Parent : AMS.MEL.Session := AMS.MEL.Open (Library, Scenario);
         Object : Owner := Open (Parent);
         View   : V.View := As_Channel (Object);
      begin
         Close (Object);
         AMS.MEL.Close (Parent);
         Require (V.Is_Open (View), Label & ": weak view closed with its owner");
         Expect_Expired (View, Label & " weak");
         V.Close (View);
         Require (not V.Is_Open (View), Label & ": expired view Close did not clear");
         V.Close (View);
      end Weak;

      --  Closing the View first leaves the typed owner fully usable.
      procedure Close_First is
         Parent : AMS.MEL.Session := AMS.MEL.Open (Library, Scenario);
         Object : Owner := Open (Parent);
         Before : constant V.Channel_Capability := Capabilities (Object);
         View   : V.View := As_Channel (Object);
      begin
         V.Close (View);
         V.Close (View);
         Require (not V.Is_Open (View), Label & ": view Close did not clear");
         Require (Is_Open (Object), Label & ": view Close closed the typed owner");
         Require (Same (Capabilities (Object), Before), Label & ": typed capability after Close");
         Activate (Object);
         Check_Active (Object);
         Require (Same (Capabilities (Object), Before), Label & ": typed capability after Enable");
         Close (Object);
         Require (not Is_Open (Object), Label & ": typed Close failed");
         AMS.MEL.Close (Parent);
      --  View finalization at scope exit after explicit Close is harmless.
      end Close_First;

      --  Two Views from one owner are independent weak views.
      procedure Multiple is
         Parent : AMS.MEL.Session := AMS.MEL.Open (Library, Scenario);
         Object : Owner := Open (Parent);
         First  : V.View := As_Channel (Object);
         Second : V.View := As_Channel (Object);
      begin
         V.Close (First);
         Require
           (not V.Is_Open (First) and then V.Is_Open (Second), Label & ": views not distinct");
         Exercise (Second, Capabilities (Object), Label & " second view");
         Close (Object);
         AMS.MEL.Close (Parent);
         Expect_Expired (Second, Label & " second view");
         V.Close (Second);
      end Multiple;

      --  As_Channel on a closed typed owner raises and yields no open View.
      procedure Closed_Owner is
         Parent : AMS.MEL.Session := AMS.MEL.Open (Library, Scenario);
         Object : Owner := Open (Parent);
      begin
         Close (Object);
         begin
            declare
               Unexpected : constant V.View := As_Channel (Object);
            begin
               raise Program_Error
                 with
                   Label & ": As_Channel on closed owner returned" & V.Is_Open (Unexpected)'Image;
            end;
         exception
            when AMS.MEL.Provider_Error =>
               null;
         end;
         AMS.MEL.Close (Parent);
      end Closed_Owner;

      procedure Run_All is
      begin
         Lifecycle;
         Weak;
         Close_First;
         Multiple;
         Closed_Owner;
      end Run_All;
   end Family_Tests;

   procedure No_Check (Object : C2.Control_Channel) is null;
   procedure No_Check (Object : AMS.MEL.IR.Image_Stream) is null;
   procedure No_Check (Object : H.Health_Channel) is null;

   --  C2 adapters.
   function Open_C2 (Parent : AMS.MEL.Session) return C2.Control_Channel
   is (C2.Open (Parent, C2_Config));
   procedure C2_Active (Object : C2.Control_Channel) is
      --  Typed C2 commands keep working after the common View was used.
      Request : C2.Mode_Request := C2.Submit_Operate (Object, 16#8000_0001#);
      Result  : constant C2.Mode_Result := C2.Wait (Request, 5_000);
   begin
      Require
        (C2.Status (Result) = C2.Success and then C2.Mode (Result) = C2.Task_Sched,
         "C2 typed Operate after common use failed");
      C2.Close (Request);
   end C2_Active;

   --  Image adapters.
   function Open_Image (Parent : AMS.MEL.Session) return AMS.MEL.IR.Image_Stream
   is (AMS.MEL.IR.Open_Image_Stream (Parent, Image_Config));

   --  Health adapters.
   function Open_Health (Parent : AMS.MEL.Session) return H.Health_Channel
   is (H.Open (Parent, Health_Config));

   --  Instrumentation adapters.
   Level : constant Instr.Instrumentation_Level_Command :=
     (Command_ID => 16#E123_4567#, Priority => Instr.Debug);
   function Open_Instr (Parent : AMS.MEL.Session) return Instr.Instrumentation_Channel
   is (Instr.Open (Parent, Instr_Config));
   procedure Instr_Attached (Object : Instr.Instrumentation_Channel) is
   begin
      --  The common View must not relax the typed Enabled-only Submit.
      declare
         Unexpected : Instr.Instrumentation_Request := Instr.Submit (Object, Level);
      begin
         Instr.Close (Unexpected);
         raise Program_Error with "Instrumentation Submit accepted while Attached";
      end;
   exception
      when AMS.MEL.Provider_Error =>
         null;
   end Instr_Attached;
   procedure Instr_Active (Object : Instr.Instrumentation_Channel) is
      use type Instr.Instrumentation_Outcome;
      Request : Instr.Instrumentation_Request := Instr.Submit (Object, Level);
   begin
      Require
        (Instr.Status (Instr.Wait (Request, 5_000)) = Instr.Success,
         "Instrumentation typed Submit after Enable failed");
      Instr.Close (Request);
   end Instr_Active;

   --  Track adapters.
   function Open_Track (Parent : AMS.MEL.Session) return Trk.Track_Channel
   is (Trk.Open (Parent, Track_Config));
   procedure Track_Attached (Object : Trk.Track_Channel) is
      No_ID    : constant AMS.MEL.IR.UCI_ID := AMS.MEL.IR.Create_UCI_ID (Zero, "");
      Update   : constant Upd.Track_Data_Update :=
        (Platform_ID                 => 0,
         Capability_UUID             => No_ID,
         Activity_UUID               => No_ID,
         Track_ID                    => 0,
         Entity_UUID                 => No_ID,
         Status                      => Upd.Create,
         Time_Of_Validity_Seconds    => 0.0,
         Time_Of_Last_Update_Seconds => 0.0,
         Position_ECEF               => (0.0, 0.0, 0.0),
         Velocity_ECEF               => (0.0, 0.0, 0.0),
         Covariance                  => (others => 0.0),
         Maneuver_Probability        => 0.0,
         Track_Quality               => 0.0);
      Response : constant Sys.System_Track_Data_Response :=
        (System_Time_NS       => 0,
         Command_ID           => 0,
         Request_ID           => 0,
         Track_ID             => 0,
         Range_M              => 0.0,
         Range_Rate_MPS       => 0.0,
         Range_Error_M        => 0.0,
         Range_Rate_Error_MPS => 0.0,
         Az_El_Valid          => False,
         Inertial_Az_El       => (0.0, 0.0),
         Az_El_Error          => (0.0, 0.0),
         Range_Valid          => False);
   begin
      --  TrackDataUpdate and SystemTrackDataResponse remain Enabled-only.
      begin
         declare
            Unexpected : Upd.Update_Request := Upd.Submit (Object, Update);
         begin
            Upd.Close (Unexpected);
            raise Program_Error with "TrackDataUpdate accepted while Attached";
         end;
      exception
         when AMS.MEL.Provider_Error =>
            null;
      end;
      begin
         declare
            Unexpected : Sys.Response_Request := Sys.Submit (Object, Response);
         begin
            Sys.Close (Unexpected);
            raise Program_Error with "SystemTrackDataResponse accepted while Attached";
         end;
      exception
         when AMS.MEL.Provider_Error =>
            null;
      end;
   end Track_Attached;
   procedure No_Check (Object : Trk.Track_Channel) is null;

   --  The rich ChannelCapability assertions reused from the legacy
   --  C2.Common capability test.
   procedure Check_Rich (Value : V.Channel_Capability; Label : String) is
   begin
      if AMS.MEL.IR.Descriptive_Label (V.Channel_ID (Value)) /= "channel-α"
        or else V.Height (Value) /= 1080
        or else V.Width (Value) /= 1920
        or else V.Bit_Depth (Value) /= 12
        or else V.Row_Pitch (Value) /= 4096
        or else V.Buffer_Size (Value) /= 8_388_608
        or else V.Image_Size (Value) /= 4_147_200
        or else V.Number_Of_Bands (Value) /= 3
        or else V.Format (Value) /= V.RGB
        or else V.Sensor_Type_Count (Value) /= 2
        or else V.Sensor_Type_At (Value, 1) /= V.Gimbal_Horizontal
        or else V.Sensor_Type_At (Value, 2) /= V.Step_Stare
        or else AMS.MEL.IR.Descriptive_Label (V.Platform_ID (Value)) /= "platform-€"
        or else AMS.MEL.IR.Offset_X_M (V.Sensor_Location (Value)) /= 1.25
        or else AMS.MEL.IR.Offset_Y_M (V.Sensor_Location (Value)) /= -2.5
        or else AMS.MEL.IR.Offset_Z_M (V.Sensor_Location (Value)) /= 3.75
        or else AMS.MEL.IR.Key (V.Sensor_Location (Value)) /= "sensor-key"
        or else AMS.MEL.IR.System_Name (V.Sensor_Location (Value)) /= "system-β"
        or else V.Channel_Type_Count (Value) /= 3
        or else V.Channel_Type_At (Value, 1) /= V.Command_And_Control
        or else V.Channel_Type_At (Value, 3) /= V.Reserved_2
        or else V.Task_Schedule_Depth (Value) /= 17
        or else not V.ODC_Available (Value)
        or else not V.NUC_Available (Value)
        or else V.Metadata_Capability_Count (Value) /= 4
        or else not V.Has_Metadata_Capability (Value, V.Channel_Comms_Test_Rep)
        or else V.Image_Band_Count (Value) /= 2
        or else V.Image_Band_Index_At (Value, 1) /= 2
        or else V.Image_Band_Info_Count (Value, 1) /= 2
        or else V.Image_Band_Info_At (Value, 1, 1).Kind /= V.IR_Longwave
        or else V.Image_Band_Index_At (Value, 2) /= 9
        or else V.Nav_Frame_Count (Value) /= 2
        or else V.Nav_Frame_At (Value, 1) /= V.NED_Sensor
        or else V.Nav_Frame_At (Value, 2) /= V.ECEF
      then
         raise Program_Error with Label & ": rich ChannelCapability mismatch";
      end if;
   end Check_Rich;

   --  Upstream Return::Fail is a successful request completion.
   procedure Test_Return_Fail (Library : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Library, "keepalive-fail");
      Control : C2.Control_Channel := C2.Open (Parent, C2_Config);
      View    : V.View := C2.As_Channel (Control);
      Request : V.Return_Request := V.Send_Keep_Alive (View);
      Result  : constant V.Return_Result := V.Wait (Request, 5_000);
   begin
      Require
        (V.Status (Result) = V.Success
         and then V.Value (Result) = V.Fail
         and then V.Value (V.Wait (Request, 0)) = V.Fail,
         "Return::Fail was not Success/Fail");
      V.Close (Request);
      V.Close (View);
      C2.Close (Control);
      AMS.MEL.Close (Parent);
   end Test_Return_Fail;

   --  ErrorOr rejections with the complete long UTF-8 diagnostic (613 bytes,
   --  larger than the 512-byte fixed buffer, forcing the cached retry).
   procedure Test_Rejections (Library : String) is
   begin
      declare
         Parent  : AMS.MEL.Session := AMS.MEL.Open (Library, "keepalive-reject");
         Control : C2.Control_Channel := C2.Open (Parent, C2_Config);
         View    : V.View := C2.As_Channel (Control);
         Request : V.Return_Request := V.Send_Keep_Alive (View);
         Result  : constant V.Return_Result := V.Wait (Request, 5_000);
         Again   : constant V.Return_Result := V.Wait (Request, 0);
      begin
         Require
           (V.Status (Result) = V.Rejected
            and then V.Rejection_Code (Result) = V.Invalid_State
            and then V.Description (Result) = Long_Rejection
            and then V.Status (Again) = V.Rejected
            and then V.Rejection_Code (Again) = V.Invalid_State
            and then V.Description (Again) = Long_Rejection,
            "KeepAlive rejection mismatch");
         V.Close (Request);
         V.Close (View);
         C2.Close (Control);
         AMS.MEL.Close (Parent);
      end;
      declare
         Parent  : AMS.MEL.Session := AMS.MEL.Open (Library, "comms-reject");
         Control : C2.Control_Channel := C2.Open (Parent, C2_Config);
         View    : V.View := C2.As_Channel (Control);
         Request : V.Comms_Request := V.Submit_Comms_Test (View, 1, 2, 3);
         Result  : constant V.Comms_Result := V.Wait (Request, 5_000);
         Again   : constant V.Comms_Result := V.Wait (Request, 0);
      begin
         Require
           (V.Status (Result) = V.Rejected
            and then V.Rejection_Code (Result) = V.Invalid_Parameters
            and then V.Description (Result) = Long_Rejection
            and then V.Status (Again) = V.Rejected
            and then V.Description (Again) = Long_Rejection,
            "CommsTest rejection mismatch");
         V.Close (Request);
         V.Close (View);
         C2.Close (Control);
         AMS.MEL.Close (Parent);
      end;
   end Test_Rejections;

   --  Wait(0) times out; a later Wait returns the terminal result, which is
   --  then cached. The delayed mock completion is the public timeout contract.
   procedure Test_Timeout (Library : String) is
   begin
      declare
         Parent  : AMS.MEL.Session := AMS.MEL.Open (Library, "keepalive-delayed");
         Control : C2.Control_Channel := C2.Open (Parent, C2_Config);
         View    : V.View := C2.As_Channel (Control);
         Request : V.Return_Request := V.Send_Keep_Alive (View);
      begin
         begin
            declare
               Unexpected : constant V.Return_Result := V.Wait (Request, 0);
            begin
               raise Program_Error with "KeepAlive did not time out" & V.Status (Unexpected)'Image;
            end;
         exception
            when AMS.MEL.IR.Timeout_Error =>
               null;
         end;
         Require (V.Is_Open (Request), "timeout consumed the KeepAlive request");
         Require
           (V.Value (V.Wait (Request, 5_000)) = V.Return_Success
            and then V.Value (V.Wait (Request, 0)) = V.Return_Success,
            "KeepAlive cached result after timeout mismatch");
         V.Close (Request);
         V.Close (View);
         C2.Close (Control);
         AMS.MEL.Close (Parent);
      end;
      declare
         Parent  : AMS.MEL.Session := AMS.MEL.Open (Library, "comms-delayed");
         Control : C2.Control_Channel := C2.Open (Parent, C2_Config);
         View    : V.View := C2.As_Channel (Control);
         Request : V.Comms_Request := V.Submit_Comms_Test (View, 7, 17, 19);
      begin
         begin
            declare
               Unexpected : constant V.Comms_Result := V.Wait (Request, 0);
            begin
               raise Program_Error with "CommsTest did not time out" & V.Status (Unexpected)'Image;
            end;
         exception
            when AMS.MEL.IR.Timeout_Error =>
               null;
         end;
         declare
            Later  : constant V.Comms_Result := V.Wait (Request, 5_000);
            Cached : constant V.Comms_Result := V.Wait (Request, 0);
         begin
            Require
              (V.Status (Later) = V.Success
               and then V.Report (Later) = (Command_ID => 17, Request_ID => 19)
               and then V.Report (Cached) = V.Report (Later),
               "CommsTest cached result after timeout mismatch");
         end;
         V.Close (Request);
         V.Close (View);
         C2.Close (Control);
         AMS.MEL.Close (Parent);
      end;
   end Test_Timeout;

   --  Parent-first: View, typed owner, and Session close while a KeepAlive is
   --  pending; the safe Return_Request still completes because the admitted
   --  native request, not the Ada View, owns the family graph.
   procedure Test_Parent_First (Library : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Library, "keepalive-lifetime");
      Control : C2.Control_Channel := C2.Open (Parent, C2_Config);
      View    : V.View := C2.As_Channel (Control);
      Request : V.Return_Request := V.Send_Keep_Alive (View);
   begin
      begin
         declare
            Unexpected : constant V.Return_Result := V.Wait (Request, 0);
         begin
            raise Program_Error
              with "parent-first KeepAlive did not time out" & V.Status (Unexpected)'Image;
         end;
      exception
         when AMS.MEL.IR.Timeout_Error =>
            null;
      end;
      V.Close (View);
      C2.Close (Control);
      AMS.MEL.Close (Parent);
      Require
        (not V.Is_Open (View) and then not C2.Is_Open (Control) and then V.Is_Open (Request),
         "parent-first owners did not close independently");
      declare
         Result : constant V.Return_Result := V.Wait (Request, 5_000);
      begin
         Require
           (V.Status (Result) = V.Success
            and then V.Value (Result) = V.Return_Success
            and then V.Value (V.Wait (Request, 0)) = V.Return_Success,
            "parent-first KeepAlive did not complete");
      end;
      V.Close (Request);
   end Test_Parent_First;

   --  The rich snapshot remains fully readable after View, typed owner, and
   --  Session close (and therefore after provider unload).
   procedure Test_Capability_Snapshot (Library : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Library, "capability-rich");
      Control : C2.Control_Channel := C2.Open (Parent, C2_Config);
      View    : V.View := C2.As_Channel (Control);
      Value   : constant V.Channel_Capability := V.Capabilities (View);
   begin
      V.Close (View);
      C2.Close (Control);
      AMS.MEL.Close (Parent);
      Check_Rich (Value, "C2 view snapshot");
   end Test_Capability_Snapshot;

   --  Scope-only finalization of View, Return_Request, and Comms_Request, and
   --  explicit Close followed by finalization. Finalizers never raise.
   procedure Test_Finalization (Library : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Library, "comms-high");
      Control : C2.Control_Channel := C2.Open (Parent, C2_Config);
   begin
      for Iteration in 1 .. 3 loop
         declare
            View  : constant V.View := C2.As_Channel (Control);
            Ret   : constant V.Return_Request := V.Send_Keep_Alive (View);
            Comms : constant V.Comms_Request :=
              V.Submit_Comms_Test (View, High_Channel, High_Command, High_Request);
         begin
            Require
              (V.Is_Open (View) and then V.Is_Open (Ret) and then V.Is_Open (Comms),
               "finalization owners did not open" & Iteration'Image);
         --  Leave the scope without Close: only finalization releases.
         end;
      end loop;
      declare
         View  : V.View := C2.As_Channel (Control);
         Ret   : V.Return_Request := V.Send_Keep_Alive (View);
         Comms : V.Comms_Request :=
           V.Submit_Comms_Test (View, High_Channel, High_Command, High_Request);
      begin
         Require
           (V.Report (V.Wait (Comms, 5_000)).Command_ID = High_Command,
            "finalization Comms result mismatch");
         V.Close (Comms);
         V.Close (Ret);
         V.Close (View);
      --  Finalization after explicit Close is harmless.
      end;
      --  The typed owner was never affected by any View finalization.
      Require (C2.Is_Open (Control), "View finalization closed the typed owner");
      C2.Enable (Control);
      C2.Close (Control);
      AMS.MEL.Close (Parent);
   end Test_Finalization;

   --  C2.Command_ID remains a usable name for existing C2 APIs and is the
   --  same type as the canonical Channel.Command_ID, with no conversion.
   procedure Test_Command_ID_Compatibility (Library : String) is
      Legacy    : constant C2.Command_ID := 16#8000_0001#;
      --  No conversion in either direction: this compiles only because the
      --  two names denote one type.
      Canonical : constant V.Command_ID := Legacy;
      Back      : constant C2.Command_ID := Canonical;
      pragma Compile_Time_Error (C2.Command_ID'Size /= 32, "C2.Command_ID'Size is not 32");
      pragma Compile_Time_Error (V.Command_ID'Size /= 32, "Channel.Command_ID'Size is not 32");
      pragma
        Compile_Time_Error (C2.Command_ID'Last /= V.Command_ID'Last, "Command_ID ranges differ");
      Parent    : AMS.MEL.Session := AMS.MEL.Open (Library, "c2-command-id");
      Control   : C2.Control_Channel := C2.Open (Parent, C2_Config);
   begin
      C2.Enable (Control);
      declare
         Typed_ID : constant C2.Command_ID := 16#89AB_CDEF#;
         Request  : C2.Mode_Request := C2.Submit_Operate (Control, Typed_ID);
      begin
         Require (C2.Status (C2.Wait (Request, 5_000)) = C2.Success, "C2 Operate failed");
         C2.Close (Request);
      end;
      declare
         --  A canonical Channel.Command_ID passes directly to a C2 API.
         Canonical_ID : constant V.Command_ID := 16#89AB_CDEF#;
         Request      : C2.Mode_Request := C2.Submit_Operate (Control, Canonical_ID);
      begin
         Require (C2.Status (C2.Wait (Request, 5_000)) = C2.Success, "C2 Operate failed");
         C2.Close (Request);
      end;
      declare
         View    : V.View := C2.As_Channel (Control);
         --  A legacy C2.Command_ID passes directly to the common facade.
         Request : V.Comms_Request := V.Submit_Comms_Test (View, 1, Legacy, 3);
         Report  : constant V.Comms_Test_Report := V.Report (V.Wait (Request, 5_000));
         Echo    : constant C2.Command_ID := Report.Command_ID;
      begin
         Require
           (Echo = Legacy and then Echo = Canonical and then Report.Command_ID = Back,
            "Command_ID round trip mismatch");
         V.Close (Request);
         V.Close (View);
      end;
      C2.Close (Control);
      AMS.MEL.Close (Parent);
   end Test_Command_ID_Compatibility;

   procedure Run (Provider_Path : String) is
      package C2_Family is new
        Family_Tests
          (Owner          => C2.Control_Channel,
           Library        => Provider_Path,
           Label          => "C2",
           Scenario       => "comms-high",
           Open           => Open_C2,
           Is_Open        => C2.Is_Open,
           As_Channel     => C2.As_Channel,
           Capabilities   => C2.Common.Capabilities,
           Activate       => C2.Enable,
           Check_Attached => No_Check,
           Check_Active   => C2_Active,
           Close          => C2.Close);
      package Image_Family is new
        Family_Tests
          (Owner          => AMS.MEL.IR.Image_Stream,
           Library        => Provider_Path,
           Label          => "Image",
           Scenario       => "comms-high",
           Open           => Open_Image,
           Is_Open        => AMS.MEL.IR.Is_Open,
           As_Channel     => AMS.MEL.IR.Image.As_Channel,
           Capabilities   => AMS.MEL.IR.Image.Capabilities,
           Activate       => AMS.MEL.IR.Start,
           Check_Attached => No_Check,
           Check_Active   => No_Check,
           Close          => AMS.MEL.IR.Close);
      package Health_Family is new
        Family_Tests
          (Owner          => H.Health_Channel,
           Library        => Provider_Path,
           Label          => "Health",
           Scenario       => "health-ada-common",
           Open           => Open_Health,
           Is_Open        => H.Is_Open,
           As_Channel     => H.As_Channel,
           Capabilities   => H.Capabilities,
           Activate       => H.Enable,
           Check_Attached => No_Check,
           Check_Active   => No_Check,
           Close          => H.Close);
      package Instr_Family is new
        Family_Tests
          (Owner          => Instr.Instrumentation_Channel,
           Library        => Provider_Path,
           Label          => "Instrumentation",
           Scenario       => "instr-ada-common",
           Open           => Open_Instr,
           Is_Open        => Instr.Is_Open,
           As_Channel     => Instr.As_Channel,
           Capabilities   => Instr.Capabilities,
           Activate       => Instr.Enable,
           Check_Attached => Instr_Attached,
           Check_Active   => Instr_Active,
           Close          => Instr.Close);
      package Track_Family is new
        Family_Tests
          (Owner          => Trk.Track_Channel,
           Library        => Provider_Path,
           Label          => "Track",
           Scenario       => "track-ada-common",
           Open           => Open_Track,
           Is_Open        => Trk.Is_Open,
           As_Channel     => Trk.As_Channel,
           Capabilities   => Trk.Capabilities,
           Activate       => Trk.Enable,
           Check_Attached => Track_Attached,
           Check_Active   => No_Check,
           Close          => Trk.Close);
   begin
      C2_Family.Run_All;
      Image_Family.Run_All;
      Health_Family.Run_All;
      Instr_Family.Run_All;
      Track_Family.Run_All;
      Test_Return_Fail (Provider_Path);
      Test_Rejections (Provider_Path);
      Test_Timeout (Provider_Path);
      Test_Parent_First (Provider_Path);
      Test_Capability_Snapshot (Provider_Path);
      Test_Finalization (Provider_Path);
      Test_Command_ID_Compatibility (Provider_Path);
      Ada.Text_IO.Put_Line ("PASS: Ada safe common Channel facade contract (five families)");
   end Run;
end AMS_MEL_IR_Channel_Tests;
