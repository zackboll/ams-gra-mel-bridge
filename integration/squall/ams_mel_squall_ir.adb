with Ada.Command_Line;
with Ada.Exceptions;
with Ada.Text_IO;
with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.IR.Image;
with AMS.MEL.IR.Image.Metadata;
with AMS.MEL.IR.C2;
with AMS.MEL.IR.C2.Metadata;
with AMS.MEL.IR.C2.Common;
with AMS.MEL.IR.Channel;
with AMS.MEL.IR.Health_Status;
with AMS.MEL.IR.Health_Status.Metadata;
with AMS.MEL.IR.Instrumentation;
with AMS.MEL.IR.Track;
with AMS.MEL.Status;
with Interfaces;

procedure AMS_MEL_Squall_IR is
   package C2 renames AMS.MEL.IR.C2;
   package Metadata renames AMS.MEL.IR.C2.Metadata;
   package Common renames AMS.MEL.IR.C2.Common;
   package Channel_Value renames AMS.MEL.IR.Channel;
   package Image_Metadata renames AMS.MEL.IR.Image.Metadata;
   package Health renames AMS.MEL.IR.Health_Status;
   package Health_Metadata renames AMS.MEL.IR.Health_Status.Metadata;
   package Instrumentation renames AMS.MEL.IR.Instrumentation;
   package Track renames AMS.MEL.IR.Track;
   package Status renames AMS.MEL.Status;
   use type AMS.MEL.IR.Counter;
   use type C2.MFA_Mode;
   use type C2.Command_Return;
   use type C2.Error_Code;
   use type C2.Outcome;
   use type C2.Command_ID;
   use type Metadata.Metadata_Kind;
   use type Metadata.Command_State;
   use type Metadata.Cannot_Comply;
   use type Channel_Value.Channel_Type;
   use type Channel_Value.Pixel_Format;
   use type AMS.MEL.IR.Image.Full_Image_Type;
   use type AMS.MEL.IR.Image_Flip;
   use type AMS.MEL.IR.Byte;
   use type Channel_Value.Metadata_Capability;
   use type Channel_Value.Comms_Request_ID;
   use type Channel_Value.Comms_Test_Report;
   use type Health_Metadata.Metadata_Kind;
   use type Health_Metadata.Failure_Level;
   use type Image_Metadata.Metadata_Kind;
   use type Image_Metadata.Azimuth_Elevation;
   use type AMS.MEL.IR.Image.Navigation_Outcome;
   use type Status.Euler;
   use type Status.MFA_State;
   use type Status.State_Transition_Status;
   use type Interfaces.Unsigned_32;
   use type Interfaces.Unsigned_64;

   Channel_UUID : constant AMS.MEL.IR.UUID :=
     [0 => 16#00#, 1 => 16#40#, 2 => 16#04#, 3 => 16#10#,
      4 => 16#11#, 5 => 16#22#, 6 => 16#43#, 7 => 16#44#,
      8 => 16#85#, 9 => 16#66#, 10 => 16#77#, 11 => 16#88#,
      12 => 16#99#, 13 => 16#AA#, 14 => 16#BB#, 15 => 16#CC#];
   Platform_UUID : constant AMS.MEL.IR.UUID :=
     [0 => 16#00#, 1 => 16#40#, 2 => 16#04#, 3 => 16#11#,
      4 => 16#21#, 5 => 16#32#, 6 => 16#43#, 7 => 16#54#,
      8 => 16#86#, 9 => 16#67#, 10 => 16#78#, 11 => 16#89#,
      12 => 16#9A#, 13 => 16#AB#, 14 => 16#BC#, 15 => 16#CD#];
   Channel_ID : constant AMS.MEL.IR.UCI_ID :=
     AMS.MEL.IR.Create_UCI_ID (Channel_UUID, "Task 004 Ada IR");
   Platform_ID : constant AMS.MEL.IR.UCI_ID :=
     AMS.MEL.IR.Create_UCI_ID (Platform_UUID, "Task 004 integration platform");
   Location : constant AMS.MEL.IR.Component_Location :=
     AMS.MEL.IR.Create_Component_Location
       (0.0, 0.0, 0.0, "task-004-station", "ams-mel-squall-integration");
   Image_Config : constant AMS.MEL.IR.Image_Config :=
     AMS.MEL.IR.Create_Image_Config
       (Channel_ID, Platform_ID, Location, Buffer_Count => 4,
        Buffer_Size => 1024 * 1024, Queue_Capacity => 8);
   C2_Config : constant C2.Control_Config :=
     C2.Create_Config (Channel_ID, Platform_ID, Location);
   Health_Config : constant Health.Health_Config :=
      Health.Create_Config (Channel_ID, Platform_ID, Location);
   Instrumentation_Config : constant Instrumentation.Instrumentation_Config :=
      Instrumentation.Create_Config (Channel_ID, Platform_ID, Location);
   Track_Config : constant Track.Track_Config :=
      Track.Create_Config (Channel_ID, Platform_ID, Location);

   function Checksum (Pixels : AMS.MEL.IR.Pixel_Array) return Interfaces.Unsigned_64 is
      Value : Interfaces.Unsigned_64 := 16#1465_0FB0_739D_0383#;
   begin
      for Pixel of Pixels loop
         Value := (Value xor Interfaces.Unsigned_64 (Pixel)) * 16#0000_0100_0000_01B3#;
      end loop;
      return Value;
   end Checksum;

   procedure Expect_Command_Status
     (Stream : Metadata.Metadata_Stream; ID : C2.Command_ID;
      State : Metadata.Command_State; Reason : Metadata.Cannot_Comply;
      Description_Empty : Boolean)
   is
      Event : constant Metadata.Metadata_Event := Metadata.Receive (Stream, 5_000);
      Status : constant Metadata.Command_Status := Metadata.Command (Event);
   begin
      if Metadata.Kind (Event) /= Metadata.Command_Status_Event
        or else Metadata.Command_ID (Status) /= ID
        or else Metadata.State (Status) /= State
        or else Metadata.Reason (Status) /= Reason
        or else (Metadata.Reason_Description (Status)'Length = 0) /= Description_Empty
      then
         raise Program_Error with "Squall CommandStatus mismatch for" & ID'Image;
      end if;
      Ada.Text_IO.Put_Line
        ("CommandStatus: id=" & ID'Image & " state=" &
         Metadata.Command_State'Image (State) & " reason=" &
         Metadata.Cannot_Comply'Image (Reason) & " description=" &
         Metadata.Reason_Description (Status));
   end Expect_Command_Status;

   procedure Expect_Empty_BIT_Status (Stream : Metadata.Metadata_Stream) is
      Event : constant Metadata.Metadata_Event := Metadata.Receive (Stream, 5_000);
   begin
      if Metadata.Kind (Event) /= Metadata.BIT_Status_Event
        or else Metadata.Active_BIT_Count (Event) /= 0
        or else Metadata.Completed_BIT_Count (Event) /= 0
        or else Metadata.Fault_Count (Event) /= 0
      then
         raise Program_Error with "Squall BIT_Status was not well-formed and empty";
      end if;
      Ada.Text_IO.Put_Line ("BIT_Status: empty default");
   end Expect_Empty_BIT_Status;

begin
   if Ada.Command_Line.Argument_Count < 2 or else Ada.Command_Line.Argument_Count > 4 then
      Ada.Text_IO.Put_Line
        (Ada.Text_IO.Standard_Error,
         "usage: ams_mel_squall_ir PROVIDER_SO PROFILE_JSON [FRAME_COUNT] [TIMEOUT_MS]");
      Ada.Command_Line.Set_Exit_Status (Ada.Command_Line.Failure);
      return;
   end if;

   declare
      Frame_Count : constant Positive :=
        (if Ada.Command_Line.Argument_Count >= 3
         then Positive'Value (Ada.Command_Line.Argument (3)) else 3);
      Timeout_MS : constant Positive :=
        (if Ada.Command_Line.Argument_Count >= 4
         then Positive'Value (Ada.Command_Line.Argument (4)) else 10_000);
      Parent : AMS.MEL.Session := AMS.MEL.Open
        (Ada.Command_Line.Argument (1), Ada.Command_Line.Argument (2));
      Version : constant AMS.MEL.Provider_Version :=
        AMS.MEL.Query_Provider_Version (Parent);
      Stream : AMS.MEL.IR.Image_Stream :=
        AMS.MEL.IR.Open_Image_Stream (Parent, Image_Config);
      Image_Metadata_Stream : Image_Metadata.Metadata_Stream :=
        Image_Metadata.Open (Stream, 32);
   begin
      if Frame_Count < 3 then
         raise Constraint_Error with "frame count must be at least three";
      end if;
      Ada.Text_IO.Put_Line
        ("provider version: api=" & AMS.MEL.Provider_Version_Number'Image
           (AMS.MEL.API_Version (Version)) & " library=" &
         AMS.MEL.Provider_Version_Number'Image (AMS.MEL.Library_Version (Version)) &
         " vendor=" & AMS.MEL.Vendor (Version) &
         " description=" & AMS.MEL.Description (Version));

      declare
         Capability : constant Channel_Value.Channel_Capability :=
           AMS.MEL.IR.Image.Capabilities (Stream);
      begin
         if Channel_Value.Width (Capability) /= 320
           or else Channel_Value.Height (Capability) /= 200
           or else Channel_Value.Bit_Depth (Capability) /= 8
           or else Channel_Value.Number_Of_Bands (Capability) /= 1
           or else Channel_Value.Format (Capability) /= Channel_Value.Mono
           or else not Channel_Value.Has_Metadata_Capability
             (Capability, Channel_Value.Bad_Pixel_List)
            or else not Channel_Value.Has_Metadata_Capability
              (Capability, Channel_Value.Line_Of_Sight_Report)
            or else not Channel_Value.Has_Metadata_Capability
              (Capability, Channel_Value.Line_Of_Sight_Euler)
            or else not Channel_Value.Has_Metadata_Capability
              (Capability, Channel_Value.Navigation_Report_Resp)
         then
            raise Program_Error with "unexpected Squall Image capability";
         end if;
         Ada.Text_IO.Put_Line
           ("Image capability: geometry=320x200 bit-depth=8 bands=1 format=MONO "
             & "metadata=BAD_PIXEL_LIST,LINE_OF_SIGHT_REPORT,LINE_OF_SIGHT_EULER,NAVIGATION_REPORT_RESP");
      end;

      --  Pinned Squall b1015728f904c799fa0c07489fce48e78f67845f does NOT
      --  support the conditionally required Instrumentation channel: its
      --  SquallControl::attachChannel handles only IRSTImage,
      --  CommandAndControl, and HealthAndStatus and returns nullptr for
      --  Instrumentation. This proves the binding surface fails cleanly
      --  against that unsupported provider profile rather than treating an
      --  unsupported optional family as a binding failure. It is NOT positive
      --  Instrumentation execution evidence.
      begin
         declare
            Rejected : Instrumentation.Instrumentation_Channel :=
              Instrumentation.Open (Parent, Instrumentation_Config);
         begin
            Instrumentation.Close (Rejected);
            raise Program_Error with
              "pinned Squall unexpectedly attached an Instrumentation channel";
         end;
      exception
         when Error : AMS.MEL.Provider_Error =>
            if Ada.Exceptions.Exception_Message (Error) /= "attachChannel returned null" then
               raise Program_Error with
                 "unexpected Squall Instrumentation failure: "
                 & Ada.Exceptions.Exception_Message (Error);
            end if;
            Ada.Text_IO.Put_Line
              ("Instrumentation: pinned Squall unsupported as expected "
               & "(Provider_Error: attachChannel returned null)");
      end;

      --  Task 029B3: pinned Squall likewise does NOT support the
      --  conditionally required Track channel through this bridge's
      --  Control::attachChannel path; the default branch returns nullptr and
      --  the exported createTrackChannel also returns nullptr. This proves the
      --  Track binding fails cleanly against that unsupported provider profile
      --  on the already-open Session. It is NOT positive Track execution or
      --  IRSTTrackReport evidence.
      begin
         declare
            Rejected : Track.Track_Channel := Track.Open (Parent, Track_Config);
         begin
            Track.Close (Rejected);
            raise Program_Error with
              "pinned Squall unexpectedly attached a Track channel";
         end;
      exception
         when Error : AMS.MEL.Provider_Error =>
            if Ada.Exceptions.Exception_Message (Error) /= "attachChannel returned null" then
               raise Program_Error with
                 "unexpected Squall Track failure: "
                 & Ada.Exceptions.Exception_Message (Error);
            end if;
            Ada.Text_IO.Put_Line
              ("Track: pinned Squall unsupported as expected "
               & "(Provider_Error: attachChannel returned null)");
      end;

      declare
         Event : constant Image_Metadata.Metadata_Event :=
           Image_Metadata.Receive (Image_Metadata_Stream, 5_000);
         Value : constant Image_Metadata.Bad_Pixel_List :=
           Image_Metadata.Bad_Pixel_List_Value (Event);
      begin
         if Image_Metadata.Reported_Size (Value) /= 0
           or else Image_Metadata.Reported_Count (Value) /= 0
           or else Image_Metadata.Pixel_Count (Value) /= 0
         then
            raise Program_Error with "unexpected Squall initial BadPixelList";
         end if;
         Ada.Text_IO.Put_Line
           ("Image BadPixelList: reported-size=0 reported-count=0 pixels=0");
      end;

      --  NavigationReport submission is intentionally valid while the Image
      --  stream is logically Attached, before Start.
      declare
         Submitted_Time : constant Long_Long_Integer := -123_456_789_012;
         Report         : constant AMS.MEL.IR.Image.Navigation_Report :=
           (System_Time_NS               => Submitted_Time,
            State                        => AMS.MEL.IR.Image.Blended,
            Latitude_Rad                 => 0.523598,
            Longitude_Rad                => -1.308997,
            Altitude_M                   => 987.5,
            Attitude                     => (Roll => 0.1, Pitch => 0.2, Yaw => 0.3),
            Attitude_Rate                =>
              (Value          => (Roll => 0.11, Pitch => -0.22, Yaw => 0.33),
               System_Time_NS => -424_242),
            Speed                        => (North => 10.0, East => -20.0, Down => 30.0),
            Acceleration                 => (North => -1.0, East => 2.0, Down => -3.0),
            Wander_Angle_Rad             => 0.05,
            Magnetic_Heading             => 12.5,
            Altitude_MSL                 => 1000.25,
            Position_Velocity_Covariance =>
              (Position_Position_Pn_Pn => 1.5, Position_Position_Pn_Pe => 2.5,
               Position_Position_Pn_Pd => 3.5, Position_Position_Pe_Pe => 4.5,
               Position_Position_Pe_Pd => 5.5, Position_Position_Pd_Pd => 6.5,
               Position_Velocity_Pn_Vn => 7.5, Position_Velocity_Pn_Ve => 8.5,
               Position_Velocity_Pn_Vd => 9.5, Position_Velocity_Pe_Ve => 10.5,
               Position_Velocity_Pe_Vd => 11.5, Position_Velocity_Pd_Vd => 12.5,
               Velocity_Velocity_Vn_Vn => 13.5, Velocity_Velocity_Vn_Ve => 14.5,
               Velocity_Velocity_Vn_Vd => 15.5, Velocity_Velocity_Ve_Ve => 16.5,
               Velocity_Velocity_Ve_Vd => 17.5, Velocity_Velocity_Vd_Vd => 18.5));
         Request : AMS.MEL.IR.Image.Navigation_Request :=
           AMS.MEL.IR.Image.Submit_Navigation_Report (Stream, Report);
         Callback_Response : Image_Metadata.Navigation_Response;
         Have_Callback     : Boolean := False;
      begin
         for Index in 1 .. 32 loop
            exit when Have_Callback;
            declare
               Event : constant Image_Metadata.Metadata_Event :=
                 Image_Metadata.Receive (Image_Metadata_Stream, Timeout_MS);
            begin
               if Image_Metadata.Kind (Event) = Image_Metadata.Navigation_Response_Event then
                  Callback_Response := Image_Metadata.Navigation_Response_Value (Event);
                  Have_Callback := True;
               end if;
            end;
         end loop;
         if not Have_Callback
           or else Callback_Response.System_Time_NS /= Submitted_Time
           or else Callback_Response.Command_ID /= 0
           or else Callback_Response.Request_ID /= 0
         then
            raise Program_Error with "invalid Squall NavigationReportResp callback";
         end if;
         Ada.Text_IO.Put_Line
           ("Image NavigationReportResp callback: system_time="
              & Callback_Response.System_Time_NS'Image & " command_id=0 request_id=0");
         declare
            Result : constant AMS.MEL.IR.Image.Navigation_Result :=
              AMS.MEL.IR.Image.Wait (Request, Timeout_MS);
            Response : constant AMS.MEL.IR.Image.Navigation_Response :=
              AMS.MEL.IR.Image.Response (Result);
         begin
            if AMS.MEL.IR.Image.Status (Result) /= AMS.MEL.IR.Image.Success
              or else Response.System_Time_NS /= Callback_Response.System_Time_NS
              or else Response.Command_ID /= Callback_Response.Command_ID
              or else Response.Request_ID /= Callback_Response.Request_ID
            then
               raise Program_Error with "Squall NavigationReport future mismatched callback";
            end if;
            Ada.Text_IO.Put_Line ("Image NavigationReport future: matches callback response");
         end;
         declare
            Cached : constant AMS.MEL.IR.Image.Navigation_Result :=
              AMS.MEL.IR.Image.Wait (Request, 0);
         begin
            if AMS.MEL.IR.Image.Status (Cached) /= AMS.MEL.IR.Image.Success
              or else AMS.MEL.IR.Image.Response (Cached).System_Time_NS /= Submitted_Time
            then
               raise Program_Error with "Squall NavigationReport cached Wait(0) mismatched";
            end if;
            Ada.Text_IO.Put_Line ("Image NavigationReport cached Wait(0): matches");
         end;
         AMS.MEL.IR.Image.Close (Request);
      end;

      --  Data destination and buffers are ready before Operate is submitted.
      AMS.MEL.IR.Start (Stream);
      declare
         Health_Channel : Health.Health_Channel := Health.Open (Parent, Health_Config);
         Health_Stream : Health_Metadata.Metadata_Stream :=
            Health_Metadata.Open (Health_Channel, 32);
         Channel : C2.Control_Channel := C2.Open (Parent, C2_Config);
         Metadata_Stream : Metadata.Metadata_Stream := Metadata.Open (Channel, 32);
      begin
         --  Open succeeds only after native registration of all six required
         --  Health callbacks; the capability enum does not advertise two of them.
         Ada.Text_IO.Put_Line ("Health callbacks: all six registrations succeeded");
         Health.Enable (Health_Channel);
         declare
            Capability : constant Channel_Value.Channel_Capability :=
              Health.Capabilities (Health_Channel);
         begin
            if Channel_Value.Channel_Type_Count (Capability) /= 1
              or else Channel_Value.Channel_Type_At (Capability, 1) /=
                Channel_Value.Health_And_Status
              or else not Channel_Value.Has_Metadata_Capability
                (Capability, Channel_Value.MFA_Status)
              or else not Channel_Value.Has_Metadata_Capability
                (Capability, Channel_Value.BIT_Status)
              or else not Channel_Value.Has_Metadata_Capability
                (Capability, Channel_Value.Subsystem_Status_Resp)
              or else not Channel_Value.Has_Metadata_Capability
                (Capability, Channel_Value.MFA_Status_Detailed)
              or else not Channel_Value.Has_Metadata_Capability
                (Capability, Channel_Value.Channel_Comms_Test_Rep)
            then
               raise Program_Error with "unexpected Squall Health capability";
            end if;
            Ada.Text_IO.Put_Line
              ("Health capability: type=HEALTH_AND_STATUS required metadata present");
         end;
         declare
            Configuration : constant Metadata.Metadata_Event :=
              Metadata.Receive (Metadata_Stream, 5_000);
         begin
            if Metadata.Kind (Configuration) /= Metadata.BIT_Configuration_Event
              or else Metadata.BIT_Type_Count (Configuration) /= 0
            then
               raise Program_Error with
                 "Squall BIT_Configuration was not well-formed and empty";
            end if;
            Ada.Text_IO.Put_Line ("BIT_Configuration: empty default");
         end;
         Expect_Empty_BIT_Status (Metadata_Stream);
         --  Inherited Channel services are intentionally exercised while the
         --  C2 channel is merely attached, before C2.Enable.
         Metadata.Enable_Comms_Test_Events (Metadata_Stream);
         declare
            Capability : constant Channel_Value.Channel_Capability :=
              Common.Capabilities (Channel);
         begin
            if Channel_Value.Channel_Type_Count (Capability) /= 1 or else
              Channel_Value.Channel_Type_At (Capability, 1) /=
                Channel_Value.Command_And_Control or else
              Channel_Value.Metadata_Capability_Count (Capability) /= 4 or else
              not Channel_Value.Has_Metadata_Capability
                (Capability, Channel_Value.BIT_Configuration) or else
              not Channel_Value.Has_Metadata_Capability
                (Capability, Channel_Value.Command_Status) or else
              not Channel_Value.Has_Metadata_Capability
                (Capability, Channel_Value.BIT_Status) or else
              not Channel_Value.Has_Metadata_Capability
                (Capability, Channel_Value.Channel_Comms_Test_Rep) or else
              Channel_Value.Task_Schedule_Depth (Capability) /= 0 or else
              Channel_Value.ODC_Available (Capability) or else
              Channel_Value.NUC_Available (Capability)
            then raise Program_Error with "unexpected Squall C2 capability"; end if;
            Ada.Text_IO.Put_Line
              ("C2 capability: type=COMMAND_AND_CONTROL metadata=4 schedule=0 odc=FALSE nuc=FALSE");
         end;
         declare
            Request : C2.Return_Request := Common.Send_Keep_Alive (Channel);
         begin
            if C2.Value (C2.Wait (Request, 5_000)) /= C2.Return_Success or else
              C2.Value (C2.Wait (Request, 0)) /= C2.Return_Success
            then raise Program_Error with "Squall KeepAlive failed"; end if;
            Ada.Text_IO.Put_Line ("KeepAlive result: SUCCESS"); C2.Close (Request);
         end;
         declare
            Request : Common.Comms_Request := Common.Submit_Comms_Test
              (Channel, 7, 16#0040_1901#, 16#8040_1902#);
            Result : constant Common.Comms_Result := Common.Wait (Request, 5_000);
            Reply : constant Channel_Value.Comms_Test_Report := Common.Report (Result);
            Event : constant Metadata.Metadata_Event := Metadata.Receive (Metadata_Stream, 5_000);
            Callback : constant Channel_Value.Comms_Test_Report := Metadata.Comms_Test (Event);
         begin
            if Common.Status (Result) /= C2.Success or else
              Reply.Command_ID /= 16#0040_1901# or else Reply.Request_ID /= 16#8040_1902# or else
              Metadata.Kind (Event) /= Metadata.Channel_Comms_Test_Event or else
              Callback.Command_ID /= Reply.Command_ID or else Callback.Request_ID /= Reply.Request_ID or else
              Common.Report (Common.Wait (Request, 0)) /= Reply
            then raise Program_Error with "Squall CommsTest mismatch"; end if;
            Ada.Text_IO.Put_Line
              ("CommsTest response/callback: command=" & Reply.Command_ID'Image &
               " request=" & Reply.Request_ID'Image);
            Common.Close (Request);
         end;
         C2.Enable (Channel);
         declare
            Request : C2.Mode_Request := C2.Submit_Mode
              (Channel, 16#0040_1701#, C2.Standby, C2.Unused);
            Result : constant C2.Mode_Result := C2.Wait (Request, 5_000);
         begin
            if C2.Status (Result) /= C2.Success or else C2.Mode (Result) /= C2.Unused then
               raise Program_Error with "Squall did not accept Standby/Unused";
            end if;
            Ada.Text_IO.Put_Line ("general mode result: STANDBY/UNUSED");
            Expect_Command_Status
              (Metadata_Stream, 16#0040_1701#, Metadata.Accepted,
               Metadata.Not_Set, Description_Empty => True);
            C2.Close (Request);
         end;
         declare
            Seen_MFA, Seen_BIT, Seen_Subsystem, Seen_Discrete, Seen_Detailed :
              Boolean := False;
         begin
            for Attempt in 1 .. 20 loop
               declare
                  Event : constant Health_Metadata.Metadata_Event :=
                    Health_Metadata.Receive (Health_Stream, 5_000);
               begin
                  case Health_Metadata.Kind (Event) is
                     when Health_Metadata.MFA_Status_Event =>
                        declare
                           Value : constant Status.MFA_Status :=
                             Health_Metadata.MFA_Status_Value (Event);
                           About : constant Status.About := Status.About_Value (Value);
                        begin
                           if Status.State (Value) = Status.Standby
                             and then Status.State_Description (Value) = "Squall OK"
                             and then Status.Mode_Description (Value) = "IR backend status available"
                             and then Status.Transition_Status (Value) = Status.Not_Transitioning
                             and then Status.Model (About) = "Squall IR MFA"
                             and then Status.Software_Version (About) = "unknown"
                           then
                              Ada.Text_IO.Put_Line
                                ("Health MFA_Status: state=STANDBY description=Squall OK mode=IR backend status available " &
                                 "transition=NOT_TRANSITIONING model=Squall IR MFA software=unknown serial=" &
                                 Status.Serial_Number (About) & " bootloader=" &
                                 Status.Bootloader_Software_Version (About) & " hardware=" &
                                 Status.Hardware_Version (About));
                              Seen_MFA := True;
                           else
                              Ada.Text_IO.Put_Line ("Health MFA_Status: ignored pre-Standby polling cycle");
                           end if;
                        end;
                     when Health_Metadata.BIT_Status_Event =>
                        declare Value : constant Status.BIT_Status := Health_Metadata.BIT_Status_Value (Event); begin
                           if Status.Active_BIT_Count (Value) /= 0 or else Status.Completed_BIT_Count (Value) /= 0
                             or else Status.Fault_Count (Value) /= 0
                           then raise Program_Error with "unexpected Squall Health BIT_Status"; end if;
                           Ada.Text_IO.Put_Line ("Health BIT_Status: active= 0 completed= 0 fault= 0");
                        end;
                        Seen_BIT := True;
                     when Health_Metadata.Subsystem_Status_Event =>
                        declare Value : constant Health_Metadata.Subsystem_Status := Health_Metadata.Subsystem_Status_Value (Event); begin
                           if Health_Metadata.Subsystem_ID (Value) /= 0 or else Health_Metadata.Criticality (Value) /= 0
                             or else Health_Metadata.Status_Sequence_Number (Value) /= 0
                             or else Health_Metadata.Failure (Value) /= Health_Metadata.Available
                             or else Health_Metadata.Reported_Subsystem_Count (Value) /= 0
                             or else Health_Metadata.Subsystem_Count (Value) /= 0
                             or else Health_Metadata.Reported_CSCI_Count (Value) /= 0
                             or else Health_Metadata.CSCI_Count (Value) /= 0
                           then raise Program_Error with "unexpected Squall SubsystemStatusResp"; end if;
                           Ada.Text_IO.Put_Line ("Health SubsystemStatusResp: id= 0 criticality= 0 sequence= 0 failure=AVAILABLE subsystems= 0 csci= 0");
                        end;
                        Seen_Subsystem := True;
                     when Health_Metadata.Discrete_Status_Event =>
                        if Status.Pair_Count (Health_Metadata.Discrete_Status_Value (Event)) /= 0
                        then raise Program_Error with "unexpected Squall DiscreteStatus"; end if;
                        Ada.Text_IO.Put_Line ("Health DiscreteStatus: count= 0"); Seen_Discrete := True;
                     when Health_Metadata.MFA_Status_Detailed_Event =>
                        if Status.Pair_Count (Health_Metadata.MFA_Status_Detailed_Value (Event)) /= 0
                        then raise Program_Error with "unexpected Squall MFA_StatusDetailed"; end if;
                        Ada.Text_IO.Put_Line ("Health MFA_StatusDetailed: count= 0"); Seen_Detailed := True;
                     when Health_Metadata.Security_Audit_Event => null;
                  end case;
               end;
               exit when Seen_MFA and Seen_BIT and Seen_Subsystem and Seen_Discrete and Seen_Detailed;
            end loop;
            if not (Seen_MFA and Seen_BIT and Seen_Subsystem and Seen_Discrete and Seen_Detailed)
            then raise Program_Error with "Squall required Health events were incomplete"; end if;
            Ada.Text_IO.Put_Line ("Health events: MFA_STATUS BIT_STATUS SUBSYSTEM_STATUS DISCRETE_STATUS MFA_STATUS_DETAILED");
         end;
         declare
            Request : C2.Mode_Request := C2.Submit_Mode
              (Channel, 16#0040_1702#, C2.Operate, C2.Scan_Volume_Sched);
            Result : constant C2.Mode_Result := C2.Wait (Request, 5_000);
         begin
            if C2.Status (Result) /= C2.Rejected or else
              C2.Rejection_Code (Result) /= C2.Invalid_Parameters
            then
               raise Program_Error with "Squall did not reject unsupported scan mode";
            end if;
            Ada.Text_IO.Put_Line ("scan mode result: REJECTED/INVALID_PARAMETERS");
            Expect_Command_Status
              (Metadata_Stream, 16#0040_1702#, Metadata.Rejected,
               Metadata.Invalid_Input_Parameter, Description_Empty => False);
            C2.Close (Request);
         end;
         declare
            Empty_Request : C2.Return_Request := C2.Submit_Config_Set
              (Channel, 16#0040_1703#);
            Payload_Request : C2.Return_Request := C2.Submit_Config_Set
              (Channel, 16#0040_1704#, Config => "task-017");
         begin
            if C2.Value (C2.Wait (Empty_Request, 5_000)) /= C2.Return_Success or else
              C2.Value (C2.Wait (Payload_Request, 5_000)) /= C2.Fail
            then
               raise Program_Error with "Squall ConfigSet behavior changed";
            end if;
            Ada.Text_IO.Put_Line ("ConfigSet empty result: SUCCESS");
            Expect_Command_Status
              (Metadata_Stream, 16#0040_1703#, Metadata.Accepted,
               Metadata.Not_Set, Description_Empty => True);
            Ada.Text_IO.Put_Line ("ConfigSet payload result: FAIL");
            Expect_Command_Status
              (Metadata_Stream, 16#0040_1704#, Metadata.Rejected,
               Metadata.Invalid_Input_Parameter, Description_Empty => False);
            C2.Close (Empty_Request); C2.Close (Payload_Request);
         end;
         declare
            Payload_Request : C2.Return_Request := C2.Submit_BIT_Initiate
              (Channel, [16#8000_0001#], 16#0040_1705#);
         begin
            if C2.Value (C2.Wait (Payload_Request, 5_000)) /= C2.Fail then
               raise Program_Error with "Squall did not return Fail for BIT payload";
            end if;
            Ada.Text_IO.Put_Line ("BIT payload result: FAIL");
            Expect_Command_Status
              (Metadata_Stream, 16#0040_1705#, Metadata.Rejected,
               Metadata.Invalid_Input_Parameter, Description_Empty => False);
            C2.Close (Payload_Request);
         end;
         declare
            BIT_Request : C2.Return_Request :=
              C2.Submit_BIT_No_Op (Channel, 16#0040_1402#);
            BIT_Result : constant C2.Return_Result := C2.Wait (BIT_Request, 5_000);
         begin
            if C2.Status (BIT_Result) /= C2.Success
              or else C2.Value (BIT_Result) /= C2.Return_Success
            then
               raise Program_Error with "Squall did not return BIT Success";
            end if;
            Ada.Text_IO.Put_Line ("BIT result: SUCCESS");
            Expect_Command_Status
              (Metadata_Stream, 16#0040_1402#, Metadata.Accepted,
               Metadata.Not_Set, Description_Empty => True);
            Expect_Empty_BIT_Status (Metadata_Stream);
            C2.Close (BIT_Request);
         end;
         declare
            Request : C2.Mode_Request := C2.Submit_Mode
              (Channel, 16#0040_0402#, C2.Operate, C2.Task_Sched);
            Result : constant C2.Mode_Result := C2.Wait (Request, 5_000);
         begin
            if C2.Status (Result) /= C2.Success
              or else C2.Mode (Result) /= C2.Task_Sched
            then
               raise Program_Error with "Squall did not return Task_Sched";
            end if;
            Ada.Text_IO.Put_Line ("C2 result: TASK_SCHED");
            Expect_Command_Status
              (Metadata_Stream, 16#0040_0402#, Metadata.Accepted,
               Metadata.Not_Set, Description_Empty => True);

            --  Exercise retained child/request ownership after parent close.
            AMS.MEL.Close (Parent);
            declare
               Cached : constant C2.Mode_Result := C2.Wait (Request, 0);
            begin
               if C2.Status (Cached) /= C2.Success
                 or else C2.Mode (Cached) /= C2.Task_Sched
               then
                  raise Program_Error with "completed request was not retained";
               end if;
            end;

            declare
               Previous_ID : Interfaces.Unsigned_32 := 0;
            begin
               for Index in 1 .. Frame_Count loop
                  declare
                     Frame : constant AMS.MEL.IR.Frame :=
                       AMS.MEL.IR.Receive (Stream, Timeout_MS);
                     Expected : constant Long_Long_Integer :=
                       Long_Long_Integer (Frame.Width) * Long_Long_Integer (Frame.Height);
                  begin
                     if Frame.Width = 0 or else Frame.Height = 0
                       or else Frame.Bits_Per_Pixel /= 8
                       or else Frame.Number_Of_Bands /= 1
                       or else Expected /= Long_Long_Integer (Frame.Pixel_Count)
                       or else (Index > 1 and then Frame.Frame_ID <= Previous_ID)
                     then
                        raise Program_Error with "invalid Squall Mono8 frame";
                     end if;
                     Previous_ID := Frame.Frame_ID;
                     Ada.Text_IO.Put_Line
                       ("frame" & Index'Image & ": id=" & Frame.Frame_ID'Image &
                        " geometry=" & Frame.Width'Image & "x" & Frame.Height'Image &
                        " bytes=" & Frame.Pixel_Count'Image & " checksum=" &
                        Interfaces.Unsigned_64'Image (Checksum (Frame.Pixels)));
                  end;
               end loop;
            end;

            declare
               Full : constant AMS.MEL.IR.Image.Full_Frame :=
                 AMS.MEL.IR.Image.Receive (Stream, Timeout_MS);
               Sensor : constant AMS.MEL.IR.Image.Contributing_Sensor :=
                 AMS.MEL.IR.Image.Contributing_Sensor_Value (Full);
            begin
               if AMS.MEL.IR.Image.Width (Full) /= 320
                 or else AMS.MEL.IR.Image.Height (Full) /= 200
                 or else AMS.MEL.IR.Image.Bits_Per_Pixel (Full) /= 8
                 or else AMS.MEL.IR.Image.Number_Of_Bands (Full) /= 1
                 or else AMS.MEL.IR.Image.Pixel_Format (Full) /= Channel_Value.Mono
                 or else AMS.MEL.IR.Image.Image_Type (Full) /= AMS.MEL.IR.Image.Staring
                 or else AMS.MEL.IR.Image.Image_Flip (Full) /= AMS.MEL.IR.No_Flip
                 or else AMS.MEL.IR.Image.Integration_Time_NS (Full) /= 0
                 or else Sensor.Sensor_ID /= 0
                 or else AMS.MEL.IR.Offset_X_M (Sensor.Location) /= 0.0
                 or else AMS.MEL.IR.Offset_Y_M (Sensor.Location) /= 0.0
                 or else AMS.MEL.IR.Offset_Z_M (Sensor.Location) /= 0.0
                 or else AMS.MEL.IR.Key (Sensor.Location) /= "task-004-station"
                 or else AMS.MEL.IR.System_Name (Sensor.Location) /= "ams-mel-squall-integration"
                 or else AMS.MEL.IR.Image.Image_Flag_Count (Full) /= 0
                 or else AMS.MEL.IR.Image.Sensor_Inertial_State_Count (Full) /= 0
                 or else AMS.MEL.IR.Image.Sensor_Nav_State_Count (Full) /= 0
                 or else AMS.MEL.IR.Image.Band_Index (Full) /= 0
                 or else AMS.MEL.IR.Image.Pixel_Count (Full) /= 64_000
               then raise Program_Error with "invalid Squall full FrameHeader"; end if;
               Ada.Text_IO.Put_Line ("full FrameHeader: 320x200 Mono8 sparse metadata");
            end;

            --  Task 030A: the same real 320x200 Mono8 frames through the
            --  high-rate borrowed path. The borrowed view must report the
            --  expected geometry and an identical checksum to an explicit
            --  owned copy of the same lease, proving the borrow observed the
            --  real payload rather than an empty or stale view.
            declare
               Lease : AMS.MEL.IR.Image.Frame_Lease :=
                 AMS.MEL.IR.Image.Acquire_Frame (Stream, Timeout_MS);
               Borrowed : Interfaces.Unsigned_64 := 0;
               Length   : Natural := 0;
               procedure Observe (Pixels : AMS.MEL.IR.Pixel_Array) is
               begin
                  Length := Pixels'Length;
                  Borrowed := Checksum (Pixels);
               end Observe;
            begin
               if not AMS.MEL.IR.Image.Is_Open (Lease)
                 or else AMS.MEL.IR.Image.Width (Lease) /= 320
                 or else AMS.MEL.IR.Image.Height (Lease) /= 200
                 or else AMS.MEL.IR.Image.Bits_Per_Pixel (Lease) /= 8
                 or else AMS.MEL.IR.Image.Number_Of_Bands (Lease) /= 1
                 or else AMS.MEL.IR.Image.Pixel_Format (Lease) /= Channel_Value.Mono
                 or else AMS.MEL.IR.Image.Pixel_Count (Lease) /= 64_000
               then
                  raise Program_Error with "invalid Squall zero-copy frame lease";
               end if;
               AMS.MEL.IR.Image.With_Pixels (Lease, Observe'Access);
               declare
                  Owned : constant AMS.MEL.IR.Pixel_Array :=
                    AMS.MEL.IR.Image.Copy_Pixels (Lease);
               begin
                  if Length /= 64_000
                    or else Owned'Length /= 64_000
                    or else Borrowed /= Checksum (Owned)
                  then
                     raise Program_Error with "Squall borrowed and owned payloads disagree";
                  end if;
                  --  The lease keeps the payload valid after an explicit close
                  --  only through the owned copy, never through the borrow.
                  AMS.MEL.IR.Image.Close (Lease);
                  if AMS.MEL.IR.Image.Is_Open (Lease)
                    or else Checksum (Owned) /= Borrowed
                  then
                     raise Program_Error with "Squall lease close semantics failed";
                  end if;
               end;
               Ada.Text_IO.Put_Line
                 ("zero-copy lease: 320x200 Mono8 bytes=" & Length'Image & " checksum=" &
                  Interfaces.Unsigned_64'Image (Borrowed) &
                  " (borrowed view == owned copy; no native-snapshot payload copy)");
            end;

            declare
               Have_Report : Boolean := False;
               Have_Euler  : Boolean := False;
               Report      : Image_Metadata.Line_Of_Sight_Report;
            begin
               for Index in 1 .. 32 loop
                  exit when Have_Euler;
                  declare
                     Event : constant Image_Metadata.Metadata_Event :=
                       Image_Metadata.Receive (Image_Metadata_Stream, Timeout_MS);
                  begin
                     case Image_Metadata.Kind (Event) is
                        when Image_Metadata.Bad_Pixel_List_Event => null;
                        when Image_Metadata.Line_Of_Sight_Report_Event =>
                           Report := Image_Metadata.Line_Of_Sight_Report_Value (Event);
                           Have_Report := True;
                        when Image_Metadata.Line_Of_Sight_Euler_Event =>
                           declare
                              Euler : constant Image_Metadata.Line_Of_Sight_Euler :=
                                Image_Metadata.Line_Of_Sight_Euler_Value (Event);
                           begin
                              if not Have_Report
                                or else Report.Pointing_Angle.Azimuth_Rad /= Euler.Attitude.Yaw
                                or else Report.Pointing_Angle.Elevation_Rad /= Euler.Attitude.Pitch
                                or else Euler.Attitude.Roll /= 0.0
                                or else Report.Pointing_Angle_Rates /= (0.0, 0.0)
                                or else Euler.Attitude_Rates /= (0.0, 0.0, 0.0)
                                or else Report.At_Speed or else Report.In_Tolerance
                                or else Report.Platform_Attitude /= (0.0, 0.0, 0.0)
                                or else Report.Validity_Flag_Bitfield /= 0
                                or else Report.Image_Rotation_Rad /= 0.0
                              then
                                 raise Program_Error with "invalid Squall LOS consistency";
                              end if;
                              Have_Euler := True;
                           end;
                        when Image_Metadata.Navigation_Response_Event =>
                           --  The Task 027B NavigationReport request already
                           --  drained its own response before Start; no
                           --  further event of this kind is expected here.
                           null;
                     end case;
                  end;
               end loop;
               if not Have_Report or else not Have_Euler then
                  raise Program_Error with "Squall did not provide valid LOS metadata";
               end if;
               Ada.Text_IO.Put_Line ("Image LOS: Report/Euler consistency validated");
            end;
            Image_Metadata.Close (Image_Metadata_Stream);

            declare
               Counts : constant AMS.MEL.IR.Stream_Counters :=
                 AMS.MEL.IR.Counters (Stream);
            begin
               Ada.Text_IO.Put_Line
                 ("counters: received=" & Counts.Frames_Received'Image &
                  " dropped=" & Counts.Frames_Dropped_Queue_Full'Image &
                  " malformed=" & Counts.Malformed_Or_Unsupported'Image);
               if Counts.Frames_Received < AMS.MEL.IR.Counter (Frame_Count)
                 or else Counts.Malformed_Or_Unsupported /= 0
               then
                  raise Program_Error with "invalid Squall stream counters";
               end if;
            end;
            C2.Close (Request);
         end;
         declare
            Counts : constant Metadata.Metadata_Counters :=
              Metadata.Counters (Metadata_Stream);
         begin
            Ada.Text_IO.Put_Line
              ("metadata counters: received=" & Counts.Events_Received'Image &
               " dropped=" & Counts.Events_Dropped_Queue_Full'Image &
               " malformed=" & Counts.Malformed_Or_Unsupported'Image);
            if Counts.Events_Received < 11
              or else Counts.Events_Dropped_Queue_Full /= 0
              or else Counts.Malformed_Or_Unsupported /= 0
            then
               raise Program_Error with "invalid Squall metadata counters";
            end if;
         end;
         C2.Close (Channel);
         begin
            declare
               Unexpected : constant Metadata.Metadata_Event :=
                 Metadata.Receive (Metadata_Stream, 0);
            begin
               raise Program_Error with
                 "metadata remained after close: " &
                 Metadata.Metadata_Kind'Image (Metadata.Kind (Unexpected));
            end;
         exception
            when AMS.MEL.IR.Stream_Stopped => null;
         end;
         Metadata.Close (Metadata_Stream);
         declare
            Counts : constant Health_Metadata.Metadata_Counters :=
              Health_Metadata.Counters (Health_Stream);
         begin
            Ada.Text_IO.Put_Line
              ("Health metadata counters: received=" & Counts.Events_Received'Image &
               " dropped=" & Counts.Events_Dropped_Queue_Full'Image &
               " malformed=" & Counts.Malformed_Or_Unsupported'Image);
            if Counts.Events_Received < 5
              or else Counts.Events_Dropped_Queue_Full /= 0
              or else Counts.Malformed_Or_Unsupported /= 0
            then
               raise Program_Error with "invalid Squall Health metadata counters";
            end if;
         end;
         Health.Close (Health_Channel);
         begin
            loop
               declare
                  Retained : constant Health_Metadata.Metadata_Event :=
                    Health_Metadata.Receive (Health_Stream, 0);
                  pragma Unreferenced (Retained);
               begin
                  --  Deep-copied events queued before parent-first close remain
                  --  valid; drain them before requiring terminal stop.
                  null;
               end;
            end loop;
         exception
            when AMS.MEL.IR.Stream_Stopped => null;
         end;
         Health_Metadata.Close (Health_Stream);
      end;
      AMS.MEL.IR.Close (Stream);
      Ada.Text_IO.Put_Line ("PASS: real Squall IR Ada integration");
   end;
exception
   when Error : others =>
      Ada.Text_IO.Put_Line
        (Ada.Text_IO.Standard_Error,
         "FAIL: real Squall IR Ada integration: " &
         Ada.Exceptions.Exception_Information (Error));
      Ada.Command_Line.Set_Exit_Status (Ada.Command_Line.Failure);
end AMS_MEL_Squall_IR;
