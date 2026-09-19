with Ada.Text_IO;
with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.IR.C2;

package body AMS_MEL_IR_C2_Tests is
   package C2 renames AMS.MEL.IR.C2;
   use type C2.Error_Code;
   use type C2.MFA_Mode;
   use type C2.MFA_State;
   use type C2.System_Time_Nanoseconds;
   use type C2.Command_Return;
   use type C2.Outcome;

   Zero_UUID   : constant AMS.MEL.IR.UUID := [others => 0];
   Channel_ID  : constant AMS.MEL.IR.UCI_ID :=
     AMS.MEL.IR.Create_UCI_ID (Zero_UUID, "Ada IR C2 channel");
   Platform_ID : constant AMS.MEL.IR.UCI_ID := AMS.MEL.IR.Create_UCI_ID (Zero_UUID, "Ada platform");
   Location    : constant AMS.MEL.IR.Component_Location :=
     AMS.MEL.IR.Create_Component_Location (1.25, -2.5, 3.75, "station-1", "mock-aircraft");
   Config      : constant C2.Control_Config := C2.Create_Config (Channel_ID, Platform_ID, Location);

   function Long_Rejection return String is
      Result : String (1 .. 613) := [others => 'x'];
   begin
      Result (511) := Character'Val (16#E2#);
      Result (512) := Character'Val (16#82#);
      Result (513) := Character'Val (16#AC#);
      Result (514 .. Result'Last) := [others => 'y'];
      return Result;
   end Long_Rejection;

   procedure Test_Success (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "c2-command-id");
      Channel : C2.Control_Channel := C2.Open (Parent, Config);
   begin
      C2.Enable (Channel);
      declare
         Request : C2.Mode_Request := C2.Submit_Operate (Channel, 16#89AB_CDEF#);
         Result  : constant C2.Mode_Result := C2.Wait (Request, 1_000);
      begin
         if C2.Status (Result) /= C2.Success or else C2.Mode (Result) /= C2.Task_Sched then
            raise Program_Error with "Ada C2 success conversion failed";
         end if;
         C2.Close (Request);
         C2.Close (Request);
         if C2.Is_Open (Request) then
            raise Program_Error with "Ada C2 request close did not clear owner";
         end if;
      end;
      C2.Close (Channel);
      C2.Close (Channel);
      AMS.MEL.Close (Parent);
   end Test_Success;

   procedure Test_Timeout_Lifetime (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "c2-delayed");
      Channel : C2.Control_Channel := C2.Open (Parent, Config);
   begin
      C2.Enable (Channel);
      declare
         Request : C2.Mode_Request := C2.Submit_Operate (Channel, 7);
      begin
         begin
            declare
               Unexpected : constant C2.Mode_Result := C2.Wait (Request, 0);
            begin
               raise Program_Error
                 with "Ada C2 timeout returned " & C2.Outcome'Image (C2.Status (Unexpected));
            end;
         exception
            when AMS.MEL.IR.Timeout_Error =>
               null;
         end;
         AMS.MEL.Close (Parent);
         C2.Close (Channel);
         declare
            Result : constant C2.Mode_Result := C2.Wait (Request, 1_000);
         begin
            if C2.Mode (Result) /= C2.Task_Sched then
               raise Program_Error with "Ada C2 delayed result failed";
            end if;
         end;
         C2.Close (Request);
      end;
   end Test_Timeout_Lifetime;

   procedure Test_Rejection (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "c2-reject");
      Channel : C2.Control_Channel := C2.Open (Parent, Config);
   begin
      C2.Enable (Channel);
      declare
         Request : C2.Mode_Request := C2.Submit_Operate (Channel);
         Result  : constant C2.Mode_Result := C2.Wait (Request, 1_000);
      begin
         if C2.Status (Result) /= C2.Rejected
           or else C2.Rejection_Code (Result) /= C2.Invalid_Parameters
           or else C2.Description (Result) /= "invalid task schedule"
         then
            raise Program_Error with "Ada C2 rejection conversion failed";
         end if;
         C2.Close (Request);
      end;
      C2.Close (Channel);
      AMS.MEL.Close (Parent);
   end Test_Rejection;

   procedure Test_Long_Rejection (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "c2-reject-long");
      Channel : C2.Control_Channel := C2.Open (Parent, Config);
   begin
      C2.Enable (Channel);
      declare
         Request : C2.Mode_Request := C2.Submit_Operate (Channel);
         Result  : constant C2.Mode_Result := C2.Wait (Request, 1_000);
      begin
         if C2.Status (Result) /= C2.Rejected
           or else C2.Rejection_Code (Result) /= C2.Invalid_Parameters
           or else C2.Description (Result) /= Long_Rejection
         then
            raise Program_Error with "Ada C2 complete long rejection description was not preserved";
         end if;
         C2.Close (Request);
      end;
      C2.Close (Channel);
      AMS.MEL.Close (Parent);
   end Test_Long_Rejection;

   procedure Test_Pending_Finalization (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "c2-lifetime");
      Channel : C2.Control_Channel := C2.Open (Parent, Config);
   begin
      C2.Enable (Channel);
      declare
         Request : constant C2.Mode_Request := C2.Submit_Operate (Channel);
      begin
         if not C2.Is_Open (Request) then
            raise Program_Error with "Ada C2 request was not published";
         end if;
      end;
      C2.Close (Channel);
      AMS.MEL.Close (Parent);
   end Test_Pending_Finalization;

   procedure Test_BIT (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "bit-command-id");
      Channel : C2.Control_Channel := C2.Open (Parent, Config);
   begin
      C2.Enable (Channel);
      declare
         Request : C2.Return_Request := C2.Submit_BIT_No_Op (Channel, 16#89AB_CDEF#);
         Result  : constant C2.Return_Result := C2.Wait (Request, 1_000);
      begin
         if C2.Status (Result) /= C2.Success or else C2.Value (Result) /= C2.Return_Success then
            raise Program_Error with "Ada BIT success conversion failed";
         end if;
         C2.Close (Request);
         C2.Close (Request);
      end;
      C2.Close (Channel);
      AMS.MEL.Close (Parent);
   end Test_BIT;

   procedure Test_BIT_Fail (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "bit-fail");
      Channel : C2.Control_Channel := C2.Open (Parent, Config);
   begin
      C2.Enable (Channel);
      declare
         Request : C2.Return_Request := C2.Submit_BIT_No_Op (Channel);
         Result  : constant C2.Return_Result := C2.Wait (Request, 1_000);
      begin
         if C2.Status (Result) /= C2.Success or else C2.Value (Result) /= C2.Fail then
            raise Program_Error with "Ada BIT Return::Fail was not preserved";
         end if;
         C2.Close (Request);
      end;
      C2.Close (Channel);
      AMS.MEL.Close (Parent);
   end Test_BIT_Fail;

   procedure Test_BIT_Timeout (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "bit-delayed");
      Channel : C2.Control_Channel := C2.Open (Parent, Config);
   begin
      C2.Enable (Channel);
      declare
         Request : C2.Return_Request := C2.Submit_BIT_No_Op (Channel);
      begin
         begin
            declare
               Unexpected : constant C2.Return_Result := C2.Wait (Request, 0);
            begin
               raise Program_Error with C2.Outcome'Image (C2.Status (Unexpected));
            end;
         exception
            when AMS.MEL.IR.Timeout_Error =>
               null;
         end;
         AMS.MEL.Close (Parent);
         C2.Close (Channel);
         if C2.Value (C2.Wait (Request, 1_000)) /= C2.Return_Success then
            raise Program_Error with "Ada delayed BIT failed";
         end if;
         C2.Close (Request);
      end;
   end Test_BIT_Timeout;

   procedure Test_BIT_Rejection (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "bit-reject-long");
      Channel : C2.Control_Channel := C2.Open (Parent, Config);
   begin
      C2.Enable (Channel);
      declare
         Request : C2.Return_Request := C2.Submit_BIT_No_Op (Channel);
         Result  : constant C2.Return_Result := C2.Wait (Request, 1_000);
      begin
         if C2.Status (Result) /= C2.Rejected
           or else C2.Rejection_Code (Result) /= C2.Invalid_Parameters
           or else C2.Description (Result) /= Long_Rejection
         then
            raise Program_Error with "Ada BIT rejection conversion failed";
         end if;
         C2.Close (Request);
      end;
      C2.Close (Channel);
      AMS.MEL.Close (Parent);
   end Test_BIT_Rejection;

   procedure Test_BIT_Pending_Finalization (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "bit-lifetime");
      Channel : C2.Control_Channel := C2.Open (Parent, Config);
   begin
      C2.Enable (Channel);
      declare
         Request : constant C2.Return_Request := C2.Submit_BIT_No_Op (Channel);
      begin
         if not C2.Is_Open (Request) then
            raise Program_Error with "Ada BIT request was not published";
         end if;
      end;
      C2.Close (Channel);
      AMS.MEL.Close (Parent);
   end Test_BIT_Pending_Finalization;

   procedure Test_General_Mode (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "mode-full");
      Channel : C2.Control_Channel := C2.Open (Parent, Config);
      Scan    : constant C2.Scan_Parameters :=
        (Elevation_Defined_With_Range_And_Altitude => True,
         Center_Azimuth_Rad                        => 0.25,
         Center_Elevation_Rad                      => -0.5,
         Center_Frame_Reference_EL                 => C2.Aircraft,
         Center_Frame_Reference_AZ                 => C2.Inertial,
         Scan_Width_Rad                            => 1.25,
         Scan_Height_Rad                           => 0.75,
         Continuous_Scan                           => 1,
         Returning                                 => 2,
         Agile_Scan                                => 3,
         Scan_ID                                   => 16#89AB_CDEF#,
         Scan_Rate_Rad_Per_Second                  => -0.125,
         Preferred_Revisit_Interval_Seconds        => 2.5,
         Required_Revisit_Interval_Seconds         => 3.5,
         Max_Range_Of_Interest_M                   => 123_456,
         Min_Range_Of_Interest_M                   => 42,
         Elevation_Scan_Center_Altitude_M          => 7_000,
         Elevation_Scan_Center_Range_M             => 9_000,
         Degradation                               => C2.Revisit_Degradation);
   begin
      C2.Enable (Channel);
      declare
         Request : C2.Mode_Request :=
           C2.Submit_Mode (Channel, 16#89AB_CDEF#, C2.Operate, C2.Scan_Volume_Sched, Scan);
      begin
         if C2.Mode (C2.Wait (Request, 1_000)) /= C2.Scan_Volume_Sched then
            raise Program_Error with "complete Ada ScanParam was not preserved";
         end if;
         C2.Close (Request);
      end;
      C2.Close (Channel);
      AMS.MEL.Close (Parent);

      for State in C2.MFA_State'(C2.Initialization) .. C2.MFA_State'(C2.Maintenance) loop
         if State in C2.Standby | C2.Initialization | C2.Maintenance | C2.Operate then
            declare
               P    : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "mode-general");
               Ch   : C2.Control_Channel := C2.Open (P, Config);
               Mode : constant C2.MFA_Mode :=
                 (if State = C2.Operate then C2.Scan_Bar_Sched else C2.Unused);
            begin
               C2.Enable (Ch);
               declare
                  R : C2.Mode_Request := C2.Submit_Mode (Ch, 16#FEDC_BA98#, State, Mode);
               begin
                  if C2.Mode (C2.Wait (R, 1_000)) /= Mode then
                     raise Program_Error with "general mode result mismatch";
                  end if;
                  C2.Close (R);
               end;
               C2.Close (Ch);
               AMS.MEL.Close (P);
            end;
         end if;
      end loop;
      declare
         P  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "mode-general");
         Ch : C2.Control_Channel := C2.Open (P, Config);
      begin
         C2.Enable (Ch);
         declare
            R : C2.Mode_Request := C2.Submit_Mode (Ch, 16#8000_0000#, C2.Operate, C2.Task_Sched);
         begin
            if C2.Mode (C2.Wait (R, 1_000)) /= C2.Task_Sched then
               raise Program_Error with "general TaskSched result mismatch";
            end if;
            C2.Close (R);
         end;
         C2.Close (Ch);
         AMS.MEL.Close (P);
      end;
   end Test_General_Mode;

   procedure Test_BIT_Choices (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "bit-ada");
      Channel : C2.Control_Channel := C2.Open (Parent, Config);
      Faults  : C2.Fault_Code_Vectors.Vector;
   begin
      C2.Enable (Channel);
      Faults.Append ("fault-alpha");
      Faults.Append
        ("fault-" & Character'Val (16#E2#) & Character'Val (16#82#) & Character'Val (16#AC#));
      declare
         Initiate     : C2.Return_Request :=
           C2.Submit_BIT_Initiate (Channel, [1, 16#8000_0001#, 16#FFFF_FFFF#], 11);
         Initiate_One : C2.Return_Request := C2.Submit_BIT_Initiate (Channel, [42], 14);
         Cancel       : C2.Return_Request := C2.Submit_BIT_Cancel (Channel, [7, 9], 12);
         Cancel_One   : C2.Return_Request := C2.Submit_BIT_Cancel (Channel, [7], 15);
         Clear        : C2.Return_Request := C2.Submit_BIT_Clear_Faults (Channel, Faults, 13);
      begin
         if C2.Value (C2.Wait (Initiate, 1_000)) /= C2.Return_Success
           or else C2.Value (C2.Wait (Initiate_One, 1_000)) /= C2.Return_Success
           or else C2.Value (C2.Wait (Cancel, 1_000)) /= C2.Return_Success
           or else C2.Value (C2.Wait (Cancel_One, 1_000)) /= C2.Return_Success
           or else C2.Value (C2.Wait (Clear, 1_000)) /= C2.Return_Success
         then
            raise Program_Error with "Ada BIT choice failed";
         end if;
         C2.Close (Initiate);
         C2.Close (Initiate_One);
         C2.Close (Cancel);
         C2.Close (Cancel_One);
         C2.Close (Clear);
      end;
      C2.Close (Channel);
      AMS.MEL.Close (Parent);
   end Test_BIT_Choices;

   procedure Test_Config_Set (Provider_Path : String) is
      procedure Check (Scenario : String; Expected : C2.Command_Return) is
         Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, Scenario);
         Channel : C2.Control_Channel := C2.Open (Parent, Config);
      begin
         C2.Enable (Channel);
         declare
            Request : C2.Return_Request :=
              C2.Submit_Config_Set
                (Channel,
                 16#FEDC_BA98#,
                 C2.System_Time_Nanoseconds'(-1_234_567_890_123),
                 "configuration-"
                 & Character'Val (16#E2#)
                 & Character'Val (16#82#)
                 & Character'Val (16#AC#));
         begin
            if C2.Value (C2.Wait (Request, 1_000)) /= Expected then
               raise Program_Error with "Ada ConfigSet result mismatch";
            end if;
            C2.Close (Request);
         end;
         C2.Close (Channel);
         AMS.MEL.Close (Parent);
      end Check;
   begin
      Check ("config-full", C2.Return_Success);
      Check ("config-fail", C2.Fail);
      declare
         Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "config-positive");
         Channel : C2.Control_Channel := C2.Open (Parent, Config);
      begin
         C2.Enable (Channel);
         declare
            Request : C2.Return_Request :=
              C2.Submit_Config_Set (Channel, 16#8000_0000#, 9_876_543_210, "positive");
         begin
            if C2.Value (C2.Wait (Request, 1_000)) /= C2.Return_Success then
               raise Program_Error with "positive ConfigSet failed";
            end if;
            C2.Close (Request);
         end;
         C2.Close (Channel);
         AMS.MEL.Close (Parent);
      end;
      declare
         Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "config-reject");
         Channel : C2.Control_Channel := C2.Open (Parent, Config);
      begin
         C2.Enable (Channel);
         declare
            Request : C2.Return_Request := C2.Submit_Config_Set (Channel);
            Result  : constant C2.Return_Result := C2.Wait (Request, 1_000);
         begin
            if C2.Status (Result) /= C2.Rejected
              or else C2.Rejection_Code (Result) /= C2.Invalid_Parameters
            then
               raise Program_Error with "Ada ConfigSet rejection mismatch";
            end if;
            C2.Close (Request);
         end;
         C2.Close (Channel);
         AMS.MEL.Close (Parent);
      end;
   end Test_Config_Set;

   procedure Run (Provider_Path : String) is
   begin
      Test_Success (Provider_Path);
      Test_Timeout_Lifetime (Provider_Path);
      Test_Rejection (Provider_Path);
      Test_Long_Rejection (Provider_Path);
      Test_Pending_Finalization (Provider_Path);
      Test_BIT (Provider_Path);
      Test_BIT_Fail (Provider_Path);
      Test_BIT_Timeout (Provider_Path);
      Test_BIT_Rejection (Provider_Path);
      Test_BIT_Pending_Finalization (Provider_Path);
      Test_General_Mode (Provider_Path);
      Test_BIT_Choices (Provider_Path);
      Test_Config_Set (Provider_Path);
      Ada.Text_IO.Put_Line ("PASS: Ada required IR C2 command contract");
   end Run;
end AMS_MEL_IR_C2_Tests;
