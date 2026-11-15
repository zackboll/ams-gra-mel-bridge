with Ada.Command_Line;
with Ada.Exceptions;
with Ada.Text_IO;
with Interfaces;
with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.IR.Buffer;
with AMS.MEL.IR.C2;
with AMS.MEL.IR.C2.Metadata;
with AMS.MEL.IR.Channel;
with AMS.MEL.IR.Channel.Metadata;
with AMS.MEL.IR.Discovery;
with AMS.MEL.IR.Health_Status;
with AMS.MEL.IR.Health_Status.Metadata;
with AMS.MEL.IR.Image;
with AMS.MEL.IR.Image.Checkouts;
with AMS.MEL.IR.Image.Metadata;
with AMS.MEL.IR.Stacked_Image;
with AMS_MEL_IR_Minimal_Checks;

procedure AMS_MEL_IR_Minimal_Consumer is
   package IR renames AMS.MEL.IR;
   package V renames IR.Channel;
   package C renames IR.C2;
   package CM renames C.Metadata;
   package M renames V.Metadata;
   package HM renames IR.Health_Status.Metadata;
   package IM renames IR.Image.Metadata;
   package B renames IR.Buffer;
   package X renames IR.Image.Checkouts;
   package Q renames AMS_MEL_IR_Minimal_Checks;
   use Q;
   use type Interfaces.Unsigned_32;
   use type Interfaces.Unsigned_64;
   use type IR.UUID;
   use type IR.Pixel_Array;
   use type IR.Counter;
   use type V.Channel_Type;
   use type V.Outcome;
   use type V.Command_Return;
   use type CM.Metadata_Kind;
   use type C.Outcome;
   use type C.Command_Return;
   use type C.Error_Code;
   use type IR.Image.Navigation_Outcome;
   use type IR.Image.Navigation_Error_Code;
   use type B.Image_Address_Kind;
   use type B.Flag_Array;

   Types    : constant array (Positive range 1 .. 4) of V.Channel_Type :=
     [V.Command_And_Control, V.Health_And_Status, V.IRST_Image, V.Stacked_Image];
   Pattern  : constant IR.Pixel_Array := [1 .. 16 => 16#5A#];
   Observed : Evidence;

   procedure Run (Path : String) is
      Parent      : AMS.MEL.Session := AMS.MEL.Open (Path, "minimal-consumer");
      Version     : constant AMS.MEL.Provider_Version := AMS.MEL.Query_Provider_Version (Parent);
      Discovered  : constant IR.Discovery.Capability_Vectors.Vector :=
        IR.Discovery.Capabilities (Parent);
      Selected    : array (Types'Range) of V.Channel_Capability;
      Occurrences : Counts := [others => 0];
      Distinct    : Boolean := True;

      procedure Layout (Value : V.Channel_Capability) is
      begin
         Require
           (V.Width (Value) = 0
            and V.Height (Value) = 0
            and V.Bit_Depth (Value) = 0
            and V.Row_Pitch (Value) = 0
            and V.Buffer_Size (Value) = 0
            and V.Image_Size (Value) = 0
            and V.Number_Of_Bands (Value) = 0,
            "seven unconfigured layout scalars must remain zero");
      end Layout;

      function Platform (I : Positive) return IR.UCI_ID is
         ID : constant IR.UCI_ID := V.Platform_ID (Selected (I));
      begin
         return IR.Create_UCI_ID (IR.UUID_Value (ID), IR.Descriptive_Label (ID));
      end Platform;

      function Location (I : Positive) return IR.Component_Location is
         L : constant IR.Component_Location := V.Sensor_Location (Selected (I));
      begin
         return
           IR.Create_Component_Location
             (IR.Offset_X_M (L),
              IR.Offset_Y_M (L),
              IR.Offset_Z_M (L),
              IR.Key (L),
              IR.System_Name (L));
      end Location;
   begin
      Identity
        (AMS.MEL.Vendor (Version),
         AMS.MEL.Description (Version),
         Long_Long_Integer (AMS.MEL.API_Version (Version)),
         Long_Long_Integer (AMS.MEL.Library_Version (Version)));
      for Capability of Discovered loop
         Require (V.Channel_Type_Count (Capability) = 1, "ambiguous capability categories");
         for I in Types'Range loop
            if V.Channel_Type_At (Capability, 1) = Types (I) then
               Occurrences (I) := Occurrences (I) + 1;
               Selected (I) := Capability;
            end if;
         end loop;
      end loop;
      for I in Types'Range loop
         for J in Types'First .. I - 1 loop
            Distinct :=
              Distinct
              and IR.UUID_Value (V.Channel_ID (Selected (I)))
                  /= IR.UUID_Value (V.Channel_ID (Selected (J)));
         end loop;
      end loop;
      Selection (Occurrences, Natural (Discovered.Length), Distinct);
      for I in Types'Range loop
         Layout (Selected (I));
         Ada.Text_IO.Put_Line
           ("discovered "
            & Types (I)'Image
            & " label="
            & IR.Descriptive_Label (V.Channel_ID (Selected (I)))
            & " platform="
            & IR.Descriptive_Label (Platform (I))
            & " location="
            & IR.Key (Location (I))
            & " system="
            & IR.System_Name (Location (I))
            & " offsets="
            & IR.Offset_X_M (Location (I))'Image
            & IR.Offset_Y_M (Location (I))'Image
            & IR.Offset_Z_M (Location (I))'Image);
      end loop;
      declare
         C2         : C.Control_Channel :=
           C.Open
             (Parent, C.Create_Config (V.Channel_ID (Selected (1)), Platform (1), Location (1)));
         Health     : IR.Health_Status.Health_Channel :=
           IR.Health_Status.Open
             (Parent,
              IR.Health_Status.Create_Config
                (V.Channel_ID (Selected (2)), Platform (2), Location (2)));
         Image      : IR.Image_Stream :=
           IR.Open_Image_Control
             (Parent,
              IR.Create_Image_Control_Config
                (V.Channel_ID (Selected (3)), Platform (3), Location (3)));
         Stacked    : IR.Stacked_Image.Stacked_Channel :=
           IR.Stacked_Image.Open
             (Parent,
              IR.Stacked_Image.Create_Config
                (V.Channel_ID (Selected (4)), Platform (4), Location (4)));
         Views      : array (Types'Range) of V.View :=
           [C.As_Channel (C2),
            IR.Health_Status.As_Channel (Health),
            IR.Image.As_Channel (Image),
            IR.Stacked_Image.As_Channel (Stacked)];
         C_Metadata : CM.Metadata_Stream := CM.Open (C2, 16);
         H_Metadata : HM.Metadata_Stream := HM.Open (Health, 16);
         I_Metadata : IM.Metadata_Stream := IM.Open (Image, 16);
         Common     : array (Positive range 2 .. 4) of M.Comms_Metadata_Stream :=
           [M.Open (Views (2), 16), M.Open (Views (3), 16), M.Open (Views (4), 16)];
         Receiver   : X.Checkout_Receiver := X.Open (Image, 16, 16);
         Buffers    : array (Types'Range) of B.Host_Buffer :=
           [B.Open (Parent, 16, 1),
            B.Open (Parent, 16, 2),
            B.Open (Parent, 16, 3),
            B.Open (Parent, 16, 4)];
         Registered : array (Types'Range) of Boolean := [others => False];

         procedure Storage (I : Positive) is
         begin
            Require
              (B.Read_Storage (Buffers (I), 0, 16) = Pattern, "Buffer storage pattern" & I'Image);
         end Storage;

         procedure Quiet is
            H : constant HM.Metadata_Counters := HM.Counters (H_Metadata);
            I : constant IM.Metadata_Counters := IM.Counters (I_Metadata);
            F : constant IR.Stream_Counters := IR.Counters (Image);
            D : constant X.Diagnostic_Counters := X.Counters (Receiver);
         begin
            Require
              (H.Events_Received = 0
               and H.Events_Dropped_Queue_Full = 0
               and H.Malformed_Or_Unsupported = 0
               and I.Events_Received = 0
               and I.Events_Dropped_Queue_Full = 0
               and I.Malformed_Or_Unsupported = 0
               and F.Frames_Received = 0
               and F.Frames_Dropped_Queue_Full = 0
               and F.Malformed_Or_Unsupported = 0
               and D.Callbacks_Received = 0
               and D.Checkouts_Accepted = 0
               and D.Queue_Overflow = 0
               and D.Callbacks_Rejected = 0
               and D.Releases_Uncertain = 0,
               "unexpected image or sensor telemetry counters");
            begin
               declare
                  Event : constant HM.Metadata_Event := HM.Receive (H_Metadata, 0);
               begin
                  Require (False, "unexpected Health telemetry " & HM.Kind (Event)'Image);
               end;
            exception
               when IR.Timeout_Error =>
                  null;
            end;
            begin
               declare
                  Event : constant IM.Metadata_Event := IM.Receive (I_Metadata, 0);
               begin
                  Require (False, "unexpected Image telemetry " & IM.Kind (Event)'Image);
               end;
            exception
               when IR.Timeout_Error =>
                  null;
            end;
            begin
               declare
                  Lease : X.Checkout_Lease := X.Receive (Receiver, 0);
               begin
                  X.Close (Lease);
                  Require (False, "unexpected opaque image checkout");
               end;
            exception
               when IR.Timeout_Error =>
                  null;
            end;
         end Quiet;

         procedure Cleanup (Best_Effort : Boolean) is
            Failed : Boolean := False;
         begin
            --  Each step is attempted independently on failure, without consuming
            --  a typed owner before its exact registration has been removed.
            for Step in 1 .. 24 loop
               begin
                  case Step is
                     when 1 .. 4   =>
                        V.Disable (Views (Step));

                     when 5 .. 8   =>
                        declare
                           I : constant Positive := Step - 4;
                        begin
                           if Registered (I) then
                              V.Unregister_Buffer (Views (I), Buffers (I));
                              Registered (I) := False;
                           end if;
                           Storage (I);
                           B.Close (Buffers (I));
                        end;

                     when 9        =>
                        CM.Close (C_Metadata);

                     when 10       =>
                        HM.Close (H_Metadata);

                     when 11       =>
                        IM.Close (I_Metadata);

                     when 12 .. 14 =>
                        M.Close (Common (Step - 10));

                     when 15       =>
                        X.Close (Receiver);

                     when 16       =>
                        C.Close (C2);

                     when 17       =>
                        IR.Health_Status.Close (Health);

                     when 18       =>
                        IR.Close (Image);

                     when 19       =>
                        IR.Stacked_Image.Close (Stacked);

                     when 20 .. 23 =>
                        V.Close (Views (Step - 19));

                     when 24       =>
                        AMS.MEL.Close (Parent);
                  end case;
                  Ada.Text_IO.Put_Line ("cleanup step" & Step'Image & " OK");
               exception
                  when E : others =>
                     Failed := True;
                     Ada.Text_IO.Put_Line
                       (Ada.Text_IO.Standard_Error,
                        "cleanup step" & Step'Image & ": " & Ada.Exceptions.Exception_Message (E));
                     if not Best_Effort then
                        raise;
                     end if;
               end;
            end loop;
            if not Best_Effort then
               Observed.Cleanup_Complete := not Failed;
            end if;
         end Cleanup;
      begin
         for I in Types'Range loop
            declare
               Actual : constant V.Channel_Capability := V.Capabilities (Views (I));
               A      : constant IR.UCI_ID := V.Channel_ID (Actual);
               D      : constant IR.UCI_ID := V.Channel_ID (Selected (I));
            begin
               Require
                 (V.Channel_Type_Count (Actual) = 1
                  and then V.Channel_Type_At (Actual, 1) = Types (I)
                  and then IR.UUID_Value (A) = IR.UUID_Value (D)
                  and then IR.Descriptive_Label (A) = IR.Descriptive_Label (D),
                  "attached capability identity" & I'Image);
               Layout (Actual);
            end;
         end loop;
         CM.Enable_Comms_Test_Events (C_Metadata);
         Observed.Registrations := 17;
         Ada.Text_IO.Put_Line
           ("registered 17 required metadata callbacks (4 C2, 7 Health, 5 Image, 1 Stacked)");
         for I in Types'Range loop
            B.Write_Storage (Buffers (I), 0, Pattern);
            B.Set_Flags (Buffers (I), []);
            B.Add_Flag (Buffers (I), B.Host);
            declare
               P : constant B.Buffer_Properties := B.Properties (Buffers (I));
            begin
               Require
                 (P.Capacity_Bytes = 16
                  and P.Provider_Size = 16
                  and P.Initialized_Context = Long_Long_Integer (I)
                  and P.Provider_Context = Long_Long_Integer (I)
                  and P.Buffer_Address_Matches
                  and P.Image_Address = B.Null_Image
                  and B.Flags (Buffers (I)) = [B.Host],
                  "Buffer properties/flags" & I'Image);
            end;
            Storage (I);
            V.Register_Buffer (Views (I), Buffers (I));
            Registered (I) := True;
            Ada.Text_IO.Put_Line ("Buffer" & I'Image & " verified and registered");
         end loop;
         C.Enable (C2);
         IR.Health_Status.Enable (Health);
         IR.Start (Image);
         IR.Stacked_Image.Enable (Stacked);
         for I in Types'Range loop
            declare
               Request : V.Return_Request := V.Send_Keep_Alive (Views (I));
               Result  : constant V.Return_Result := V.Wait (Request, 5_000);
            begin
               Require
                 (V.Status (Result) = V.Success and then V.Value (Result) = V.Return_Success,
                  "KeepAlive" & I'Image);
               V.Close (Request);
               Observed.Keep_Alives := Observed.Keep_Alives + 1;
               Ada.Text_IO.Put_Line ("KeepAlive" & I'Image & " Return_Success");
            end;
         end loop;
         for I in Types'Range loop
            declare
               Selector : constant V.Comms_Channel_ID := V.Channel_Type'Pos (Types (I));
               Expected : constant V.Comms_Test_Report :=
                 (V.Command_ID (99 + I), V.Comms_Request_ID (199 + I));
            begin
               for Routed in Boolean loop
                  declare
                     Request : V.Comms_Request :=
                       V.Submit_Comms_Test
                         (Views ((if Routed then 1 else I)),
                          Selector,
                          Expected.Command_ID,
                          Expected.Request_ID);
                     Result  : constant V.Comms_Result := V.Wait (Request, 5_000);
                  begin
                     Require (V.Status (Result) = V.Success, "CommsTest future" & I'Image);
                     Correlation (V.Report (Result), Expected, "CommsTest future" & I'Image);
                     V.Close (Request);
                     Observed.Futures := Observed.Futures + 1;
                     Ada.Text_IO.Put_Line
                       ("future destination"
                        & I'Image
                        & " routed="
                        & Routed'Image
                        & " command="
                        & Expected.Command_ID'Image
                        & " request="
                        & Expected.Request_ID'Image);
                  end;
               end loop;
               for Report_Number in 1 .. 2 loop
                  declare
                     Actual : V.Comms_Test_Report;
                  begin
                     if I = 1 then
                        declare
                           Event : constant CM.Metadata_Event := CM.Receive (C_Metadata, 5_000);
                        begin
                           Require
                             (CM.Kind (Event) = CM.Channel_Comms_Test_Event,
                              "unexpected C2 sensor telemetry");
                           Actual := CM.Comms_Test (Event);
                        end;
                     else
                        Actual := M.Receive (Common (I), 5_000);
                     end if;
                     Correlation (Actual, Expected, "callback queue" & I'Image);
                     Observed.Callbacks (I) := Observed.Callbacks (I) + 1;
                     Ada.Text_IO.Put_Line
                       ("callback queue"
                        & I'Image
                        & " report"
                        & Report_Number'Image
                        & " command="
                        & Actual.Command_ID'Image
                        & " request="
                        & Actual.Request_ID'Image);
                  end;
               end loop;
            end;
         end loop;
         declare
            Request : C.Return_Request := C.Submit_BIT_No_Op (C2, 300);
            Result  : constant C.Return_Result := C.Wait (Request, 5_000);
         begin
            Observed.BIT_Expected :=
              C.Status (Result) = C.Success and then C.Value (Result) = C.Not_Implemented;
            C.Close (Request);
            Require (Observed.BIT_Expected, "BIT_Command expected NotImplemented");
            Ada.Text_IO.Put_Line ("BIT_Command 300: NotImplemented (not BIT execution)");
         end;
         declare
            Request : C.Return_Request := C.Submit_Config_Set (C2, 301, 0, "");
            Result  : constant C.Return_Result := C.Wait (Request, 5_000);
         begin
            Observed.Config_Expected :=
              C.Status (Result) = C.Success and then C.Value (Result) = C.Not_Implemented;
            C.Close (Request);
            Require (Observed.Config_Expected, "ConfigSetCommand expected NotImplemented");
            Ada.Text_IO.Put_Line ("ConfigSetCommand 301: NotImplemented");
         end;
         declare
            Request : C.Mode_Request := C.Submit_Mode (C2, 302, C.Standby, C.Unused);
            Result  : constant C.Mode_Result := C.Wait (Request, 5_000);
         begin
            Observed.Mode_Expected :=
              C.Status (Result) = C.Rejected
              and then C.Rejection_Code (Result) = C.Invalid_State
              and then C.Description (Result)'Length > 0;
            C.Close (Request);
            Require (Observed.Mode_Expected, "ModeCmd expected InvalidState with description");
            Ada.Text_IO.Put_Line ("ModeCmd 302: InvalidState: " & C.Description (Result));
         end;
         declare
            Neutral : constant IR.Image.Navigation_Report :=
              (System_Time_NS               => 0,
               State                        => IR.Image.Not_Set,
               Latitude_Rad
               | Longitude_Rad
               | Altitude_M
               | Wander_Angle_Rad
               | Magnetic_Heading
               | Altitude_MSL               => 0.0,
               Attitude                     => (others => 0.0),
               Attitude_Rate                => ((others => 0.0), 0),
               Speed | Acceleration         => (others => 0.0),
               Position_Velocity_Covariance => (others => 0.0));
            Request : IR.Image.Navigation_Request :=
              IR.Image.Submit_Navigation_Report (Image, Neutral);
            Result  : constant IR.Image.Navigation_Result := IR.Image.Wait (Request, 5_000);
         begin
            Observed.Navigation_Expected :=
              IR.Image.Status (Result) = IR.Image.Rejected
              and then IR.Image.Rejection_Code (Result) = IR.Image.Invalid_State
              and then IR.Image.Description (Result)'Length > 0;
            IR.Image.Close (Request);
            Require
              (Observed.Navigation_Expected,
               "NavigationReport expected InvalidState with description");
            Ada.Text_IO.Put_Line
              ("NavigationReport NotSet: InvalidState: " & IR.Image.Description (Result));
         end;
         --  Poll after all commands, so unsolicited BIT/status is also rejected.
         for I in Types'Range loop
            begin
               if I = 1 then
                  declare
                     Event : constant CM.Metadata_Event := CM.Receive (C_Metadata, 0);
                  begin
                     Require (False, "unexpected extra C2 report " & CM.Kind (Event)'Image);
                  end;
               else
                  declare
                     Event : constant V.Comms_Test_Report := M.Receive (Common (I), 0);
                  begin
                     Require (False, "unexpected extra common report " & Event.Command_ID'Image);
                  end;
               end if;
            exception
               when IR.Timeout_Error =>
                  null;
            end;
            if I = 1 then
               declare
                  Stats : constant CM.Metadata_Counters := CM.Counters (C_Metadata);
               begin
                  Require
                    (Stats.Events_Received = 2
                     and Stats.Events_Dropped_Queue_Full = 0
                     and Stats.Malformed_Or_Unsupported = 0,
                     "C2 callback counters");
               end;
            else
               declare
                  Stats : constant M.Metadata_Counters := M.Counters (Common (I));
               begin
                  Require
                    (Stats.Events_Received = 2
                     and Stats.Events_Dropped_Queue_Full = 0
                     and Stats.Malformed_Or_Unsupported = 0,
                     "common callback counters" & I'Image);
               end;
            end if;
         end loop;
         Quiet;
         Observed.Quiet := True;
         Cleanup (False);
      exception
         when E : others =>
            Ada.Text_IO.Put_Line
              (Ada.Text_IO.Standard_Error, "[FAIL] " & Ada.Exceptions.Exception_Information (E));
            Cleanup (True);
            raise;
      end;
      Complete (Observed);
   exception
      when others =>
         --  Partial elaboration uses limited-owner non-raising finalization.
         --  Session Close does not cancel work or guarantee immediate dlclose.
         begin
            AMS.MEL.Close (Parent);
         exception
            when others =>
               null;
         end;
         raise;
   end Run;
begin
   if Ada.Command_Line.Argument_Count = 1 and then Ada.Command_Line.Argument (1) = "--help" then
      Ada.Text_IO.Put_Line
        ("Usage: ams_mel_ir_minimal_consumer /absolute/path/to/libminimal_ir_mel_provider.so");
   elsif Ada.Command_Line.Argument_Count /= 1 then
      Ada.Text_IO.Put_Line
        (Ada.Text_IO.Standard_Error, "Expected exactly one provider-library path; use --help");
      Ada.Command_Line.Set_Exit_Status (2);
   else
      Run (Ada.Command_Line.Argument (1));
      Ada.Text_IO.Put_Line
        ("PASS: four channels, 17 registrations, 4 KeepAlive, 8 futures, 8 callbacks; cleanup verified");
   end if;
exception
   when E : others =>
      Ada.Text_IO.Put_Line
        (Ada.Text_IO.Standard_Error, "[FAIL] " & Ada.Exceptions.Exception_Information (E));
      Ada.Command_Line.Set_Exit_Status (1);
end AMS_MEL_IR_Minimal_Consumer;
