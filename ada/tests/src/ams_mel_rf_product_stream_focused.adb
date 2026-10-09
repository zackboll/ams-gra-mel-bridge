with Ada.Strings.Fixed;
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

procedure AMS_MEL_RF_Product_Stream_Focused is
   package C2 renames AMS.MEL.RF.C2;
   package Status renames AMS.MEL.RF.C2.Interval_Status;
   use type C2.Request_Outcome;
   use type Interfaces.C.int;
   use type System.Address;
   function DL_Open
     (Path : Interfaces.C.Strings.chars_ptr; Flags : Interfaces.C.int) return System.Address
   with Import, Convention => C, External_Name => "dlopen";
   function DL_Sym
     (Handle : System.Address; Name : Interfaces.C.Strings.chars_ptr) return System.Address
   with Import, Convention => C, External_Name => "dlsym";
   function DL_Close (Handle : System.Address) return Interfaces.C.int
   with Import, Convention => C, External_Name => "dlclose";
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

   Path     : constant String :=
     Ada.Environment_Variables.Value ("AMS_MEL_TEST_PROVIDER_DIR") & "/libmock_rf_provider.so";
   type Scalar_Access is
     access function (I, E, F : Interfaces.C.unsigned) return Interfaces.Integer_64
   with Convention => C;
   type Number_Access is
     access function (I, E, T, Edge : Interfaces.C.unsigned) return Interfaces.C.double
   with Convention => C;
   function To_Scalar is new Ada.Unchecked_Conversion (System.Address, Scalar_Access);
   function To_Number is new Ada.Unchecked_Conversion (System.Address, Number_Access);
   use type Interfaces.Integer_64;
   use type Interfaces.Unsigned_64;
   use type Interfaces.C.double;
   pragma Validity_Checks ("F");
   function IEEE is new Ada.Unchecked_Conversion (Interfaces.Unsigned_64, Long_Float);
   function Bits is new Ada.Unchecked_Conversion (Interfaces.C.double, Interfaces.Unsigned_64);
   type Failure_Array is array (Positive range <>) of String (1 .. 7);
   Failures : constant Failure_Array := ["std    ", "unknown", "alloc  "];
   procedure Observe (Reference, Tag : Natural; Width : Interfaces.Integer_64; Count : Natural) is
      Name        : Interfaces.C.Strings.chars_ptr := Interfaces.C.Strings.New_String (Path);
      Scalar_Name : Interfaces.C.Strings.chars_ptr :=
        Interfaces.C.Strings.New_String ("mock_rf_f6_scalar");
      Number_Name : Interfaces.C.Strings.chars_ptr :=
        Interfaces.C.Strings.New_String ("mock_rf_f6_double");
      Handle      : constant System.Address := DL_Open (Name, 2);
      Scalar      : constant Scalar_Access := To_Scalar (DL_Sym (Handle, Scalar_Name));
      Number      : constant Number_Access := To_Number (DL_Sym (Handle, Number_Name));
   begin
      if Handle = System.Null_Address or else Scalar = null or else Number = null then
         raise Program_Error with "pulse observations unavailable";
      end if;
      for I in 0 .. 1 loop
         for E in 0 .. 1 loop
            if Scalar (Interfaces.C.unsigned (I), Interfaces.C.unsigned (E), 0)
              /= Interfaces.Integer_64 (Reference)
              or else Scalar (Interfaces.C.unsigned (I), Interfaces.C.unsigned (E), 5) /= Width
              or else Scalar (Interfaces.C.unsigned (I), Interfaces.C.unsigned (E), 6)
                      /= Interfaces.Integer_64 (Tag)
              or else Scalar (Interfaces.C.unsigned (I), Interfaces.C.unsigned (E), 7)
                      /= Interfaces.Integer_64 (Count)
            then
               raise Program_Error with "pulse scalar fidelity";
            end if;
            if Count > 0 then
               if Scalar (Interfaces.C.unsigned (I), Interfaces.C.unsigned (E), 1) /= 255
                 or else Scalar (Interfaces.C.unsigned (I), Interfaces.C.unsigned (E), 2) /= 0
                 or else Scalar (Interfaces.C.unsigned (I), Interfaces.C.unsigned (E), 3) /= 0
                 or else Scalar (Interfaces.C.unsigned (I), Interfaces.C.unsigned (E), 4) /= 255
                 or else Number (Interfaces.C.unsigned (I), Interfaces.C.unsigned (E), 0, 0)
                         /= 12.25
                 or else Number (Interfaces.C.unsigned (I), Interfaces.C.unsigned (E), 0, 1) /= -3.5
                 or else Bits (Number (Interfaces.C.unsigned (I), Interfaces.C.unsigned (E), 1, 0))
                         /= 16#8000_0000_0000_0000#
                 or else Bits (Number (Interfaces.C.unsigned (I), Interfaces.C.unsigned (E), 1, 1))
                         /= 16#7FF0_0000_0000_0000#
                 or else Number (Interfaces.C.unsigned (I), Interfaces.C.unsigned (E), 2, 0)
                         = Number (Interfaces.C.unsigned (I), Interfaces.C.unsigned (E), 2, 0)
                 or else Bits (Number (Interfaces.C.unsigned (I), Interfaces.C.unsigned (E), 2, 1))
                         /= 16#FFF0_0000_0000_0000#
                 or else Number (Interfaces.C.unsigned (I), Interfaces.C.unsigned (E), 3, 0)
                         /= 12.25
                 or else Number (Interfaces.C.unsigned (I), Interfaces.C.unsigned (E), 3, 1) /= -3.5
               then
                  raise Program_Error with "pulse threshold or M/N fidelity";
               end if;
            end if;
         end loop;
      end loop;
      if DL_Close (Handle) /= 0 then
         raise Program_Error;
      end if;
      Interfaces.C.Strings.Free (Name);
      Interfaces.C.Strings.Free (Scalar_Name);
      Interfaces.C.Strings.Free (Number_Name);
   end Observe;
   function LF_Value
     (I, Command, Write, Field : Interfaces.C.unsigned) return Interfaces.Unsigned_64
   is
      type Getter is
        access function
          (I, Command, Write, Field : Interfaces.C.unsigned) return Interfaces.Unsigned_64
      with Convention => C;
      function Convert is new Ada.Unchecked_Conversion (System.Address, Getter);
      Path_C  : Interfaces.C.Strings.chars_ptr := Interfaces.C.Strings.New_String (Path);
      Library : constant System.Address := DL_Open (Path_C, 2);
      Name    : Interfaces.C.Strings.chars_ptr :=
        Interfaces.C.Strings.New_String ("mock_rf_f7_value");
      Address : constant System.Address := DL_Sym (Library, Name);
      Result  : Interfaces.Unsigned_64;
   begin
      if Library = System.Null_Address or else Address = System.Null_Address then
         raise Program_Error;
      end if;
      Result := Convert (Address) (I, Command, Write, Field);
      Interfaces.C.Strings.Free (Name);
      Interfaces.C.Strings.Free (Path_C);
      if DL_Close (Library) /= 0 then
         raise Program_Error;
      end if;
      return Result;
   end LF_Value;
   function Stream_Value (I, Index, Field : Interfaces.C.unsigned) return Interfaces.Unsigned_64 is
      type Getter is
        access function (I, Index, Field : Interfaces.C.unsigned) return Interfaces.Unsigned_64
      with Convention => C;
      function Convert is new Ada.Unchecked_Conversion (System.Address, Getter);
      Path_C  : Interfaces.C.Strings.chars_ptr := Interfaces.C.Strings.New_String (Path);
      Library : constant System.Address := DL_Open (Path_C, 2);
      Name    : Interfaces.C.Strings.chars_ptr :=
        Interfaces.C.Strings.New_String ("mock_rf_f8_value");
      Address : constant System.Address := DL_Sym (Library, Name);
      Result  : Interfaces.Unsigned_64;
   begin
      if Library = System.Null_Address or else Address = System.Null_Address then
         raise Program_Error;
      end if;
      Result := Convert (Address) (I, Index, Field);
      Interfaces.C.Strings.Free (Name);
      Interfaces.C.Strings.Free (Path_C);
      if DL_Close (Library) /= 0 then
         raise Program_Error;
      end if;
      return Result;
   end Stream_Value;
   procedure Send_Empty (Object : in out C2.Job) is
      Absent : constant C2.RX_Job_Interval_Config := C2.Create_RX_Job_Interval (1, 2);
      Empty  : C2.RX_Job_Interval_Config := Absent;
      List   : C2.RX_Job_Interval_List;
   begin
      C2.Set_Interval_Product_Stream_Params (Empty, C2.Create_Product_Stream_Params);
      C2.Append_Job_Interval (List, Absent);
      C2.Append_Job_Interval (List, Empty);
      C2.Add_RX_Job_Intervals (Object, List);
      for I in Interfaces.C.unsigned range 0 .. 1 loop
         if Stream_Value (I, 0, 0) /= 0 or else Stream_Value (I, 0, 1) /= 0 then
            raise Program_Error with "legacy absent/explicit empty product stream";
         end if;
      end loop;
   end Send_Empty;
   procedure Send
     (Object : in out C2.Job; Enabled : Boolean; Polarizations : Natural; Clear : Boolean := False)
   is
      Event        : C2.RX_Receive_Event_Config :=
        C2.Create_RX_Receive_Event (1, "rx/µ-main", 0, 1, 1.0, 2.0);
      Interval     : C2.RX_Job_Interval_Config := C2.Create_RX_Job_Interval (1, 2);
      List         : C2.RX_Job_Interval_List;
      Reference    : constant C2.Pulse_Threshold_Reference :=
        C2.Pulse_Threshold_Reference'Val (Polarizations);
      Tag          : constant C2.Pulse_Time_Tag_Threshold :=
        C2.Pulse_Time_Tag_Threshold'Val (Polarizations mod 2);
      Width        : constant Interfaces.Integer_64 :=
        (if Polarizations = 0
         then Interfaces.Integer_64'First
         elsif Polarizations = 1
         then 0
         else Interfaces.Integer_64'Last);
      Settings     : C2.Pulse_Detection_Settings :=
        C2.Create_Pulse_Detection_Settings (Reference, 255, 0, 0, 255, Width, Tag);
      Copy         : C2.Pulse_Detection_Settings;
      Command      : C2.Local_Function_Command :=
        C2.Create_Local_Function_Command
          (16#8000_0001#, Interfaces.Unsigned_64 (Interfaces.C.size_t'Last));
      Command_Copy : C2.Local_Function_Command;
   begin
      C2.Append_Polarization (Event, 1.0, -0.0, 2.25, -3.5);
      C2.Set_Event_Execution_Type (Event, C2.Conditional);
      C2.Set_Event_Termination_Type (Event, C2.Cancel_Event);
      C2.Set_Stab_Point_Index (Event, 999);
      C2.Append_Applicable_RX_Element_Group (Event, 2);
      C2.Append_Stab_Point (Interval, C2.Create_Face_Relative_Pointing (1.5, -0.5));
      C2.Append_Pulse_Detection_Threshold (Settings, 12.25, -3.5);
      C2.Append_Pulse_Detection_Threshold
        (Settings, IEEE (16#8000_0000_0000_0000#), IEEE (16#7FF0_0000_0000_0000#));
      C2.Append_Pulse_Detection_Threshold
        (Settings, IEEE (16#7FF8_0000_0000_0001#), IEEE (16#FFF0_0000_0000_0000#));
      C2.Append_Pulse_Detection_Threshold (Settings, 12.25, -3.5);
      Copy := Settings;
      C2.Append_Pulse_Detection_Threshold (Settings, 99.0, 88.0);
      C2.Set_Pulse_Detection_Settings (Event, Settings);
      C2.Set_Pulse_Detection_Settings (Event, Copy);
      C2.Clear_Pulse_Detection_Settings (Event);
      if not Clear then
         C2.Set_Pulse_Detection_Settings (Event, Copy);
      end if;
      if Enabled then
         C2.Set_Interval_Status_Enable (Interval, C2.Always);
      end if;
      C2.Append_RX_Event (Interval, Event);
      C2.Append_RX_Event (Interval, Event);
      if not Clear then
         C2.Append_Local_Function_Write (Command, 0, 0);
         C2.Append_Local_Function_Write
           (Command, Interfaces.Unsigned_64'Last, Interfaces.Unsigned_64'Last);
         Command_Copy := Command;
         C2.Append_Local_Function_Write (Command_Copy, 0, 7);
         C2.Append_Local_Function_Write (Command_Copy, 16#8000_0000_0000_0000#, 1);
         C2.Append_Interval_Local_Function_Command (Interval, Command);
         C2.Append_Interval_Local_Function_Command (Interval, Command);
         C2.Append_Interval_Local_Function_Command (Interval, Command_Copy);
         C2.Append_Local_Function_Write (Command, 42, 99);
         C2.Append_Interval_Local_Function_Command
           (Interval, C2.Create_Local_Function_Command (Interfaces.Unsigned_32'Last, 0));
         C2.Append_Interval_Local_Function_Command
           (Interval, C2.Create_Local_Function_Command (0, 1));
         C2.Append_Interval_Local_Function_Command
           (Interval, C2.Create_Local_Function_Command (1, 0));
         C2.Append_Interval_Local_Function_Command
           (Interval, C2.Create_Local_Function_Command (16#8000_0000#, 0));
      end if;
      declare
         Params : C2.Product_Stream_Params := C2.Create_Product_Stream_Params;
         Saved  : C2.Product_Stream_Params;
      begin
         C2.Append_Product_Stream_RX_Group (Params, 2);
         C2.Append_Product_Stream_RX_Group (Params, 0);
         C2.Append_Product_Stream_RX_Group (Params, 2);
         C2.Append_Product_Stream_Endpoint (Params, 0, 0, 0);
         C2.Append_Product_Stream_Endpoint
           (Params,
            Interfaces.Unsigned_64'Last,
            16#8000_0000_0000_0000#,
            Interfaces.Unsigned_64'Last);
         C2.Append_Product_Stream_Endpoint
           (Params,
            Interfaces.Unsigned_64'Last,
            16#8000_0000_0000_0000#,
            Interfaces.Unsigned_64'Last);
         C2.Append_Product_Stream_Endpoint (Params, 16#0123_4567_89AB_CDEF#, 1, 0);
         Saved := Params;
         C2.Append_Product_Stream_RX_Group (Params, 99);
         C2.Append_Product_Stream_Endpoint (Params, 99, 99, 99);
         C2.Set_Interval_Product_Stream_Params (Interval, Params);
         C2.Set_Interval_Product_Stream_Params (Interval, Saved);
         C2.Clear_Interval_Product_Stream_Params (Interval);
         if not Clear then
            C2.Set_Interval_Product_Stream_Params (Interval, Saved);
         end if;
         C2.Append_Product_Stream_RX_Group (Saved, 99);
         C2.Append_Product_Stream_Endpoint (Saved, 99, 99, 99);
      end;
      C2.Append_Job_Interval (List, Interval);
      C2.Append_Job_Interval (List, Interval);
      C2.Add_RX_Job_Intervals (Object, List);
      for I in Interfaces.C.unsigned range 0 .. 1 loop
         if Stream_Value (I, 0, 0) /= (if Clear then 0 else 3)
           or else Stream_Value (I, 0, 1) /= (if Clear then 0 else 4)
           or else Stream_Value (I, 0, 6) /= 0
           or else Stream_Value (I, 0, 7) /= 0
         then
            raise Program_Error with "product stream counts/defaults";
         end if;
         if not Clear then
            if Stream_Value (I, 0, 2) /= 2
              or else Stream_Value (I, 1, 2) /= 0
              or else Stream_Value (I, 2, 2) /= 2
              or else Stream_Value (I, 0, 3) /= 0
              or else Stream_Value (I, 0, 4) /= 0
              or else Stream_Value (I, 0, 5) /= 0
              or else Stream_Value (I, 1, 3) /= Interfaces.Unsigned_64'Last
              or else Stream_Value (I, 2, 3) /= Interfaces.Unsigned_64'Last
              or else Stream_Value (I, 1, 4) /= 16#8000_0000_0000_0000#
              or else Stream_Value (I, 2, 4) /= 16#8000_0000_0000_0000#
              or else Stream_Value (I, 1, 5) /= Interfaces.Unsigned_64'Last
              or else Stream_Value (I, 2, 5) /= Interfaces.Unsigned_64'Last
              or else Stream_Value (I, 3, 3) /= 16#0123_4567_89AB_CDEF#
              or else Stream_Value (I, 3, 4) /= 1
              or else Stream_Value (I, 3, 5) /= 0
            then
               raise Program_Error with "product stream copy/order/scalar fidelity";
            end if;
         end if;
         if Clear then
            if LF_Value (I, 0, 0, 0) /= 0 then
               raise Program_Error;
            end if;
         else
            if LF_Value (I, 0, 0, 0) /= 7
              or else LF_Value (I, 0, 0, 1) /= 16#8000_0001#
              or else LF_Value (I, 0, 0, 2) /= Interfaces.Unsigned_64 (Interfaces.C.size_t'Last)
              or else LF_Value (I, 0, 0, 3) /= 2
              or else LF_Value (I, 1, 0, 3) /= 2
              or else LF_Value (I, 2, 0, 3) /= 4
              or else LF_Value (I, 3, 0, 3) /= 0
              or else LF_Value (I, 0, 0, 4) /= 0
              or else LF_Value (I, 0, 0, 5) /= 0
              or else LF_Value (I, 0, 1, 4) /= Interfaces.Unsigned_64'Last
              or else LF_Value (I, 0, 1, 5) /= Interfaces.Unsigned_64'Last
              or else LF_Value (I, 2, 2, 4) /= 0
              or else LF_Value (I, 2, 2, 5) /= 7
              or else LF_Value (I, 2, 3, 4) /= 16#8000_0000_0000_0000#
              or else LF_Value (I, 2, 3, 5) /= 1
            then
               raise Program_Error with "LF value/copy fidelity";
            end if;
         end if;
      end loop;
      if Clear then
         Observe (0, 0, 0, 0);
      else
         Observe (Polarizations, Polarizations mod 2, Width, 4);
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
               Send_Empty (Object);
               Ada.Environment_Variables.Set
                 ("AMS_MEL_TEST_RF_JOB_FAILURE", "interval-product-stream-allocation");
               begin
                  Send (Object, False, 0);
                  raise Program_Error with "preparation failure accepted";
               exception
                  when AMS.MEL.Provider_Error =>
                     null;
               end;
               Ada.Environment_Variables.Clear ("AMS_MEL_TEST_RF_JOB_FAILURE");
               Send (Object, False, 0, True);
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
                  for Failure of Failures loop
                     Ada.Environment_Variables.Set
                       ("AMS_MEL_TEST_F4_ADD_FAILURE",
                        Ada.Strings.Fixed.Trim (Failure, Ada.Strings.Both));
                     begin
                        Send (Object, True, 2);
                        raise Program_Error with "provider failure accepted";
                     exception
                        when AMS.MEL.Provider_Error =>
                           null;
                     end;
                     Ada.Environment_Variables.Clear ("AMS_MEL_TEST_F4_ADD_FAILURE");
                     Send (Object, True, 2);
                  end loop;
                  Status.Close (Stream);
               end;
               C2.Close (Parent);
               C2.Close (VA);
               Send (Object, False, 2);
               C2.Close (Object);
            end;
            C2.Close (Request);
            C2.Close (VA);
         end;
         C2.Close (VA_Request);
         C2.Close (Parent);
      end;
   end loop;
   Ada.Text_IO.Put_Line ("PASS: safe Ada product stream focused 50/50");
end AMS_MEL_RF_Product_Stream_Focused;
