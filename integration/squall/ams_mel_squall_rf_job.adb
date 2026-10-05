with Ada.Command_Line;
with Ada.Text_IO;
with AMS.MEL.RF.Admin;
with AMS.MEL.RF.C2;
with AMS.MEL.RF.C2.Interval_Status;
with AMS.MEL.Status;
with Interfaces;

procedure AMS_MEL_Squall_RF_Job is
   package Admin renames AMS.MEL.RF.Admin;
   package C2 renames AMS.MEL.RF.C2;
   package Intervals_Status renames C2.Interval_Status;
   package Status renames AMS.MEL.Status;
   use type C2.Request_Outcome;
   use type C2.Job_Status;
   use type C2.Cancel_Error;
   use type Interfaces.Unsigned_32;
   use type Interfaces.Integer_64;
   procedure Verify (Object : C2.Job; ID : Interfaces.Unsigned_32) is
      Start     : constant Interfaces.Integer_64 :=
        C2.Actual_Start_Seconds (Object);
      Fraction  : constant Interfaces.Integer_64 :=
        C2.Actual_Start_Femtoseconds (Object);
      Duration  : constant Interfaces.Integer_64 :=
        C2.Total_Job_Duration_Femtoseconds (Object);
      Lookahead : constant Interfaces.Integer_64 :=
        C2.Lookahead_Femtoseconds (Object);
      Details   : constant Interfaces.Unsigned_32 :=
        C2.Job_Details_ID (Object);
   begin
      if C2.VA_Instance_ID (Object) /= 0
        or else C2.VA_Definition_ID (Object) /= 0
        or else C2.Job_Request_ID (Object) /= ID
        or else C2.RX_Stream_ID_Count (Object) < 1
        or else C2.RX_Stream_ID_At (Object, 1) /= 0
        or else Fraction < 0
        or else Fraction >= 1_000_000_000_000_000
      then
         raise Program_Error with "Squall Job snapshot mismatch";
      end if;
      Ada.Text_IO.Put_Line
        ("Squall Job snapshot: request"
         & ID'Image
         & " details"
         & Details'Image
         & " start"
         & Start'Image
         & "/"
         & Fraction'Image
         & " duration"
         & Duration'Image
         & " lookahead"
         & Lookahead'Image);
   end Verify;
   function Job_Config (ID : Interfaces.Unsigned_32) return C2.Job_Config is
      First : C2.RX_Element_Group_Config := C2.Create_RX_Element_Group ("0", 0.625);
      Second : C2.RX_Element_Group_Config := C2.Create_RX_Element_Group ("0", 0.5);
   begin
      --  Pinned rf_environment advertises only its active 915 MHz point range.
      C2.Append_Expected_Center_Frequency (First, 915_000_000.0, 915_000_000.0);
      C2.Append_Expected_Center_Frequency (Second, 915_000_000.0, 915_000_000.0);
      --  Pinned Squall stores these calls but does not interpret their geometry.
      C2.Append_Expected_Pointing (First, C2.Create_Face_Relative_Pointing (1.5, -0.5));
      C2.Append_Expected_Pointing
        (First, C2.Create_ECEF_Pointing (1.25, -2.5, 3.75, -4.5, 5.625, -6.75,
                                       C2.Create_UTC_Time (-7, 123456789012345)));
      C2.Append_Expected_Pointing (Second, C2.Create_Platform_Relative_Pointing (-0.75, 0.25));
      C2.Append_Expected_Pointing (Second, C2.Create_Baseline_Relative_Pointing (-2.25));
      return Config : C2.Job_Config := C2.Create_Job_Config (ID, 1, First) do
         C2.Set_Estimated_Stab_Point
           (Config, C2.Create_LLA_Pointing (-8.125, 9.25, -999.5, -21.25, 22.5, -23.75,
                                          C2.Create_UTC_Time (42, 999999999999999)));
         C2.Append_RX_Element_Group (Config, Second);
         C2.Set_Min_Start_Time (Config, C2.Create_UTC_Time (-5, 123456789012345));
         C2.Set_Max_Complete_Time (Config, C2.Create_UTC_Time (42, 999999999999999));
         C2.Set_Duration_Femtoseconds (Config, -123456789012345);
         C2.Set_Capability_ID (Config, [0, 16#FF#, 16#80#, 0, 7]);
         C2.Set_Activity_ID (Config, [16#DE#, 16#AD#, 0, 16#BE#, 16#EF#]);
         C2.Append_Instance_Selection (Config, 0);
         C2.Append_Instance_Selection (Config, 0);
         C2.Set_TX_Power_Mode_IDs (Config, [7, 7, 16#8000_0000#, Interfaces.Unsigned_32'Last]);
         C2.Set_Lookahead_Femtoseconds (Config, Interfaces.Integer_64'Last);
      end return;
   end Job_Config;
begin
   if Ada.Command_Line.Argument_Count /= 2 then
      raise Program_Error with "usage: ams_mel_squall_rf_job PROVIDER PROFILE";
   end if;
   declare
      Provider : constant String := Ada.Command_Line.Argument (1);
      Profile  : constant String := Ada.Command_Line.Argument (2);
      Control  : Admin.Admin_MEL := Admin.Open (Provider, Profile);
   begin
      if not Admin.Command_State (Control, Status.Standby)
        or else not Admin.Command_State (Control, Status.Operate_Rx_Only)
      then
         raise Program_Error with "Squall Admin rejected RX state";
      end if;
      declare
         Parent     : C2.C2_MEL := C2.Open (Provider, Profile);
         VA_Config  : constant C2.Virtual_Aperture_Config :=
           C2.Create_Virtual_Aperture_Config (0, 1, "");
         VA_Request : C2.Virtual_Aperture_Request :=
           C2.Submit_Virtual_Aperture (Parent, VA_Config);
      begin
         if C2.Outcome (C2.Wait (VA_Request, 10_000)) /= C2.Created then
            raise Program_Error with "Squall VA rejected";
         end if;
         declare
            VA    : C2.Virtual_Aperture := C2.Claim (VA_Request);
         begin
            C2.Close (VA_Request);
            for Sequence in 0 .. 1 loop
               declare
                  ID      : constant Interfaces.Unsigned_32 :=
                    16#1234_5678# + Interfaces.Unsigned_32 (Sequence);
                  Config  : constant C2.Job_Config :=
                    Job_Config (ID);
                  Request : C2.Job_Request := C2.Submit_Job (VA, Config);
               begin
                  declare
                     Result : constant C2.Job_Result := C2.Wait (Request, 10_000);
                  begin
                     if C2.Outcome (Result) /= C2.Created then
                        raise Program_Error with "Squall Job rejected: " & C2.Description (Result);
                     end if;
                  end;
                  declare
                     Object : C2.Job := C2.Claim (Request);
                     Stream : Intervals_Status.Stream := Intervals_Status.Open (Object, 2, 8, 64);
                  begin
                     C2.Close (Request);
                     Verify (Object, ID);
                     if Sequence = 1 then
                        declare
                           Interval  : C2.RX_Job_Interval_Config :=
                             C2.Create_RX_Job_Interval
                               (1,
                                2_000_000_000,
                                Job_Details_ID => C2.Job_Details_ID (Object));
                           Intervals : C2.RX_Job_Interval_List;
                        begin
                           C2.Set_Interval_Status_Enable (Interval, C2.Always);
                           C2.Append_RX_Event
                             (Interval,
                              C2.Create_RX_Receive_Event
                                (1,
                                 "0",
                                 0,
                                 1_000_000_000,
                                 100_000_000.0,
                                 1_000_000.0,
                                 Max_Extension_Femtoseconds => 500_000_000));
                           C2.Append_Job_Interval (Intervals, Interval);
                           C2.Add_RX_Job_Intervals (Object, Intervals);
                           C2.Flush_Job (Object);
                        end;
                        begin
                           declare
                              Unexpected : constant Intervals_Status.Status_Event := Intervals_Status.Receive_Event (Stream, 0);
                           begin
                              raise Program_Error with "Squall unexpectedly delivered interval status" & Intervals_Status.Interval_ID (Unexpected)'Image;
                           end;
                        exception
                           when C2.Timeout_Error => null;
                        end;
                        C2.Finalize_Job (Object);
                        begin
                           declare
                              Pending : constant C2.Job_Status :=
                                C2.Wait_Job_Status (Object, 0);
                           begin
                              raise Program_Error
                                with
                                  "Squall Job unexpectedly completed"
                                  & Pending'Image;
                           end;
                        exception
                           when C2.Timeout_Error =>
                              null;
                        end;
                        C2.Close (VA);
                        C2.Close (Parent);
                        C2.Extend_Job_Event (Object, 1, 1, 123_456_789);
                        --  Pinned extension and registration are no-ops.
                        begin
                           declare
                              Unexpected : constant Intervals_Status.Status_Event :=
                                Intervals_Status.Receive_Event (Stream, 0);
                           begin
                              raise Program_Error with "unexpected extension status" &
                                Intervals_Status.Interval_ID (Unexpected)'Image;
                           end;
                        exception
                           when C2.Timeout_Error => null;
                        end;
                        C2.Cancel_Remaining_Job_Intervals (Object);
                        declare
                           Result : constant C2.Cancel_Result :=
                             C2.Cancel_Job (Object);
                        begin
                           if not C2.Cancelled (Result)
                             or else C2.Error_Code (Result) not in C2.None
                           then
                              raise Program_Error
                                with "Squall Job cancellation failed";
                           end if;
                        end;
                        if C2.Wait_Job_Status (Object, 10_000) /= C2.Complete
                        then
                           raise Program_Error
                             with "Squall Job did not complete";
                        end if;
                        Verify (Object, ID);
                     end if;
                     C2.Close (Object);
                     begin
                        declare
                           Unexpected : constant Intervals_Status.Status_Event := Intervals_Status.Receive_Event (Stream, 0);
                        begin
                           raise Program_Error with "status stream not stopped" & Intervals_Status.Interval_ID (Unexpected)'Image;
                        end;
                     exception
                        when Intervals_Status.Stream_Stopped => null;
                     end;
                     Intervals_Status.Close (Stream);
                  end;
               end;
            end loop;
         end;
      end;
      Admin.Close (Control);
   end;
   Ada.Text_IO.Put_Line
      ("PASS: safe Ada Squall RF two Jobs and parent-first lifecycle; real Squall " &
       "ElementGroupCommand accepted bridge-issued addExpectedPointingAngle calls " &
       "and requestJob accepted the completed request (not geometry/scheduling evidence)");
end AMS_MEL_Squall_RF_Job;
