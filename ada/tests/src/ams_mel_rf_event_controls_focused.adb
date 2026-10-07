with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.RF.C2;
with AMS.MEL.RF.C2.Interval_Status;
with Ada.Environment_Variables;
with Ada.Text_IO;
with Interfaces;
with Interfaces.C;
with Interfaces.C.Strings;
with Ada.Unchecked_Conversion;
with System;

procedure AMS_MEL_RF_Event_Controls_Focused is
   package C2 renames AMS.MEL.RF.C2;
   package Status renames AMS.MEL.RF.C2.Interval_Status;
   use type C2.Request_Outcome;
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

   Path : constant String :=
     Ada.Environment_Variables.Value ("AMS_MEL_TEST_PROVIDER_DIR") & "/libmock_rf_provider.so";
   procedure Send (Object : in out C2.Job; Enabled : Boolean; Polarizations : Natural) is
      Event    : C2.RX_Receive_Event_Config :=
        C2.Create_RX_Receive_Event (1, "rx/µ-main", 0, 1, 1.0, 2.0);
      Interval : C2.RX_Job_Interval_Config := C2.Create_RX_Job_Interval (1, 2);
      List     : C2.RX_Job_Interval_List;
   begin
      for I in 1 .. Polarizations loop
         C2.Append_Polarization (Event, Long_Float (I), -0.0, 2.25, -3.5);
      end loop;
      if Polarizations = 2 then
         begin
            C2.Append_Polarization (Event, 0.0, 0.0, 0.0, 0.0);
            raise Program_Error with "third polarization accepted";
         exception
            when Constraint_Error =>
               null;
         end;
      end if;
      if Polarizations > 0 then
         C2.Set_Polarization_Beam_Steer_Correction (Event, True);
         C2.Set_Phase_Offset_Radians (Event, -0.0);
         C2.Set_Event_Execution_Type (Event, C2.Conditional);
         C2.Set_Event_Termination_Type (Event, C2.Cancel_Event);
         C2.Set_Allow_Delay_Start (Event, True);
         C2.Set_Iteration_Hold_Count (Event, Interfaces.Unsigned_64'Last);
         C2.Set_Iteration_Termination_Count (Event, 16#8000_0000_0000_0000#);
         C2.Set_Channelization_Enabled (Event, True);
         C2.Set_Interval_TX_Power_Mode_ID (Interval, 16#DEAD_BEEF#);
         C2.Set_Interval_Activity_ID (Interval, [0, 16#FF#, 16#80#, 0, 16#7F#]);
         C2.Set_Interval_Execution_Type (Interval, C2.Conditional);
         C2.Set_Stab_Point_Index (Event, 999);
         C2.Append_Applicable_RX_Element_Group (Event, 2);
         C2.Append_Applicable_RX_Element_Group (Event, 0);
         C2.Append_Applicable_RX_Element_Group (Event, 2);
         C2.Append_Stab_Point (Interval, C2.Create_Face_Relative_Pointing (1.5, -0.5));
      end if;
      if Enabled then
         C2.Set_Interval_Status_Enable (Interval, C2.Always);
      end if;
      C2.Append_RX_Event (Interval, Event);
      C2.Append_Job_Interval (List, Interval);
      C2.Add_RX_Job_Intervals (Object, List);
      Ada.Environment_Variables.Set ("AMS_MEL_TEST_F5_POLARIZATIONS", Polarizations'Image);
      if Mock_Calls (Path, "mock_rf_f5_safe_fidelity") /= 1 then
         raise Program_Error with "safe F5 payload fidelity failed";
      end if;
   end Send;
begin
   for Repeat in 1 .. 50 loop
      declare
         Parent     : C2.C2_MEL := C2.Open (Path, "c2:job-ok");
         VA_Request : C2.Virtual_Aperture_Request := C2.Submit_Virtual_Aperture (Parent, VA_Config);
      begin
         if C2.Outcome (C2.Wait (VA_Request, 3_000)) /= C2.Created then
            raise Program_Error;
         end if;
         declare
            VA      : C2.Virtual_Aperture := C2.Claim (VA_Request);
            Request : C2.Job_Request := C2.Submit_Job (VA, Job_Config);
         begin
            if C2.Outcome (C2.Wait (Request, 3_000)) /= C2.Created then
               raise Program_Error;
            end if;
            declare
               Object : C2.Job := C2.Claim (Request);
            begin
               Send (Object, False, 0);
               Send (Object, False, 1);
               Send (Object, False, 2);
               begin
                  Send (Object, True, 2);
                  raise Program_Error with "status gate accepted";
               exception
                  when AMS.MEL.Provider_Error =>
                     null;
               end;
               declare
                  Stream : Status.Stream := Status.Open (Object, 2, 1, 0);
               begin
                  Send (Object, True, 2);
                  Ada.Environment_Variables.Set ("AMS_MEL_TEST_F4_ADD_FAILURE", "std");
                  begin
                     Send (Object, True, 2);
                     raise Program_Error with "provider failure accepted";
                  exception
                     when AMS.MEL.Provider_Error =>
                        null;
                  end;
                  Ada.Environment_Variables.Clear ("AMS_MEL_TEST_F4_ADD_FAILURE");
                  Send (Object, True, 2);
                  Status.Close (Stream);
               end;
               C2.Close (Object);
            end;
            C2.Close (Request);
            C2.Close (VA);
         end;
         C2.Close (VA_Request);
         C2.Close (Parent);
      end;
   end loop;
   Ada.Text_IO.Put_Line ("PASS: safe Ada RX event controls focused 50/50");
end AMS_MEL_RF_Event_Controls_Focused;
