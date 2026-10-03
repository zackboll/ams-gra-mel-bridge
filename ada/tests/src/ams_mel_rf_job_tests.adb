with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.RF.C2;
with Ada.Unchecked_Conversion;
with Ada.Text_IO;
with Ada.Strings.Unbounded;
with Interfaces;
with Interfaces.C;
with Interfaces.C.Strings;
with System;

package body AMS_MEL_RF_Job_Tests is
   --  These fixtures deliberately carry IEEE NaN/infinity to the provider.
   pragma Validity_Checks ("F");
   package C2 renames AMS.MEL.RF.C2;
   use type C2.Request_Outcome;
   use type C2.Request_Error_Code;
   use type C2.Job_Status;
   use type C2.Cancel_Error;
   use type Interfaces.Unsigned_32;
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
   function Job_Getter_Calls (Path : String) return Interfaces.C.unsigned
   is (Mock_Calls (Path, "mock_rf_job_get_calls"));
   function Finalize_Calls (Path : String) return Interfaces.C.unsigned
   is (Mock_Calls (Path, "mock_rf_job_finalize_calls"));
   function Cancel_Calls (Path : String) return Interfaces.C.unsigned
   is (Mock_Calls (Path, "mock_rf_job_cancel_calls"));
   procedure Expect_Failure (Operation : not null access procedure) is
   begin
      Operation.all;
      raise Program_Error with "expected Provider_Error";
   exception
      when AMS.MEL.Provider_Error =>
         null;
   end Expect_Failure;
   procedure Release_Job (Path : String) is
      Name   : Interfaces.C.Strings.chars_ptr := Interfaces.C.Strings.New_String (Path);
      Symbol : Interfaces.C.Strings.chars_ptr :=
        Interfaces.C.Strings.New_String ("mock_rf_job_release_one");
      Handle : constant System.Address := DL_Open (Name, 2);
   begin
      if Handle = System.Null_Address
        or else To_Release (DL_Sym (Handle, Symbol)).all /= 1
        or else DL_Close (Handle) /= 0
      then
         raise Program_Error with "mock Job release failed";
      end if;
      Interfaces.C.Strings.Free (Name);
      Interfaces.C.Strings.Free (Symbol);
   end Release_Job;
   procedure Release_Finalize (Path : String) is
      Name   : Interfaces.C.Strings.chars_ptr := Interfaces.C.Strings.New_String (Path);
      Symbol : Interfaces.C.Strings.chars_ptr :=
        Interfaces.C.Strings.New_String ("mock_rf_job_resolve_finalize");
      Handle : constant System.Address := DL_Open (Name, 2);
   begin
      if Handle = System.Null_Address
        or else To_Release (DL_Sym (Handle, Symbol)).all /= 1
        or else DL_Close (Handle) /= 0
      then
         raise Program_Error with "mock finalize release failed";
      end if;
      Interfaces.C.Strings.Free (Name);
      Interfaces.C.Strings.Free (Symbol);
   end Release_Finalize;

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

   procedure Snapshot (Object : C2.Job) is
   begin
      if C2.Actual_Start_Seconds (Object) /= -123456789
        or else C2.Actual_Start_Femtoseconds (Object) /= 999999999999999
        or else C2.Total_Job_Duration_Femtoseconds (Object) /= 7654321098765
        or else C2.VA_Instance_ID (Object) /= 42
        or else C2.VA_Definition_ID (Object) /= 16#ABCD_EF01#
        or else C2.Job_Details_ID (Object) /= 16#1020_3040#
        or else C2.Job_Request_ID (Object) /= 16#FEDC_BA98#
        or else C2.Lookahead_Femtoseconds (Object) /= -12345
        or else C2.RX_Stream_ID_Count (Object) /= 3
        or else C2.RX_Stream_ID_At (Object, 1) /= 0
        or else C2.RX_Stream_ID_At (Object, 2) /= 3
        or else C2.RX_Stream_ID_At (Object, 3) /= Interfaces.Unsigned_32'Last
      then
         raise Program_Error with "Ada Job snapshot lost data";
      end if;
   end Snapshot;

   function Special (Bits : Interfaces.Unsigned_64) return Long_Float is
      function Convert is new Ada.Unchecked_Conversion (Interfaces.Unsigned_64, Long_Float);
   begin
      return Convert (Bits);
   end Special;
   function Intervals return C2.RX_Job_Interval_List is
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
      B    : constant C2.RX_Job_Interval_Config :=
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
      C2.Append_RX_Event
        (A,
         C2.Create_RX_Receive_Event
           (16#8000_0001#,
            "rx/µ-main",
            -123,
            456789,
            987654321.125,
            2000000.5,
            16#1_0000_0007#,
            3,
            -999));
      C2.Append_RX_Event
        (A,
         C2.Create_RX_Receive_Event
           (Interfaces.Unsigned_32'Last,
            "β-secondary",
            777,
            -888,
            Special (16#8000_0000_0000_0000#),
            Special (16#7FF0_0000_0000_0000#),
            0,
            16#1_0000_0009#,
            Interfaces.Integer_64'Last - 1));
      C2.Append_Job_Interval (List, A);
      C2.Append_Job_Interval (List, B);
      return List;
   end Intervals;
   procedure Interval_Tests (Provider_Path : String) is
      List : constant C2.RX_Job_Interval_List := Intervals;
   begin
      if C2.Job_Interval_Count (List) /= 2 then
         raise Program_Error with "interval construction mismatch";
      end if;
      begin
         declare
            Invalid  : constant C2.RX_Receive_Event_Config :=
              C2.Create_RX_Receive_Event (1, "a" & Character'Val (0), 0, 0, 0.0, 0.0);
            Interval : C2.RX_Job_Interval_Config := C2.Create_RX_Job_Interval (1, 0);
         begin
            C2.Append_RX_Event (Interval, Invalid);
            raise Program_Error with "NUL label accepted";
         end;
      exception
         when Constraint_Error =>
            null;
      end;
      for Repeat in 1 .. 50 loop
         declare
            Parent     : C2.C2_MEL := C2.Open (Provider_Path, "c2:finalize-cancel");
            VA_Request : C2.Virtual_Aperture_Request :=
              C2.Submit_Virtual_Aperture (Parent, VA_Config);
         begin
            if C2.Outcome (C2.Wait (VA_Request, 3000)) /= C2.Created then
               raise Program_Error;
            end if;
            declare
               VA      : C2.Virtual_Aperture := C2.Claim (VA_Request);
               Request : C2.Job_Request := C2.Submit_Job (VA, Job_Config);
            begin
               C2.Close (VA_Request);
               if C2.Outcome (C2.Wait (Request, 3000)) /= C2.Created then
                  raise Program_Error;
               end if;
               declare
                  Object    : C2.Job := C2.Claim (Request);
                  Adds      : constant Interfaces.C.unsigned :=
                    Mock_Calls (Provider_Path, "mock_rf_job_add_calls");
                  Flushes   : constant Interfaces.C.unsigned :=
                    Mock_Calls (Provider_Path, "mock_rf_job_flush_calls");
                  Remaining : constant Interfaces.C.unsigned :=
                    Mock_Calls (Provider_Path, "mock_rf_job_remaining_calls");
                  procedure Add is
                  begin
                     C2.Add_RX_Job_Intervals (Object, List);
                  end Add;
                  procedure Flush is
                  begin
                     C2.Flush_Job (Object);
                  end Flush;
                  procedure Cancel_Remaining is
                  begin
                     C2.Cancel_Remaining_Job_Intervals (Object);
                  end Cancel_Remaining;
               begin
                  C2.Close (Request);
                  if Repeat = 1 then
                     declare
                        Boundary_List : C2.RX_Job_Interval_List;
                        Starts        : constant array (1 .. 4) of Interfaces.Integer_64 :=
                          [0, 1, -1, Interfaces.Integer_64'Last];
                     begin
                        for Start of Starts loop
                           C2.Append_Job_Interval
                             (Boundary_List,
                              C2.Create_RX_Job_Interval
                                (1, 0, Interval_Start_Femtoseconds => Start));
                        end loop;
                        C2.Add_RX_Job_Intervals (Object, Boundary_List);
                        if Mock_Calls (Provider_Path, "mock_rf_job_start_boundary") /= 1 then
                           raise Program_Error with "Ada interval start was remapped";
                        end if;
                     end;
                  end if;
                  C2.Add_RX_Job_Intervals (Object, List);
                  if Mock_Calls (Provider_Path, "mock_rf_job_add_calls")
                    /= Adds + (if Repeat = 1 then 2 else 1)
                    or else Mock_Calls (Provider_Path, "mock_rf_job_interval_fidelity") /= 1
                  then
                     raise Program_Error with "Ada interval nested fidelity/defaults mismatch";
                  end if;
                  C2.Flush_Job (Object);
                  C2.Flush_Job (Object);
                  C2.Cancel_Remaining_Job_Intervals (Object);
                  C2.Finalize_Job (Object);
                  begin
                     declare
                        Value : constant C2.Job_Status := C2.Wait_Job_Status (Object, 0);
                     begin
                        raise Program_Error with "expected timeout" & Value'Image;
                     end;
                  exception
                     when C2.Timeout_Error =>
                        null;
                  end;
                  Expect_Failure (Add'Access);
                  Expect_Failure (Flush'Access);
                  C2.Close (VA);
                  C2.Close (Parent);
                  C2.Cancel_Remaining_Job_Intervals (Object);
                  Snapshot (Object);
                  declare
                     Result : constant C2.Cancel_Result := C2.Cancel_Job (Object);
                  begin
                     if not C2.Cancelled (Result) then
                        raise Program_Error;
                     end if;
                  end;
                  if C2.Wait_Job_Status (Object, 3000) /= C2.Complete then
                     raise Program_Error;
                  end if;
                  Expect_Failure (Cancel_Remaining'Access);
                  if Mock_Calls (Provider_Path, "mock_rf_job_add_calls")
                    /= Adds + (if Repeat = 1 then 2 else 1)
                    or else Mock_Calls (Provider_Path, "mock_rf_job_flush_calls") /= Flushes + 2
                    or else Mock_Calls (Provider_Path, "mock_rf_job_remaining_calls")
                            /= Remaining + 2
                  then
                     raise Program_Error with "Ada interval operation counts mismatch";
                  end if;
                  Snapshot (Object);
                  C2.Close (Object);
               end;
            end;
         end;
      end loop;
      Ada.Text_IO.Put_Line ("PASS: safe Ada RX JobInterval focused repeat 50/50");
      for Variant in 1 .. 9 loop
         declare
            Names      : constant array (1 .. 9) of Ada.Strings.Unbounded.Unbounded_String :=
              [Ada.Strings.Unbounded.To_Unbounded_String ("c2:add-throw"),
               Ada.Strings.Unbounded.To_Unbounded_String ("c2:add-unknown"),
               Ada.Strings.Unbounded.To_Unbounded_String ("c2:add-alloc"),
               Ada.Strings.Unbounded.To_Unbounded_String ("c2:flush-throw"),
               Ada.Strings.Unbounded.To_Unbounded_String ("c2:flush-unknown"),
               Ada.Strings.Unbounded.To_Unbounded_String ("c2:flush-alloc"),
               Ada.Strings.Unbounded.To_Unbounded_String ("c2:remaining-throw"),
               Ada.Strings.Unbounded.To_Unbounded_String ("c2:remaining-unknown"),
               Ada.Strings.Unbounded.To_Unbounded_String ("c2:remaining-alloc")];
            Parent     : C2.C2_MEL :=
              C2.Open (Provider_Path, Ada.Strings.Unbounded.To_String (Names (Variant)));
            VA_Request : C2.Virtual_Aperture_Request :=
              C2.Submit_Virtual_Aperture (Parent, VA_Config);
         begin
            if C2.Outcome (C2.Wait (VA_Request, 3000)) /= C2.Created then
               raise Program_Error;
            end if;
            declare
               VA      : C2.Virtual_Aperture := C2.Claim (VA_Request);
               Request : C2.Job_Request := C2.Submit_Job (VA, Job_Config);
            begin
               C2.Close (VA_Request);
               if C2.Outcome (C2.Wait (Request, 3000)) /= C2.Created then
                  raise Program_Error;
               end if;
               declare
                  Object  : C2.Job := C2.Claim (Request);
                  Counter : constant String :=
                    (if Variant <= 3
                     then "mock_rf_job_add_calls"
                     elsif Variant <= 6
                     then "mock_rf_job_flush_calls"
                     else "mock_rf_job_remaining_calls");
                  Before  : constant Interfaces.C.unsigned := Mock_Calls (Provider_Path, Counter);
                  procedure Fail is
                  begin
                     if Variant <= 3 then
                        C2.Add_RX_Job_Intervals (Object, List);
                     elsif Variant <= 6 then
                        C2.Flush_Job (Object);
                     else
                        C2.Cancel_Remaining_Job_Intervals (Object);
                     end if;
                  end Fail;
               begin
                  C2.Close (Request);
                  Expect_Failure (Fail'Access);
                  if Mock_Calls (Provider_Path, Counter) /= Before + 1 or not C2.Is_Open (Object)
                  then
                     raise Program_Error with "mutating call retried/Job lost";
                  end if;
                  Snapshot (Object);
                  C2.Close (Object);
                  C2.Close (VA);
                  C2.Close (Parent);
               end;
            end;
         end;
      end loop;
   end Interval_Tests;

   procedure Lifecycle (Provider_Path : String) is
   begin
      for Variant in 1 .. 6 loop
         declare
            Name       : constant String :=
              (case Variant is
                 when 1      => "c2:finalize-none",
                 when 2      => "c2:finalize-progress",
                 when 3      => "c2:finalize-complete",
                 when 4      => "c2:finalize-invalid-id",
                 when 5      => "c2:finalize-interrupted",
                 when others => "c2:finalize-invalid-state");
            Expected   : constant C2.Job_Status :=
              (case Variant is
                 when 1      => C2.None,
                 when 2      => C2.In_Progress,
                 when 3      => C2.Complete,
                 when 4      => C2.Failed_Invalid_ID,
                 when 5      => C2.Failed_Interrupted,
                 when others => C2.Failed_Invalid_State);
            Parent     : C2.C2_MEL := C2.Open (Provider_Path, Name);
            VA_Request : C2.Virtual_Aperture_Request :=
              C2.Submit_Virtual_Aperture (Parent, VA_Config);
         begin
            if C2.Outcome (C2.Wait (VA_Request, 3_000)) /= C2.Created then
               raise Program_Error with "lifecycle VA rejected";
            end if;
            declare
               VA      : C2.Virtual_Aperture := C2.Claim (VA_Request);
               Request : C2.Job_Request := C2.Submit_Job (VA, Job_Config);
            begin
               C2.Close (VA_Request);
               if C2.Outcome (C2.Wait (Request, 3_000)) /= C2.Created then
                  raise Program_Error with "lifecycle Job rejected";
               end if;
               declare
                  Object : C2.Job := C2.Claim (Request);
                  Before : constant Interfaces.C.unsigned := Finalize_Calls (Provider_Path);
                  procedure Wait_Before is
                     Ignored : constant C2.Job_Status := C2.Wait_Job_Status (Object, 0);
                  begin
                     raise Program_Error with "unexpected Job status" & Ignored'Image;
                  end Wait_Before;
               begin
                  C2.Close (Request);
                  Expect_Failure (Wait_Before'Access);
                  C2.Finalize_Job (Object);
                  C2.Finalize_Job (Object);
                  if Finalize_Calls (Provider_Path) /= Before + 1
                    or else C2.Wait_Job_Status (Object, 3_000) /= Expected
                    or else C2.Wait_Job_Status (Object, 0) /= Expected
                  then
                     raise Program_Error with "Job status caching/mapping mismatch";
                  end if;
                  Snapshot (Object);
                  C2.Cancel_Remaining_Job_Intervals (Object);
                  C2.Close (VA);
                  C2.Close (Parent);
                  C2.Close (Object);
               end;
            end;
         end;
      end loop;
      for Variant in 1 .. 4 loop
         declare
            Name       : constant String :=
              (case Variant is
                 when 1      => "c2:finalize-throw",
                 when 2      => "c2:finalize-invalid",
                 when 3      => "c2:finalize-future-throw",
                 when others => "c2:finalize-unknown");
            Parent     : C2.C2_MEL := C2.Open (Provider_Path, Name);
            VA_Request : C2.Virtual_Aperture_Request :=
              C2.Submit_Virtual_Aperture (Parent, VA_Config);
         begin
            if C2.Outcome (C2.Wait (VA_Request, 3_000)) /= C2.Created then
               raise Program_Error;
            end if;
            declare
               VA      : C2.Virtual_Aperture := C2.Claim (VA_Request);
               Request : C2.Job_Request := C2.Submit_Job (VA, Job_Config);
            begin
               C2.Close (VA_Request);
               if C2.Outcome (C2.Wait (Request, 3_000)) /= C2.Created then
                  raise Program_Error;
               end if;
               declare
                  Object : C2.Job := C2.Claim (Request);
                  Before : constant Interfaces.C.unsigned := Finalize_Calls (Provider_Path);
                  procedure Start is
                  begin
                     C2.Finalize_Job (Object);
                  end Start;
                  procedure Wait_Failed is
                     Ignored : constant C2.Job_Status := C2.Wait_Job_Status (Object, 3_000);
                  begin
                     raise Program_Error with "unexpected Job status" & Ignored'Image;
                  end Wait_Failed;
               begin
                  C2.Close (Request);
                  if Name = "c2:finalize-throw" or else Name = "c2:finalize-invalid" then
                     Expect_Failure (Start'Access);
                     Expect_Failure (Start'Access);
                  else
                     Start;
                     Start;
                  end if;
                  Expect_Failure (Wait_Failed'Access);
                  Expect_Failure (Wait_Failed'Access);
                  if Finalize_Calls (Provider_Path) /= Before + 1 then
                     raise Program_Error with "failed Finalize was retried";
                  end if;
                  Snapshot (Object);
                  C2.Close (Object);
                  C2.Close (VA);
                  C2.Close (Parent);
               end;
            end;
         end;
      end loop;
      for Variant in 1 .. 4 loop
         declare
            Name       : constant String :=
              (case Variant is
                 when 1      => "c2:cancel-false",
                 when 2      => "c2:cancel-success",
                 when 3      => "c2:cancel-unknown",
                 when others => "c2:cancel-throw");
            Parent     : C2.C2_MEL := C2.Open (Provider_Path, Name);
            VA_Request : C2.Virtual_Aperture_Request :=
              C2.Submit_Virtual_Aperture (Parent, VA_Config);
         begin
            if C2.Outcome (C2.Wait (VA_Request, 3_000)) /= C2.Created then
               raise Program_Error;
            end if;
            declare
               VA      : C2.Virtual_Aperture := C2.Claim (VA_Request);
               Request : C2.Job_Request := C2.Submit_Job (VA, Job_Config);
            begin
               C2.Close (VA_Request);
               if C2.Outcome (C2.Wait (Request, 3_000)) /= C2.Created then
                  raise Program_Error;
               end if;
               declare
                  Object : C2.Job := C2.Claim (Request);
                  Before : constant Interfaces.C.unsigned := Cancel_Calls (Provider_Path);
                  Starts : constant Interfaces.C.unsigned := Finalize_Calls (Provider_Path);
                  procedure Stop is
                     Ignored : constant C2.Cancel_Result := C2.Cancel_Job (Object);
                  begin
                     raise Program_Error
                       with "unexpected cancel result" & Boolean'Image (C2.Cancelled (Ignored));
                  end Stop;
                  procedure Start is
                  begin
                     C2.Finalize_Job (Object);
                  end Start;
                  procedure Add_After_Cancel is
                  begin
                     C2.Add_RX_Job_Intervals (Object, Intervals);
                  end Add_After_Cancel;
                  procedure Flush_After_Cancel is
                  begin
                     C2.Flush_Job (Object);
                  end Flush_After_Cancel;
                  procedure Remaining_After_Cancel is
                  begin
                     C2.Cancel_Remaining_Job_Intervals (Object);
                  end Remaining_After_Cancel;
               begin
                  C2.Close (Request);
                  if Name = "c2:cancel-unknown" or else Name = "c2:cancel-throw" then
                     Expect_Failure (Stop'Access);
                     Expect_Failure (Stop'Access);
                  else
                     for I in 1 .. 2 loop
                        declare
                           Result : constant C2.Cancel_Result := C2.Cancel_Job (Object);
                        begin
                           if (C2.Cancelled (Result) and then Variant /= 2)
                             or else (not C2.Cancelled (Result) and then Variant = 2)
                             or else C2.Error_Code (Result) not in C2.None
                           then
                              raise Program_Error with "cancel bool/error mismatch";
                           end if;
                        end;
                     end loop;
                  end if;
                  Expect_Failure (Start'Access);
                  Expect_Failure (Add_After_Cancel'Access);
                  Expect_Failure (Flush_After_Cancel'Access);
                  Expect_Failure (Remaining_After_Cancel'Access);
                  if Cancel_Calls (Provider_Path) /= Before + 1
                    or else Finalize_Calls (Provider_Path) /= Starts
                  then
                     raise Program_Error with "cancel-first was retried/finalized";
                  end if;
                  Snapshot (Object);
                  C2.Close (Object);
                  C2.Close (VA);
                  C2.Close (Parent);
               end;
            end;
         end;
      end loop;
      for Variant in 1 .. 2 loop
         declare
            Name       : constant String :=
              (if Variant = 1 then "c2:finalize-delayed" else "c2:finalize-cancel");
            Parent     : C2.C2_MEL := C2.Open (Provider_Path, Name);
            VA_Request : C2.Virtual_Aperture_Request :=
              C2.Submit_Virtual_Aperture (Parent, VA_Config);
         begin
            if C2.Outcome (C2.Wait (VA_Request, 3_000)) /= C2.Created then
               raise Program_Error;
            end if;
            declare
               VA      : C2.Virtual_Aperture := C2.Claim (VA_Request);
               Request : C2.Job_Request := C2.Submit_Job (VA, Job_Config);
            begin
               C2.Close (VA_Request);
               if C2.Outcome (C2.Wait (Request, 3_000)) /= C2.Created then
                  raise Program_Error;
               end if;
               declare
                  Object : C2.Job := C2.Claim (Request);
               begin
                  C2.Close (Request);
                  C2.Finalize_Job (Object);
                  begin
                     declare
                        Unexpected : constant C2.Job_Status := C2.Wait_Job_Status (Object, 0);
                     begin
                        raise Program_Error with "poll did not time out" & Unexpected'Image;
                     end;
                  exception
                     when C2.Timeout_Error =>
                        null;
                  end;
                  C2.Close (VA);
                  C2.Close (Parent);
                  if Name = "c2:finalize-delayed" then
                     Release_Finalize (Provider_Path);
                  else
                     declare
                        Result : constant C2.Cancel_Result := C2.Cancel_Job (Object);
                     begin
                        if not C2.Cancelled (Result) or else C2.Error_Code (Result) not in C2.None
                        then
                           raise Program_Error with "cancel did not complete Job";
                        end if;
                     end;
                  end if;
                  if C2.Wait_Job_Status (Object, 3_000) /= C2.Complete then
                     raise Program_Error with "pending Job did not complete";
                  end if;
                  Snapshot (Object);
                  C2.Close (Object);
               end;
            end;
         end;
      end loop;
   end Lifecycle;

   procedure Run (Provider_Path : String) is
   begin
      declare
         Group : C2.RX_Element_Group_Config := C2.Create_RX_Element_Group ("rx", 1.0);
         procedure Expect_Bad_Label is
         begin
            declare
               Bad    : constant C2.RX_Element_Group_Config :=
                 C2.Create_RX_Element_Group ("rx" & Character'Val (0));
               Config : C2.Job_Config := C2.Create_Job_Config (0, 0, Bad);
            begin
               C2.Append_Instance_Selection (Config, 0);
            end;
            raise Program_Error with "NUL group label accepted";
         exception
            when Constraint_Error =>
               null;
         end Expect_Bad_Label;
      begin
         Expect_Bad_Label;
         begin
            declare
               Bad    : constant C2.RX_Element_Group_Config :=
                 C2.Create_RX_Element_Group ("rx", Data_Pipe_Label => "bad" & Character'Val (0));
               Config : C2.Job_Config := C2.Create_Job_Config (0, 0, Bad);
            begin
               C2.Append_Instance_Selection (Config, 0);
            end;
            raise Program_Error with "NUL data-pipe label accepted";
         exception
            when Constraint_Error =>
               null;
         end;
         begin
            C2.Append_Endpoint_ID (Group, 0);
            C2.Append_Endpoint_ID (Group, 0);
            raise Program_Error with "duplicate endpoint accepted";
         exception
            when Constraint_Error =>
               null;
         end;
         begin
            C2.Append_Expected_Center_Frequency (Group, 2.0, 1.0);
            raise Program_Error with "reversed frequency accepted";
         exception
            when Constraint_Error =>
               null;
         end;
         begin
            declare
               Bad    : constant C2.RX_Element_Group_Config :=
                 C2.Create_RX_Element_Group ("rx", 0.0);
               Config : C2.Job_Config := C2.Create_Job_Config (0, 0, Bad);
            begin
               C2.Append_Instance_Selection (Config, 0);
            end;
            raise Program_Error with "zero duty accepted";
         exception
            when Constraint_Error =>
               null;
         end;
      end;
      for Scenario in 1 .. 3 loop
         declare
            Parent     : C2.C2_MEL :=
              C2.Open (Provider_Path, (if Scenario = 1 then "c2:job-ok" else "c2:job-delayed"));
            VA_Request : C2.Virtual_Aperture_Request :=
              C2.Submit_Virtual_Aperture (Parent, VA_Config);
         begin
            if C2.Outcome (C2.Wait (VA_Request, 3_000)) /= C2.Created then
               raise Program_Error with "mock VA rejected";
            end if;
            declare
               VA      : C2.Virtual_Aperture := C2.Claim (VA_Request);
               Request : C2.Job_Request := C2.Submit_Job (VA, Job_Config);
            begin
               C2.Close (VA_Request);
               if Scenario /= 1 then
                  begin
                     declare
                        Ignore : constant C2.Job_Result := C2.Wait (Request, 0);
                     begin
                        raise Program_Error with "pending Job did not time out";
                     end;
                  exception
                     when C2.Timeout_Error =>
                        null;
                  end;
                  if Scenario = 3 then
                     C2.Close (VA);
                     C2.Close (Parent);
                  end if;
                  Release_Job (Provider_Path);
               end if;
               if C2.Outcome (C2.Wait (Request, 3_000)) /= C2.Created
                 or else C2.Outcome (C2.Wait (Request, 0)) /= C2.Created
               then
                  raise Program_Error with "mock Job not created";
               end if;
               declare
                  Object : C2.Job := C2.Claim (Request);
               begin
                  begin
                     declare
                        Second : constant C2.Job := C2.Claim (Request);
                     begin
                        if C2.Is_Open (Second) then
                           raise Program_Error with "second Job Claim accepted";
                        end if;
                     end;
                     raise Program_Error with "second Job Claim did not fail";
                  exception
                     when AMS.MEL.Provider_Error =>
                        null;
                  end;
                  C2.Close (Request);
                  C2.Close (Request);
                  if Scenario /= 3 then
                     C2.Close (VA);
                     C2.Close (Parent);
                  end if;
                  Snapshot (Object);
                  C2.Close (Object);
                  C2.Close (Object);
                  if C2.Is_Open (Object) or else C2.Is_Open (Request) then
                     raise Program_Error with "Job owners remained open";
                  end if;
               end;
            end;
         end;
      end loop;
      for Scenario in 1 .. 2 loop
         declare
            Parent     : C2.C2_MEL :=
              C2.Open
                (Provider_Path, (if Scenario = 1 then "c2:job-failure" else "c2:job-long-failure"));
            VA_Request : C2.Virtual_Aperture_Request :=
              C2.Submit_Virtual_Aperture (Parent, VA_Config);
         begin
            if C2.Outcome (C2.Wait (VA_Request, 3_000)) /= C2.Created then
               raise Program_Error;
            end if;
            declare
               VA      : C2.Virtual_Aperture := C2.Claim (VA_Request);
               Request : C2.Job_Request := C2.Submit_Job (VA, Job_Config);
               Result  : constant C2.Job_Result := C2.Wait (Request, 3_000);
            begin
               if C2.Outcome (Result) /= C2.Failed
                 or else C2.Error_Code (Result) /= C2.Invalid_Parameters
                 or else (if Scenario = 1
                          then C2.Description (Result) /= "mock Job rejected"
                          else
                            C2.Description (Result)'Length <= 800
                            or else C2.Description (Result)
                                      (C2.Description (Result)'Last
                                       - 5
                                       .. C2.Description (Result)'Last)
                                    /= "µ end")
               then
                  raise Program_Error with "Job failure diagnostic lost";
               end if;
               C2.Close (Request);
               C2.Close (VA);
               C2.Close (VA_Request);
               C2.Close (Parent);
            end;
         end;
      end loop;
      declare
         Parent     : C2.C2_MEL := C2.Open (Provider_Path, "c2:job-getter-throw");
         VA_Request : C2.Virtual_Aperture_Request := C2.Submit_Virtual_Aperture (Parent, VA_Config);
      begin
         if C2.Outcome (C2.Wait (VA_Request, 3_000)) /= C2.Created then
            raise Program_Error;
         end if;
         declare
            VA      : C2.Virtual_Aperture := C2.Claim (VA_Request);
            Request : C2.Job_Request := C2.Submit_Job (VA, Job_Config);
            Before  : Interfaces.C.unsigned;
         begin
            if C2.Outcome (C2.Wait (Request, 3_000)) /= C2.Created then
               raise Program_Error;
            end if;
            Before := Job_Getter_Calls (Provider_Path);
            for I in 1 .. 2 loop
               begin
                  declare
                     Object : constant C2.Job := C2.Claim (Request);
                  begin
                     if not C2.Is_Open (Object) then
                        raise Program_Error;
                     end if;
                     raise Program_Error with "getter failure accepted";
                  end;
               exception
                  when AMS.MEL.Provider_Error =>
                     null;
               end;
            end loop;
            if Job_Getter_Calls (Provider_Path) /= Before + 1 then
               raise Program_Error with "Job getters were rerun after failed Claim";
            end if;
            C2.Close (Request);
            C2.Close (VA);
            C2.Close (VA_Request);
            C2.Close (Parent);
         end;
      end;
      Interval_Tests (Provider_Path);
      Lifecycle (Provider_Path);
   end Run;
end AMS_MEL_RF_Job_Tests;
