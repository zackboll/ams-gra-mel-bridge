with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.RF.C2;
with Ada.Unchecked_Conversion;
with Ada.Text_IO;
with Interfaces;
with Interfaces.C;
with Interfaces.C.Strings;
with System;

with Ada.Environment_Variables;
with AMS.MEL.RF.C2.Interval_Status;

package body AMS_MEL_RF_Interval_Spatial is
   pragma Validity_Checks ("F");
   package C2 renames AMS.MEL.RF.C2;
   package Status renames AMS.MEL.RF.C2.Interval_Status;
   use type C2.Request_Outcome;
   use type Interfaces.Integer_64;
   use type Interfaces.C.int;
   use type Interfaces.C.unsigned;
   use type System.Address;
   function DL_Open
     (Path : Interfaces.C.Strings.chars_ptr; Flags : Interfaces.C.int) return System.Address
   with Import, Convention => C, External_Name => "dlopen";
   function DL_Sym
     (Handle : System.Address; Name : Interfaces.C.Strings.chars_ptr) return System.Address
   with Import, Convention => C, External_Name => "dlsym";
   function DL_Close (Handle : System.Address) return Interfaces.C.int
   with Import, Convention => C, External_Name => "dlclose";
   type Release_Access is access function return Interfaces.C.unsigned with Convention => C;
   function To_Release is new Ada.Unchecked_Conversion (System.Address, Release_Access);
   function Mock_Calls (Path, Operation : String) return Interfaces.C.unsigned is
      Name   : Interfaces.C.Strings.chars_ptr := Interfaces.C.Strings.New_String (Path);
      Symbol : Interfaces.C.Strings.chars_ptr := Interfaces.C.Strings.New_String (Operation);
      Handle : constant System.Address := DL_Open (Name, 2);
      Calls  : Interfaces.C.unsigned;
   begin
      if Handle = System.Null_Address then
         raise Program_Error with "mock load failed";
      end if;
      Calls := To_Release (DL_Sym (Handle, Symbol)).all;
      if DL_Close (Handle) /= 0 then
         raise Program_Error with "mock unload failed";
      end if;
      Interfaces.C.Strings.Free (Name);
      Interfaces.C.Strings.Free (Symbol);
      return Calls;
   end Mock_Calls;
   function VA_Config return C2.Virtual_Aperture_Config is
      Config               : C2.Virtual_Aperture_Config :=
        C2.Create_Virtual_Aperture_Config (16#FEDC_BA98#, 16#8000_0001#, "definition/β.json");
      First, Second, Third : AMS.MEL.IR.UUID := [others => 0];
   begin
      C2.Append_Local_Function_Info (Config, "alpha");
      C2.Append_Local_Function_Info (Config, "µ-local");
      C2.Append_Local_Function_Info (Config, "");
      First (1) := 16#80#;
      First (2) := 16#FF#;
      Second (0) := 16#FF#;
      Third (15) := 16#80#;
      C2.Append_Capability_ID (Config, AMS.MEL.IR.Create_UCI_ID (First, "first"));
      C2.Append_Capability_ID (Config, AMS.MEL.IR.Create_UCI_ID (Second, ""));
      C2.Append_Capability_ID (Config, AMS.MEL.IR.Create_UCI_ID (Third, "µ-third"));
      return Config;
   end VA_Config;

   function Job_Config return C2.Job_Config is
      Group : C2.RX_Element_Group_Config :=
        C2.Create_RX_Element_Group ("rx/µ-main", 0.625, "products/β");
   begin
      C2.Append_Expected_Center_Frequency (Group, 1000000.25, 2000000.5);
      C2.Append_Expected_Center_Frequency (Group, 987654321.125, 987654322.875);
      C2.Append_Endpoint_ID (Group, 0);
      C2.Append_Endpoint_ID (Group, 16#8000_0000_0000_0000#);
      C2.Append_Endpoint_ID (Group, Interfaces.Unsigned_64'Last);
      return
         Config : C2.Job_Config :=
           C2.Create_Job_Config (16#FEDC_BA98#, 16#8000_0001#, Group, 16#7FFF_FFFE#, True)
      do
         C2.Append_Instance_Selection (Config, 0);
         C2.Append_Instance_Selection (Config, 42);
         C2.Append_Instance_Selection (Config, Interfaces.Unsigned_32'Last);
      end return;
   end Job_Config;

   function Special (Bits : Interfaces.Unsigned_64) return Long_Float is
      function Convert is new Ada.Unchecked_Conversion (Interfaces.Unsigned_64, Long_Float);
   begin
      return Convert (Bits);
   end Special;
   function Intervals (Enabled : Boolean) return C2.RX_Job_Interval_List is
      A    : C2.RX_Job_Interval_Config :=
        C2.Create_RX_Job_Interval
          (Interval_ID                        => 16#1020_3040#,
           Sequence_Duration_Femtoseconds     => 9876543210123,
           Job_Details_ID                     => 16#ABCD_EF01#,
           Interval_Starting_Gap_Femtoseconds => -111,
           Sequence_Repeat_Count              => 16#1_0000_0003#,
           Calibration_Duration_Femtoseconds  => 222,
           Interval_Ending_Gap_Femtoseconds   => -333,
           Phase_Coherence_With_Prior         => True,
           Iterations_Per_Signal              => 16#1_0000_0005#,
           Max_Data_Rate_BPS                  => 123456789.25,
           Max_Sample_Rate_Hz                 => 2500000.5);
      B    : C2.RX_Job_Interval_Config :=
        C2.Create_RX_Job_Interval
          (Interval_ID                        => 42,
           Sequence_Duration_Femtoseconds     => -13,
           Job_Details_ID                     => 43,
           Interval_Start_Femtoseconds        => -1,
           Interval_Starting_Gap_Femtoseconds => 12,
           Sequence_Repeat_Count              => 2,
           Calibration_Duration_Femtoseconds  => -14,
           Interval_Ending_Gap_Femtoseconds   => 15,
           Iterations_Per_Signal              => 3,
           Max_Data_Rate_BPS                  => Special (16#7FF8_0000_0000_0001#),
           Max_Sample_Rate_Hz                 => Special (16#7FF0_0000_0000_0000#));
      List : C2.RX_Job_Interval_List;
   begin
      declare
         X    : C2.RX_Receive_Event_Config :=
           C2.Create_RX_Receive_Event
             (16#8000_0001#,
              "rx/µ-main",
              -123,
              456789,
              987654321.125,
              2000000.5,
              16#1_0000_0007#,
              3,
              -999);
         Y    : C2.RX_Receive_Event_Config :=
           C2.Create_RX_Receive_Event
             (Interfaces.Unsigned_32'Last,
              "β-secondary",
              777,
              -888,
              Special (16#8000_0000_0000_0000#),
              Special (16#7FF0_0000_0000_0000#),
              0,
              16#1_0000_0009#,
              Interfaces.Integer_64'Last - 1);
         Face : constant C2.Pointing := C2.Create_Face_Relative_Pointing (1.5, -0.5);
      begin
         C2.Set_Stab_Point_Index (X, 1);
         C2.Append_Applicable_RX_Element_Group (X, 2);
         C2.Append_Applicable_RX_Element_Group (X, 0);
         C2.Append_Applicable_RX_Element_Group (X, 2);
         C2.Set_Stab_Point_Index (Y, 999);
         C2.Append_Applicable_RX_Element_Group (Y, 7);
         C2.Append_Applicable_RX_Element_Group (Y, 7);
         C2.Append_Applicable_RX_Element_Group (Y, 1);
         C2.Append_RX_Event (A, X);
         C2.Append_RX_Event (A, Y);
         C2.Append_Stab_Point
           (A,
            C2.Create_ECEF_Pointing
              (1.25, -2.5, 3.75, -4.5, 5.625, -6.75, C2.Create_UTC_Time (-7, 123456789012345)));
         C2.Append_Stab_Point (A, C2.Create_Platform_Relative_Pointing (-0.75, 0.25));
         C2.Append_Stab_Point (A, Face);
         C2.Append_Stab_Point (A, Face);
         C2.Append_Stab_Point
           (B,
            C2.Create_LLA_Pointing
              (0.125,
               -1.25,
               12345.5,
               11.25,
               -12.5,
               13.75,
               C2.Create_UTC_Time (42, 999999999999999)));
         C2.Append_Stab_Point (B, C2.Create_Baseline_Relative_Pointing (-2.25));
         if Enabled then
            C2.Set_Interval_Status_Enable (A, C2.Always);
            C2.Set_Interval_Status_Enable (B, C2.On_Exception);
         end if;
      end;
      C2.Append_Job_Interval (List, A);
      C2.Append_Job_Interval (List, B);
      return List;
   end Intervals;
   procedure Require (Value : Boolean) is
   begin
      if not Value then
         raise Program_Error with "safe Ada interval spatial fidelity mismatch";
      end if;
   end Require;
   procedure Run (Provider_Path : String) is
   begin
      for Repeat in 1 .. 50 loop
         declare
            Parent   : C2.C2_MEL := C2.Open (Provider_Path, "c2:job-ok");
            VR       : C2.Virtual_Aperture_Request :=
              C2.Submit_Virtual_Aperture (Parent, VA_Config);
            V_Result : constant C2.Virtual_Aperture_Result := C2.Wait (VR, 3_000);
         begin
            Require (C2.Outcome (V_Result) = C2.Created);
            declare
               VA       : C2.Virtual_Aperture := C2.Claim (VR);
               JR       : C2.Job_Request := C2.Submit_Job (VA, Job_Config);
               J_Result : constant C2.Job_Result := C2.Wait (JR, 3_000);
            begin
               Require (C2.Outcome (J_Result) = C2.Created);
               declare
                  Object           : C2.Job := C2.Claim (JR);
                  List             : constant C2.RX_Job_Interval_List := Intervals (False);
                  Enabled          : constant C2.RX_Job_Interval_List := Intervals (True);
                  Default_Interval : C2.RX_Job_Interval_Config := C2.Create_RX_Job_Interval (1, 2);
                  Default_Event    : constant C2.RX_Receive_Event_Config :=
                    C2.Create_RX_Receive_Event (1, "", 0, 1, 2.0, 3.0);
                  Defaults         : C2.RX_Job_Interval_List;
               begin
                  C2.Close (JR);
                  C2.Close (VR);
                  C2.Append_RX_Event (Default_Interval, Default_Event);
                  C2.Append_Job_Interval (Defaults, Default_Interval);
                  C2.Add_RX_Job_Intervals (Object, Defaults);
                  Require (Mock_Calls (Provider_Path, "mock_rf_job_spatial_safe_defaults") = 1);
                  Require (C2.Job_Interval_Count (List) = 2);
                  C2.Add_RX_Job_Intervals (Object, List);
                  Require (Mock_Calls (Provider_Path, "mock_rf_job_interval_fidelity_v3") = 1);
                  begin
                     C2.Add_RX_Job_Intervals (Object, Enabled);
                     raise Program_Error with "missing registration accepted";
                  exception
                     when AMS.MEL.Provider_Error =>
                        null;
                  end;
                  declare
                     Stream : Status.Stream := Status.Open (Object, 2, 1, 0);
                  begin
                     Ada.Environment_Variables.Set ("AMS_MEL_TEST_F4_STATUS", "enabled");
                     C2.Add_RX_Job_Intervals (Object, Enabled);
                     Require (Mock_Calls (Provider_Path, "mock_rf_job_interval_fidelity_v3") = 1);
                     for Failure in 1 .. 3 loop
                        Ada.Environment_Variables.Set
                          ("AMS_MEL_TEST_F4_ADD_FAILURE",
                           (case Failure is
                              when 1      => "std",
                              when 2      => "unknown",
                              when others => "alloc"));
                        begin
                           C2.Add_RX_Job_Intervals (Object, Enabled);
                           raise Program_Error with "expected provider Add failure";
                        exception
                           when AMS.MEL.Provider_Error =>
                              null;
                        end;
                        Ada.Environment_Variables.Clear ("AMS_MEL_TEST_F4_ADD_FAILURE");
                        C2.Add_RX_Job_Intervals (Object, Enabled);
                        Require
                          (Mock_Calls (Provider_Path, "mock_rf_job_interval_fidelity_v3") = 1);
                     end loop;
                     Status.Close (Stream);
                     Ada.Environment_Variables.Clear ("AMS_MEL_TEST_F4_STATUS");
                  end;
                  C2.Close (VA);
                  C2.Close (Parent);
                  C2.Add_RX_Job_Intervals (Object, List);
                  Require (Mock_Calls (Provider_Path, "mock_rf_job_interval_fidelity_v3") = 1);
                  C2.Close (Object);
               end;
            end;
         end;
      end loop;
      Ada.Text_IO.Put_Line ("PASS: safe Ada RX JobInterval spatial focused repeat 50/50");
   end Run;
end AMS_MEL_RF_Interval_Spatial;
