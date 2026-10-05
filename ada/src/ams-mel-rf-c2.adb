with Interfaces.C;
with Interfaces.C.Strings;
with Ada.Containers;
with Ada.Unchecked_Conversion;
with System;
with System.Storage_Elements;

package body AMS.MEL.RF.C2 is
   package C renames AMS.MEL_C_API;
   package CS renames Interfaces.C.Strings;
   --  IEEE NaN/infinity are intentional provider data in this command profile,
   --  not invalid Ada representations. GNAT's optional float validity checks
   --  would reject them before submission; disable only those checks here.
   pragma Validity_Checks ("F");
   use type Interfaces.Integer_32;
   use type C.RF_C2_Handle;
   use type C.RF_VA_Request_Handle;
   use type C.RF_VA_Handle;
   use type C.RF_Job_Request_Handle;
   use type C.RF_Job_Handle;
   use type C.Size_T;
   use type Interfaces.Unsigned_64;
   use type Interfaces.Unsigned_32;
   use type Interfaces.Integer_64;
   use type System.Address;
   use System.Storage_Elements;
   package US renames Ada.Strings.Unbounded;
   use type Interfaces.C.char;

   type Diagnostic is array (C.Size_T range <>) of aliased Interfaces.C.char with Convention => C;
   subtype Fixed_Diagnostic is Diagnostic (0 .. 511);
   type String_Owner is new Ada.Finalization.Limited_Controlled with record
      Value : CS.chars_ptr := CS.Null_Ptr;
   end record;
   overriding
   procedure Finalize (Value : in out String_Owner) is
   begin
      CS.Free (Value.Value);
   end Finalize;

   function Valid_String (Value : String) return Boolean is
   begin
      for Item of Value loop
         if Item = Character'Val (0) then
            return False;
         end if;
      end loop;
      return True;
   end Valid_String;

   function Copy_String (Value : C.String_View_V1) return String is
      type Char_Access is access all Interfaces.C.char;
      function To_Char is new Ada.Unchecked_Conversion (System.Address, Char_Access);
   begin
      if (Value.Size > 0 and then Value.Data = System.Null_Address)
        or else Interfaces.Unsigned_64 (Value.Size) > Interfaces.Unsigned_64 (Natural'Last)
      then
         raise Provider_Error with "invalid VA label span";
      end if;
      declare
         Result : String (1 .. Natural (Value.Size));
      begin
         for I in Result'Range loop
            Result (I) :=
              Character'Val
                (Interfaces.C.char'Pos (To_Char (Value.Data + Storage_Offset (I - 1)).all));
         end loop;
         return Result;
      end;
   end Copy_String;

   function Message (Buffer : Diagnostic) return String is
      Last : Natural := 0;
   begin
      while Last < Buffer'Length and then Buffer (C.Size_T (Last)) /= Interfaces.C.nul loop
         Last := Last + 1;
      end loop;
      declare
         Text : String (1 .. Last);
      begin
         for I in Text'Range loop
            Text (I) := Character'Val (Interfaces.C.char'Pos (Buffer (C.Size_T (I - 1))));
         end loop;
         return (if Last = 0 then "native RF C2 operation failed" else Text);
      end;
   end Message;

   procedure Check (Code : Interfaces.Integer_32; Buffer : Diagnostic) is
   begin
      if Code /= C.Success then
         raise Provider_Error with Message (Buffer);
      end if;
   end Check;

   function Create_RX_Receive_Event
     (Event_ID                    : Interfaces.Unsigned_32;
      Element_Group_Label         : String;
      Start_Femtoseconds          : Interfaces.Integer_64;
      Duration_Femtoseconds       : Interfaces.Integer_64;
      Center_Frequency_Hz         : Long_Float;
      Sample_Frequency_Hz         : Long_Float;
      AGC_Processing_Iterations   : Interfaces.Unsigned_64 := 0;
      Ignored_Post_AGC_Iterations : Interfaces.Unsigned_64 := 0;
      Max_Extension_Femtoseconds  : Interfaces.Integer_64 := 0) return RX_Receive_Event_Config is
   begin
      if not Valid_String (Element_Group_Label) then
         raise Constraint_Error with "Element_Group_Label contains embedded NUL";
      end if;
      return
        (Event_ID,
         US.To_Unbounded_String (Element_Group_Label),
         Start_Femtoseconds,
         Duration_Femtoseconds,
         Center_Frequency_Hz,
         Sample_Frequency_Hz,
         AGC_Processing_Iterations,
         Ignored_Post_AGC_Iterations,
         Max_Extension_Femtoseconds);
   end Create_RX_Receive_Event;
   function Create_RX_Job_Interval
     (Interval_ID                        : Interfaces.Unsigned_32;
      Sequence_Duration_Femtoseconds     : Interfaces.Integer_64;
      Job_Details_ID                     : Interfaces.Unsigned_32 := 0;
      Interval_Start_Femtoseconds        : Interfaces.Integer_64 :=
        Continue_From_Previous_Femtoseconds;
      Interval_Starting_Gap_Femtoseconds : Interfaces.Integer_64 := 0;
      Sequence_Repeat_Count              : Interfaces.Unsigned_64 := 1;
      Calibration_Duration_Femtoseconds  : Interfaces.Integer_64 := 0;
      Interval_Ending_Gap_Femtoseconds   : Interfaces.Integer_64 := 0;
      Phase_Coherence_With_Prior         : Boolean := False;
      Iterations_Per_Signal              : Interfaces.Unsigned_64 := 0;
      Max_Data_Rate_BPS                  : Long_Float := 0.0;
      Max_Sample_Rate_Hz                 : Long_Float := 0.0) return RX_Job_Interval_Config is
   begin
      return
        (Interval_ID,
         Job_Details_ID,
         Interval_Start_Femtoseconds,
         Interval_Starting_Gap_Femtoseconds,
         Sequence_Duration_Femtoseconds,
         Calibration_Duration_Femtoseconds,
         Interval_Ending_Gap_Femtoseconds,
         Sequence_Repeat_Count,
         Iterations_Per_Signal,
         Phase_Coherence_With_Prior,
         Max_Data_Rate_BPS,
         Max_Sample_Rate_Hz,
         RX_Event_Vectors.Empty_Vector,
         Never);
   end Create_RX_Job_Interval;
   procedure Set_Interval_Status_Enable
     (Interval : in out RX_Job_Interval_Config; Mode : Interval_Status_Enable) is
   begin
      Interval.Status_Enable := Mode;
   end Set_Interval_Status_Enable;
   procedure Append_RX_Event
     (Interval : in out RX_Job_Interval_Config; Event : RX_Receive_Event_Config) is
   begin
      Interval.Events.Append (Event);
   end Append_RX_Event;
   procedure Append_Job_Interval
     (Intervals : in out RX_Job_Interval_List; Interval : RX_Job_Interval_Config) is
   begin
      Intervals.Values.Append (Interval);
   end Append_Job_Interval;
   function Job_Interval_Count (Intervals : RX_Job_Interval_List) return Natural
   is (Natural (Intervals.Values.Length));

   procedure Add_RX_Job_Intervals (Object : in out Job; Intervals : RX_Job_Interval_List) is
      function Pointer_Address is new Ada.Unchecked_Conversion (CS.chars_ptr, System.Address);
      type Native_Intervals is array (Positive range <>) of aliased C.RF_Job_Interval_Config_V1
      with Convention => C;
      type Native_Events is array (Positive range <>) of aliased C.RF_Receive_Event_Config_V1
      with Convention => C;
      type Labels is array (Positive range <>) of String_Owner;
      type Native_Intervals_V2 is array (Positive range <>) of aliased C.RF_Job_Interval_Config_V2
      with Convention => C;
      Enabled     : Boolean := False;
      Count       : constant Natural := Job_Interval_Count (Intervals);
      Event_Count : Natural := 0;
      D           : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      R           : aliased C.Size_T := 0;
   begin
      for Interval of Intervals.Values loop
         Event_Count := Event_Count + Natural (Interval.Events.Length);
         Enabled := Enabled or Interval.Status_Enable /= Never;
      end loop;
      --  All backing arrays have their final sizes before any span is assigned.
      --  Controlled label owners release every allocation on return or exception.
      declare
         Configs    : Native_Intervals (1 .. Count);
         Configs_V2 : Native_Intervals_V2 (1 .. Count);
         Events     : Native_Events (1 .. Event_Count);
         Strings    : Labels (1 .. Event_Count);
         Next_Event : Positive := 1;
         I          : Positive := 1;
         Span       : C.RF_Job_Interval_Config_Span_V1 := (System.Null_Address, C.Size_T (Count));
      begin
         for Interval of Intervals.Values loop
            declare
               First_Event : constant Positive := Next_Event;
               Event_Span  : C.RF_Receive_Event_Config_Span_V1 :=
                 (System.Null_Address, C.Size_T (Interval.Events.Length));
            begin
               for Event of Interval.Events loop
                  Strings (Next_Event).Value := CS.New_String (US.To_String (Event.Label));
                  Events (Next_Event) :=
                    (Event.Event_ID,
                     (Pointer_Address (Strings (Next_Event).Value),
                      C.Size_T (US.Length (Event.Label))),
                     Event.Start_Femtoseconds,
                     Event.Duration_Femtoseconds,
                     Interfaces.C.double (Event.Center_Frequency_Hz),
                     Interfaces.C.double (Event.Sample_Frequency_Hz),
                     Event.AGC_Processing_Iterations,
                     Event.Ignored_Post_AGC_Iterations,
                     Event.Max_Extension_Femtoseconds);
                  Next_Event := Next_Event + 1;
               end loop;
               if Event_Span.Size > 0 then
                  Event_Span.Data := Events (First_Event)'Address;
               end if;
               Configs (I) :=
                 (Interval.Interval_Start_Femtoseconds,
                  Interval.Interval_ID,
                  Interval.Interval_Starting_Gap_Femtoseconds,
                  Interval.Sequence_Duration_Femtoseconds,
                  Interval.Sequence_Repeat_Count,
                  Interval.Calibration_Duration_Femtoseconds,
                  Interval.Interval_Ending_Gap_Femtoseconds,
                  Boolean'Pos (Interval.Phase_Coherence_With_Prior),
                  Interval.Iterations_Per_Signal,
                  Interfaces.C.double (Interval.Max_Data_Rate_BPS),
                  Interfaces.C.double (Interval.Max_Sample_Rate_Hz),
                  Interval.Job_Details_ID,
                  Event_Span);
               Configs_V2 (I) :=
                 (Configs (I), Interval_Status_Enable'Enum_Rep (Interval.Status_Enable));
               I := I + 1;
            end;
         end loop;
         if Count > 0 then
            Span.Data := Configs (1)'Address;
         end if;
         if Enabled then
            Check
              (C.RF_Job_Add_RX_Intervals_V2
                 (Object.Handle,
                  (Configs_V2 (1)'Address, C.Size_T (Count)),
                  D'Address,
                  D'Length,
                  R'Access),
               D);
         else
            Check
              (C.RF_Job_Add_RX_Intervals (Object.Handle, Span, D'Address, D'Length, R'Access), D);
         end if;
      end;
   end Add_RX_Job_Intervals;
   procedure Flush_Job (Object : in out Job) is
      D : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      R : aliased C.Size_T := 0;
   begin
      Check (C.RF_Job_Flush (Object.Handle, D'Address, D'Length, R'Access), D);
   end Flush_Job;
   procedure Cancel_Remaining_Job_Intervals (Object : in out Job) is
      D : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      R : aliased C.Size_T := 0;
   begin
      Check (C.RF_Job_Cancel_Remaining_Intervals (Object.Handle, D'Address, D'Length, R'Access), D);
   end Cancel_Remaining_Job_Intervals;
   procedure Extend_Job_Event
     (Object                      : in out Job;
      Interval_ID                 : Interfaces.Unsigned_32;
      Event_ID                    : Interfaces.Unsigned_32;
      Added_Duration_Femtoseconds : Interfaces.Integer_64)
   is
      D : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      R : aliased C.Size_T := 0;
   begin
      Check
        (C.RF_Job_Extend_Event
           (Object.Handle,
            Interval_ID,
            Event_ID,
            Added_Duration_Femtoseconds,
            D'Address,
            D'Length,
            R'Access),
         D);
   end Extend_Job_Event;

   function Open (Library_Path : String; Configuration : String) return C2_MEL is
      Library_C, Configuration_C : String_Owner;
      D                          : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      R                          : aliased C.Size_T := 0;
   begin
      for Item of Library_Path loop
         if Item = Character'Val (0) then
            raise Constraint_Error with "Library_Path contains an embedded NUL";
         end if;
      end loop;
      for Item of Configuration loop
         if Item = Character'Val (0) then
            raise Constraint_Error with "Configuration contains an embedded NUL";
         end if;
      end loop;
      Library_C.Value := CS.New_String (Library_Path);
      Configuration_C.Value := CS.New_String (Configuration);
      return Result : C2_MEL do
         Check
           (C.RF_C2_Open
              (Library_C.Value,
               Configuration_C.Value,
               Result.Handle'Access,
               D'Address,
               D'Length,
               R'Access),
            D);
      end return;
   end Open;

   function Is_Open (Object : C2_MEL) return Boolean
   is (Object.Handle /= C.Null_RF_C2);

   procedure Close (Object : in out C2_MEL) is
      D : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      R : aliased C.Size_T := 0;
   begin
      Check (C.RF_C2_Close (Object.Handle'Access, D'Address, D'Length, R'Access), D);
   end Close;

   overriding
   procedure Finalize (Object : in out C2_MEL) is
      Ignored : Interfaces.Integer_32;
   begin
      Ignored := C.RF_C2_Close (Object.Handle'Access, System.Null_Address, 0, null);
   exception
      when others =>
         Object.Handle := C.Null_RF_C2;
   end Finalize;
   function Create_Virtual_Aperture_Config
     (VA_Definition_ID        : Interfaces.Unsigned_32;
      Priority                : Interfaces.Unsigned_32;
      VA_Definition_File_Info : String) return Virtual_Aperture_Config is
   begin
      if not Valid_String (VA_Definition_File_Info) then
         raise Constraint_Error with "invalid VA definition file info";
      end if;
      return
        (ID           => VA_Definition_ID,
         Priority     => Priority,
         File_Info    => US.To_Unbounded_String (VA_Definition_File_Info),
         Local        => Text_Vectors.Empty_Vector,
         Capabilities => UCI_Vectors.Empty_Vector);
   end Create_Virtual_Aperture_Config;

   procedure Append_Local_Function_Info (Config : in out Virtual_Aperture_Config; Value : String) is
   begin
      if not Valid_String (Value) then
         raise Constraint_Error with "invalid local function info";
      end if;
      Config.Local.Append (US.To_Unbounded_String (Value));
   end Append_Local_Function_Info;

   procedure Append_Capability_ID
     (Config : in out Virtual_Aperture_Config; Value : AMS.MEL.IR.UCI_ID) is
   begin
      if not Valid_String (AMS.MEL.IR.Descriptive_Label (Value)) then
         raise Constraint_Error with "invalid capability label";
      end if;
      Config.Capabilities.Append (Value);
   end Append_Capability_ID;

   function To_View (Value : String) return C.String_View_V1 is
   begin
      return
        (Data => (if Value'Length = 0 then System.Null_Address else Value'Address),
         Size => C.Size_T (Value'Length));
   end To_View;

   function Submit_Virtual_Aperture
     (Parent : C2_MEL'Class; Config : Virtual_Aperture_Config) return Virtual_Aperture_Request
   is
      type String_Owners is array (Positive range <>) of String_Owner;
      type Raw_IDs is array (Positive range <>) of aliased C.UCI_ID_V1 with Convention => C;
      function Pointer_Address is new Ada.Unchecked_Conversion (CS.chars_ptr, System.Address);
      Local_Values     : String_Owners (1 .. Natural (Config.Local.Length));
      Labels           : String_Owners (1 .. Natural (Config.Capabilities.Length));
      Raw_Local        : C.String_View_Array (1 .. Natural (Config.Local.Length));
      Raw_Capabilities : Raw_IDs (1 .. Natural (Config.Capabilities.Length));
      File_Value       : constant String := US.To_String (Config.File_Info);
      D                : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      R                : aliased C.Size_T := 0;
   begin
      for I in Local_Values'Range loop
         declare
            Value : constant String := US.To_String (Config.Local (I));
         begin
            Local_Values (I).Value := CS.New_String (Value);
            Raw_Local (I) := (Pointer_Address (Local_Values (I).Value), C.Size_T (Value'Length));
         end;
      end loop;
      for I in Labels'Range loop
         declare
            Bytes : constant AMS.MEL.IR.UUID := AMS.MEL.IR.UUID_Value (Config.Capabilities (I));
         begin
            declare
               Label : constant String := AMS.MEL.IR.Descriptive_Label (Config.Capabilities (I));
            begin
               Labels (I).Value := CS.New_String (Label);
               Raw_Capabilities (I).Descriptive_Label :=
                 (Pointer_Address (Labels (I).Value), C.Size_T (Label'Length));
            end;
            for B in Bytes'Range loop
               Raw_Capabilities (I).UUID (B) := Bytes (B);
            end loop;
         end;
      end loop;
      declare
         Raw : aliased constant C.RF_VA_Config_V1 :=
           (VA_Definition_ID        => Config.ID,
            Priority                => Config.Priority,
            Local_Function_Info     =>
              (Data =>
                 (if Raw_Local'Length = 0
                  then System.Null_Address
                  else Raw_Local (Raw_Local'First)'Address),
               Size => C.Size_T (Raw_Local'Length)),
            VA_Definition_File_Info => To_View (File_Value),
            Capability_IDs          =>
              (Data =>
                 (if Raw_Capabilities'Length = 0
                  then System.Null_Address
                  else Raw_Capabilities (Raw_Capabilities'First)'Address),
               Size => C.Size_T (Raw_Capabilities'Length)));
      begin
         return Result : Virtual_Aperture_Request do
            Check
              (C.RF_C2_Submit_VA
                 (Parent.Handle, Raw'Access, Result.Handle'Access, D'Address, D'Length, R'Access),
               D);
         end return;
      end;
   end Submit_Virtual_Aperture;

   function Is_Open (Request : Virtual_Aperture_Request) return Boolean
   is (Request.Handle /= C.Null_RF_VA_Request);
   procedure Close (Request : in out Virtual_Aperture_Request) is
      D : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      R : aliased C.Size_T := 0;
   begin
      Check (C.RF_VA_Request_Close (Request.Handle'Access, D'Address, D'Length, R'Access), D);
   end Close;
   overriding
   procedure Finalize (Request : in out Virtual_Aperture_Request) is
      Ignored : Interfaces.Integer_32;
   begin
      Ignored := C.RF_VA_Request_Close (Request.Handle'Access, System.Null_Address, 0, null);
   exception
      when others =>
         Request.Handle := C.Null_RF_VA_Request;
   end Finalize;
   function Outcome (Result : Virtual_Aperture_Result) return Request_Outcome
   is (Result.State);
   function Error_Code (Result : Virtual_Aperture_Result) return Request_Error_Code
   is (Result.Code);
   function Description (Result : Virtual_Aperture_Result) return String
   is (US.To_String (Result.Text));

   function Wait
     (Request : Virtual_Aperture_Request; Timeout_Milliseconds : Natural)
      return Virtual_Aperture_Result
   is
      D      : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      R      : aliased C.Size_T := 0;
      Raw    : aliased C.RF_VA_Result_V1 := (Error_Code => 0);
      Status : Interfaces.Integer_32;
   begin
      Status :=
        C.RF_VA_Request_Wait
          (Request.Handle,
           Interfaces.Unsigned_32 (Timeout_Milliseconds),
           Raw'Access,
           D'Address,
           D'Length,
           R'Access);
      if Status = C.Timeout then
         raise Timeout_Error;
      end if;
      if Status = C.Success then
         if Raw.Error_Code /= 0 then
            raise Provider_Error with "invalid successful VA result";
         end if;
         return (State => Created, Code => None, Text => US.Null_Unbounded_String);
      elsif Status = C.Provider_Failed then
         if Raw.Error_Code > 8 then
            raise Provider_Error with Message (D);
         end if;
         if R = 0 or else Interfaces.Unsigned_64 (R) > Interfaces.Unsigned_64 (Natural'Last) then
            raise Provider_Error with "invalid VA diagnostic size";
         end if;
         if R > D'Length then
            declare
               Full  : aliased Diagnostic (0 .. R - 1) := [others => Interfaces.C.nul];
               Again : aliased C.RF_VA_Result_V1 := (Error_Code => 0);
               Need  : aliased C.Size_T := 0;
               Retry : constant Interfaces.Integer_32 :=
                 C.RF_VA_Request_Wait
                   (Request.Handle, 0, Again'Access, Full'Address, Full'Length, Need'Access);
            begin
               if Retry /= Status or else Again.Error_Code /= Raw.Error_Code or else Need /= R then
                  raise Provider_Error with "VA terminal result changed";
               end if;
               return
                 (State => Failed,
                  Code  => Request_Error_Code'Enum_Val (Raw.Error_Code),
                  Text  => US.To_Unbounded_String (Message (Full)));
            end;
         end if;
         return
           (State => Failed,
            Code  => Request_Error_Code'Enum_Val (Raw.Error_Code),
            Text  => US.To_Unbounded_String (Message (D)));
      end if;
      raise Provider_Error with Message (D);
   end Wait;

   type Info_Access is access all C.RF_VA_Info_V1;
   function To_Info is new Ada.Unchecked_Conversion (System.Address, Info_Access);
   type U32_Access is access all Interfaces.Unsigned_32;
   function To_U32 is new Ada.Unchecked_Conversion (System.Address, U32_Access);
   type String_Access is access all C.String_View_V1;
   function To_String_View is new Ada.Unchecked_Conversion (System.Address, String_Access);
   function Claim (Request : Virtual_Aperture_Request'Class) return Virtual_Aperture is
      D       : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      R       : aliased C.Size_T := 0;
      Address : aliased System.Address := System.Null_Address;
   begin
      return Result : Virtual_Aperture do
         Check
           (C.RF_VA_Request_Claim
              (Request.Handle, Result.Handle'Access, D'Address, D'Length, R'Access),
            D);
         Check (C.RF_VA_View (Result.Handle, Address'Access, D'Address, D'Length, R'Access), D);
         if Address = System.Null_Address then
            raise Provider_Error with "null VA snapshot";
         end if;
         declare
            Info : constant C.RF_VA_Info_V1 := To_Info (Address).all;
         begin
            if (Info.VA_Instance_IDs.Size > 0
                and then Info.VA_Instance_IDs.Data = System.Null_Address)
              or else (Info.Element_Group_Labels.Size > 0
                       and then Info.Element_Group_Labels.Data = System.Null_Address)
              or else Interfaces.Unsigned_64 (Info.VA_Instance_IDs.Size)
                      > Interfaces.Unsigned_64 (Natural'Last)
              or else Interfaces.Unsigned_64 (Info.Element_Group_Labels.Size)
                      > Interfaces.Unsigned_64 (Natural'Last)
              or else Info.Is_Single_Group > 1
            then
               raise Provider_Error with "invalid VA snapshot";
            end if;
            Result.Single := Info.Is_Single_Group = 1;
            for I in 1 .. Natural (Info.VA_Instance_IDs.Size) loop
               Result.IDs.Append
                 (To_U32
                    (Info.VA_Instance_IDs.Data
                     + Storage_Offset
                         ((I - 1) * Interfaces.Unsigned_32'Object_Size / System.Storage_Unit)).all);
            end loop;
            for I in 1 .. Natural (Info.Element_Group_Labels.Size) loop
               declare
                  Raw : constant C.String_View_V1 :=
                    To_String_View
                      (Info.Element_Group_Labels.Data
                       + Storage_Offset
                           ((I - 1) * C.String_View_V1'Object_Size / System.Storage_Unit)).all;
               begin
                  Result.Labels.Append (US.To_Unbounded_String (Copy_String (Raw)));
               end;
            end loop;
         end;
      end return;
   end Claim;
   function Is_Open (Object : Virtual_Aperture) return Boolean
   is (Object.Handle /= C.Null_RF_VA);
   function VA_Instance_ID_Count (Object : Virtual_Aperture) return Natural
   is (Natural (Object.IDs.Length));
   function VA_Instance_ID_At
     (Object : Virtual_Aperture; Index : Positive) return Interfaces.Unsigned_32
   is (Object.IDs (Index));
   function Element_Group_Label_Count (Object : Virtual_Aperture) return Natural
   is (Natural (Object.Labels.Length));
   function Element_Group_Label_At (Object : Virtual_Aperture; Index : Positive) return String
   is (US.To_String (Object.Labels (Index)));
   function Is_Single_Group (Object : Virtual_Aperture) return Boolean
   is (Object.Single);
   procedure Close (Object : in out Virtual_Aperture) is
      D : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      R : aliased C.Size_T := 0;
   begin
      Check (C.RF_VA_Close (Object.Handle'Access, D'Address, D'Length, R'Access), D);
   end Close;
   overriding
   procedure Finalize (Object : in out Virtual_Aperture) is
      Ignored : Interfaces.Integer_32;
   begin
      Ignored := C.RF_VA_Close (Object.Handle'Access, System.Null_Address, 0, null);
   exception
      when others =>
         Object.Handle := C.Null_RF_VA;
   end Finalize;

   function Finite (Value : Long_Float) return Boolean
   is (Value <= Long_Float'Last and then Value >= Long_Float'First);

   function Create_TX_Element_Group
     (Label               : String;
      TX_Power_Level      : Interfaces.Unsigned_32;
      Desired_Duty_Factor : Long_Float := 1.0) return TX_Element_Group_Config is
   begin
      if not Valid_String (Label)
        or else not Finite (Desired_Duty_Factor)
        or else Desired_Duty_Factor <= 0.0
        or else Desired_Duty_Factor > 1.0
      then
         raise Constraint_Error with "invalid TX group";
      end if;
      return
        (US.To_Unbounded_String (Label),
         TX_Power_Level,
         Desired_Duty_Factor,
         Frequency_Vectors.Empty_Vector);
   end Create_TX_Element_Group;
   procedure Append_Expected_Center_Frequency
     (Group : in out TX_Element_Group_Config; Min_Hz, Max_Hz : Long_Float) is
   begin
      if not Finite (Min_Hz) or else not Finite (Max_Hz) or else Min_Hz > Max_Hz then
         raise Constraint_Error with "invalid frequency range";
      end if;
      Group.Frequencies.Append (Frequency_Range'(Min_Hz, Max_Hz));
   end Append_Expected_Center_Frequency;
   function Create_RX_Element_Group
     (Label               : String;
      Desired_Duty_Factor : Long_Float := 1.0;
      Data_Pipe_Label     : String := "default") return RX_Element_Group_Config is
   begin
      if not Valid_String (Label)
        or else not Valid_String (Data_Pipe_Label)
        or else not Finite (Desired_Duty_Factor)
        or else Desired_Duty_Factor <= 0.0
        or else Desired_Duty_Factor > 1.0
      then
         raise Constraint_Error with "invalid RX group";
      end if;
      return
        (Label       => US.To_Unbounded_String (Label),
         Pipe        => US.To_Unbounded_String (Data_Pipe_Label),
         Duty        => Desired_Duty_Factor,
         Frequencies => <>,
         Pipes       => <>,
         Pointings   => <>);
   end Create_RX_Element_Group;
   procedure Append_Expected_Center_Frequency
     (Group : in out RX_Element_Group_Config; Min_Hz, Max_Hz : Long_Float) is
   begin
      if not Finite (Min_Hz) or else not Finite (Max_Hz) or else Min_Hz > Max_Hz then
         raise Constraint_Error with "invalid frequency range";
      end if;
      Group.Frequencies.Append (Frequency_Range'(Min_Hz, Max_Hz));
   end Append_Expected_Center_Frequency;
   procedure Append_Endpoint_ID
     (Group : in out RX_Element_Group_Config; ID : Interfaces.Unsigned_64) is
   begin
      Append_Endpoint_ID (Group, US.To_String (Group.Pipe), ID);
   end Append_Endpoint_ID;
   procedure Append_Endpoint_ID
     (Group : in out RX_Element_Group_Config; Data_Pipe_Label : String; ID : Interfaces.Unsigned_64)
   is
   begin
      if not Valid_String (Data_Pipe_Label) then
         raise Constraint_Error with "invalid pipe label";
      end if;
      for I in Group.Pipes.First_Index .. Group.Pipes.Last_Index loop
         if US.To_String (Group.Pipes (I).Label) = Data_Pipe_Label then
            for Existing of Group.Pipes (I).Endpoints loop
               if Existing = ID then
                  raise Constraint_Error with "duplicate endpoint ID";
               end if;
            end loop;
            Group.Pipes (I).Endpoints.Append (ID);
            return;
         end if;
      end loop;
      declare
         Pipe : Pipe_Config := (Label => US.To_Unbounded_String (Data_Pipe_Label), Endpoints => <>);
      begin
         Pipe.Endpoints.Append (ID);
         Group.Pipes.Append (Pipe);
      end;
   end Append_Endpoint_ID;
   function Create_UTC_Time
     (Seconds, Fractional_Femtoseconds : Interfaces.Integer_64) return UTC_Time is
   begin
      if Fractional_Femtoseconds < 0 or else Fractional_Femtoseconds >= 1_000_000_000_000_000 then
         raise Constraint_Error with "noncanonical UTC fraction";
      end if;
      return (Seconds, Fractional_Femtoseconds);
   end Create_UTC_Time;
   function Seconds (Value : UTC_Time) return Interfaces.Integer_64
   is (Value.Seconds);
   function Fractional_Femtoseconds (Value : UTC_Time) return Interfaces.Integer_64
   is (Value.Fraction);
   function Create_ECEF_Pointing
     (Location_X_M, Location_Y_M, Location_Z_M       : Long_Float;
      Velocity_X_MPS, Velocity_Y_MPS, Velocity_Z_MPS : Long_Float;
      Time_Of_Validity                               : UTC_Time) return Pointing
   is
      pragma Validity_Checks ("F");
   begin
      return
        (ECEF,
         [Location_X_M, Location_Y_M, Location_Z_M, Velocity_X_MPS, Velocity_Y_MPS, Velocity_Z_MPS],
         Time_Of_Validity);
   end Create_ECEF_Pointing;
   function Create_LLA_Pointing
     (Latitude_Rad, Longitude_Rad, Altitude_M                  : Long_Float;
      Velocity_North_MPS, Velocity_East_MPS, Velocity_Down_MPS : Long_Float;
      Time_Of_Validity                                         : UTC_Time) return Pointing
   is
      pragma Validity_Checks ("F");
   begin
      return
        (LLA,
         [Latitude_Rad,
          Longitude_Rad,
          Altitude_M,
          Velocity_North_MPS,
          Velocity_East_MPS,
          Velocity_Down_MPS],
         Time_Of_Validity);
   end Create_LLA_Pointing;
   function Create_Platform_Relative_Pointing
     (Azimuth_Rad, Elevation_Rad : Long_Float) return Pointing
   is
      pragma Validity_Checks ("F");
   begin
      return (Platform_Relative, [Azimuth_Rad, Elevation_Rad, others => 0.0], (0, 0));
   end Create_Platform_Relative_Pointing;
   function Create_Face_Relative_Pointing (Azimuth_Rad, Elevation_Rad : Long_Float) return Pointing
   is
      pragma Validity_Checks ("F");
   begin
      return (Face_Relative, [Azimuth_Rad, Elevation_Rad, others => 0.0], (0, 0));
   end Create_Face_Relative_Pointing;
   function Create_Baseline_Relative_Pointing (Conic_Rad : Long_Float) return Pointing is
      pragma Validity_Checks ("F");
   begin
      return (Baseline_Relative, [Conic_Rad, others => 0.0], (0, 0));
   end Create_Baseline_Relative_Pointing;
   procedure Append_Expected_Pointing (Group : in out RX_Element_Group_Config; Value : Pointing) is
   begin
      Group.Pointings.Append (Value);
   end Append_Expected_Pointing;
   procedure Set_Estimated_Stab_Point (Config : in out Job_Config; Value : Pointing) is
   begin
      Config.Estimated_Point := Value;
      Config.Has_Estimated_Point := True;
   end Set_Estimated_Stab_Point;
   procedure Clear_Estimated_Stab_Point (Config : in out Job_Config) is
   begin
      Config.Has_Estimated_Point := False;
   end Clear_Estimated_Stab_Point;
   function Raw_Pointing (Value : Pointing) return C.RF_Pointing_V1 is
      pragma Validity_Checks ("F");
      pragma
        Compile_Time_Error
          (Long_Float'Digits < Interfaces.C.double'Digits
             or else Long_Float'Machine_Mantissa < Interfaces.C.double'Machine_Mantissa
             or else Long_Float'Machine_Emax < Interfaces.C.double'Machine_Emax
             or else Long_Float'Machine_Emin > Interfaces.C.double'Machine_Emin
             or else Long_Float'Machine_Radix /= Interfaces.C.double'Machine_Radix,
           "Pointing requires Long_Float to represent normal C double values");
      function To_C (Number : Long_Float) return Interfaces.C.double is
         pragma Validity_Checks ("F");
      begin
         return Interfaces.C.double (Number);
      end To_C;
      Numbers : Pointing_Components renames Value.Components;
      Result  : C.RF_Pointing_V1 :=
        (Kind                        => Interfaces.Unsigned_32 (Pointing_Kind'Pos (Value.Kind)),
         ECEF                        => ((0.0, 0.0, 0.0), (0.0, 0.0, 0.0), (0, 0)),
         LLA                         => (0.0, 0.0, 0.0, 0.0, 0.0, 0.0, (0, 0)),
         Platform_Relative           => (0.0, 0.0),
         Face_Relative               => (0.0, 0.0),
         Baseline_Relative_Conic_Rad => 0.0);
   begin
      case Value.Kind is
         when ECEF              =>
            Result.ECEF :=
              ((To_C (Numbers (1)), To_C (Numbers (2)), To_C (Numbers (3))),
               (To_C (Numbers (4)), To_C (Numbers (5)), To_C (Numbers (6))),
               (Value.Time.Seconds, Value.Time.Fraction));

         when LLA               =>
            Result.LLA :=
              (To_C (Numbers (1)),
               To_C (Numbers (2)),
               To_C (Numbers (3)),
               To_C (Numbers (4)),
               To_C (Numbers (5)),
               To_C (Numbers (6)),
               (Value.Time.Seconds, Value.Time.Fraction));

         when Platform_Relative =>
            Result.Platform_Relative := (To_C (Numbers (1)), To_C (Numbers (2)));

         when Face_Relative     =>
            Result.Face_Relative := (To_C (Numbers (1)), To_C (Numbers (2)));

         when Baseline_Relative =>
            Result.Baseline_Relative_Conic_Rad := To_C (Numbers (1));
      end case;
      return Result;
   end Raw_Pointing;
   procedure Append_RX_Element_Group (Config : in out Job_Config; Group : RX_Element_Group_Config)
   is
   begin
      Config.Groups.Append (Job_Group'(RX, Group));
   end Append_RX_Element_Group;
   procedure Append_TX_Element_Group (Config : in out Job_Config; Group : TX_Element_Group_Config)
   is
   begin
      Config.Groups.Append (Job_Group'(TX, Group));
   end Append_TX_Element_Group;
   procedure Set_Min_Start_Time (Config : in out Job_Config; Value : UTC_Time) is
   begin
      Config.Min_Start := Value;
   end Set_Min_Start_Time;
   procedure Set_Max_Complete_Time (Config : in out Job_Config; Value : UTC_Time) is
   begin
      Config.Max_Complete := Value;
   end Set_Max_Complete_Time;
   procedure Set_Duration_Femtoseconds (Config : in out Job_Config; Value : Interfaces.Integer_64)
   is
   begin
      Config.Duration := Value;
   end Set_Duration_Femtoseconds;
   procedure Set_Capability_ID (Config : in out Job_Config; Value : Byte_Array) is
      Bytes : Byte_Vectors.Vector;
   begin
      for Item of Value loop
         Bytes.Append (Item);
      end loop;
      Config.Capability := Bytes;
   end Set_Capability_ID;
   procedure Set_Activity_ID (Config : in out Job_Config; Value : Byte_Array) is
      Bytes : Byte_Vectors.Vector;
   begin
      for Item of Value loop
         Bytes.Append (Item);
      end loop;
      Config.Activity := Bytes;
   end Set_Activity_ID;
   procedure Set_TX_Power_Mode_IDs (Config : in out Job_Config; Value : Unsigned_32_Array) is
      IDs : ID_Vectors.Vector;
   begin
      for Item of Value loop
         IDs.Append (Item);
      end loop;
      Config.Power_Modes := IDs;
   end Set_TX_Power_Mode_IDs;
   procedure Set_Lookahead_Femtoseconds (Config : in out Job_Config; Value : Interfaces.Integer_64)
   is
   begin
      Config.Lookahead := Value;
   end Set_Lookahead_Femtoseconds;
   function Create_Job_Config
     (Request_ID, Priority       : Interfaces.Unsigned_32;
      Group                      : RX_Element_Group_Config;
      Precedence_Within_Priority : Interfaces.Unsigned_32 := 0;
      Interruptable              : Boolean := False) return Job_Config is
   begin
      return
         Result : Job_Config :=
           (ID            => Request_ID,
            Priority      => Priority,
            Precedence    => Precedence_Within_Priority,
            Interruptable => Interruptable,
            Groups        => <>,
            Instances     => <>,
            others        => <>)
      do
         Append_RX_Element_Group (Result, Group);
         Result.Capability.Append (0);
         Result.Activity.Append (0);
         Result.Power_Modes.Append (0);
      end return;
   end Create_Job_Config;
   function Create_Job_Config
     (Request_ID, Priority       : Interfaces.Unsigned_32;
      Group                      : TX_Element_Group_Config;
      Precedence_Within_Priority : Interfaces.Unsigned_32 := 0;
      Interruptable              : Boolean := False) return Job_Config is
   begin
      return
         Result : Job_Config :=
           (ID            => Request_ID,
            Priority      => Priority,
            Precedence    => Precedence_Within_Priority,
            Interruptable => Interruptable,
            others        => <>)
      do
         Append_TX_Element_Group (Result, Group);
         Result.Capability.Append (0);
         Result.Activity.Append (0);
         Result.Power_Modes.Append (0);
      end return;
   end Create_Job_Config;
   procedure Append_Instance_Selection (Config : in out Job_Config; ID : Interfaces.Unsigned_32) is
   begin
      Config.Instances.Append (ID);
   end Append_Instance_Selection;
   function Submit_Job (VA : Virtual_Aperture'Class; Config : Job_Config) return Job_Request is
      function Pointer_Address is new Ada.Unchecked_Conversion (CS.chars_ptr, System.Address);
      type Raw_Frequencies is array (Positive range <>) of aliased C.RF_Frequency_Range_V1
      with Convention => C;
      type Raw_Endpoints is array (Positive range <>) of aliased Interfaces.Unsigned_64
      with Convention => C;
      type Raw_Instances is array (Positive range <>) of aliased Interfaces.Unsigned_32
      with Convention => C;
      type Raw_Bytes is array (Positive range <>) of aliased Interfaces.Unsigned_8
      with Convention => C;
      type Raw_Groups is array (Positive range <>) of aliased C.RF_Job_Element_Group_Config_V4
      with Convention => C;
      type Raw_Pointings is array (Positive range <>) of aliased C.RF_Pointing_V1
      with Convention => C;
      type Raw_Pipes is array (Positive range <>) of aliased C.RF_RX_Data_Pipe_Endpoint_Config_V1
      with Convention => C;
      type String_Owners is array (Positive range <>) of String_Owner;
      Frequency_Count, Pipe_Count, Endpoint_Count, Pointing_Count : Natural := 0;
      Instances                                                   :
        Raw_Instances (1 .. Natural (Config.Instances.Length));
      Modes                                                       :
        Raw_Instances (1 .. Natural (Config.Power_Modes.Length));
      Capability                                                  :
        Raw_Bytes (1 .. Natural (Config.Capability.Length));
      Activity                                                    :
        Raw_Bytes (1 .. Natural (Config.Activity.Length));
      D                                                           : aliased Fixed_Diagnostic :=
        [others => Interfaces.C.nul];
      R                                                           : aliased C.Size_T := 0;
   begin
      if not Is_Open (VA) then
         raise Provider_Error with "VirtualAperture is closed";
      end if;
      for Entry_Value of Config.Groups loop
         if Entry_Value.Mode = TX then
            Frequency_Count :=
              Frequency_Count + Natural (Entry_Value.Transmit_Group.Frequencies.Length);
         else
            declare
               Group : RX_Element_Group_Config renames Entry_Value.Receive_Group;
            begin
               Frequency_Count := Frequency_Count + Natural (Group.Frequencies.Length);
               Pointing_Count := Pointing_Count + Natural (Group.Pointings.Length);
               for Pipe of Group.Pipes loop
                  if not Pipe.Endpoints.Is_Empty then
                     Pipe_Count := Pipe_Count + 1;
                     Endpoint_Count := Endpoint_Count + Natural (Pipe.Endpoints.Length);
                  end if;
               end loop;
            end;
         end if;
      end loop;
      for I in Instances'Range loop
         Instances (I) := Config.Instances (I);
      end loop;
      for I in Modes'Range loop
         Modes (I) := Config.Power_Modes (I);
      end loop;
      for I in Capability'Range loop
         Capability (I) := Config.Capability (I);
      end loop;
      for I in Activity'Range loop
         Activity (I) := Config.Activity (I);
      end loop;
      declare
         --  Final-sized backing arrays precede all pointer publication. Controlled
         --  strings clean up even when serialization or native submission raises.
         --  Inactive native payloads are never read; initialize the full backing
         --  deterministically nevertheless, rather than pass uninitialized bytes.
         Groups        : Raw_Groups (1 .. Natural (Config.Groups.Length)) :=
           [others =>
              (Mode => C.RF_Element_Group_Mode_RX,
               RX   =>
                 (Group                    =>
                    ((System.Null_Address, 0),
                     0.0,
                     (System.Null_Address, 0),
                     (System.Null_Address, 0)),
                  Expected_Pointing_Angles => (System.Null_Address, 0)),
               TX   => ((System.Null_Address, 0), 0, 0.0, (System.Null_Address, 0)))];
         Pipes         : Raw_Pipes (1 .. Pipe_Count);
         Frequencies   : Raw_Frequencies (1 .. Frequency_Count);
         Endpoints     : Raw_Endpoints (1 .. Endpoint_Count);
         Pointings     : Raw_Pointings (1 .. Pointing_Count);
         Labels        : String_Owners (Groups'Range);
         Pipe_Labels   : String_Owners (Pipes'Range);
         G, F, P, E, Q : Positive := 1;
         Raw           : aliased C.RF_Job_Request_Config_V4 :=
           (Request_ID                 => Config.ID,
            Priority                   => Config.Priority,
            Precedence_Within_Priority => Config.Precedence,
            Is_Interruptable           => (if Config.Interruptable then 1 else 0),
            Instance_Selection         =>
              (Data =>
                 (if Instances'Length = 0
                  then System.Null_Address
                  else Instances (Instances'First)'Address),
               Size => Instances'Length),
            Element_Groups             => (System.Null_Address, C.Size_T (Groups'Length)),
            Min_Start_Time             => (Config.Min_Start.Seconds, Config.Min_Start.Fraction),
            Max_Complete_Time          =>
              (Config.Max_Complete.Seconds, Config.Max_Complete.Fraction),
            Duration_Femtoseconds      => Config.Duration,
            Capability_ID              =>
              ((if Capability'Length = 0 then System.Null_Address else Capability (1)'Address),
               C.Size_T (Capability'Length)),
            Activity_ID                =>
              ((if Activity'Length = 0 then System.Null_Address else Activity (1)'Address),
               C.Size_T (Activity'Length)),
            TX_Power_Mode_IDs          =>
              ((if Modes'Length = 0 then System.Null_Address else Modes (1)'Address),
               C.Size_T (Modes'Length)),
            Lookahead_Femtoseconds     => Config.Lookahead,
            Has_Estimated_Stab_Point   => (if Config.Has_Estimated_Point then 1 else 0),
            Estimated_Stab_Point       => Raw_Pointing (Pointing'(others => <>)));
      begin
         if Config.Has_Estimated_Point then
            Raw.Estimated_Stab_Point := Raw_Pointing (Config.Estimated_Point);
         end if;
         for Entry_Value of Config.Groups loop
            if Entry_Value.Mode = TX then
               declare
                  Group   : TX_Element_Group_Config renames Entry_Value.Transmit_Group;
                  First_F : constant Positive := F;
               begin
                  Labels (G).Value := CS.New_String (US.To_String (Group.Label));
                  for Range_Value of Group.Frequencies loop
                     Frequencies (F) :=
                       (Interfaces.C.double (Range_Value.Min_Hz),
                        Interfaces.C.double (Range_Value.Max_Hz));
                     F := F + 1;
                  end loop;
                  Groups (G).Mode := C.RF_Element_Group_Mode_TX;
                  Groups (G).TX :=
                    ((Pointer_Address (Labels (G).Value), C.Size_T (US.Length (Group.Label))),
                     Group.Power,
                     Interfaces.C.double (Group.Duty),
                     ((if F = First_F then System.Null_Address else Frequencies (First_F)'Address),
                      C.Size_T (F - First_F)));
                  G := G + 1;
               end;
            else
               declare
                  Group   : RX_Element_Group_Config renames Entry_Value.Receive_Group;
                  First_F : constant Positive := F;
                  First_P : constant Positive := P;
                  First_Q : constant Positive := Q;
               begin
                  Labels (G).Value := CS.New_String (US.To_String (Group.Label));
                  for Range_Value of Group.Frequencies loop
                     Frequencies (F) :=
                       (Interfaces.C.double (Range_Value.Min_Hz),
                        Interfaces.C.double (Range_Value.Max_Hz));
                     F := F + 1;
                  end loop;
                  for Pipe of Group.Pipes loop
                     if not Pipe.Endpoints.Is_Empty then
                        declare
                           First_E : constant Positive := E;
                        begin
                           Pipe_Labels (P).Value := CS.New_String (US.To_String (Pipe.Label));
                           for ID of Pipe.Endpoints loop
                              Endpoints (E) := ID;
                              E := E + 1;
                           end loop;
                           Pipes (P) :=
                             ((Pointer_Address (Pipe_Labels (P).Value),
                               C.Size_T (US.Length (Pipe.Label))),
                              (Endpoints (First_E)'Address, C.Size_T (E - First_E)));
                           P := P + 1;
                        end;
                     end if;
                  end loop;
                  for Point of Group.Pointings loop
                     Pointings (Q) := Raw_Pointing (Point);
                     Q := Q + 1;
                  end loop;
                  Groups (G).Mode := C.RF_Element_Group_Mode_RX;
                  Groups (G).RX.Group :=
                    ((Pointer_Address (Labels (G).Value), C.Size_T (US.Length (Group.Label))),
                     Interfaces.C.double (Group.Duty),
                     ((if F = First_F then System.Null_Address else Frequencies (First_F)'Address),
                      C.Size_T (F - First_F)),
                     ((if P = First_P then System.Null_Address else Pipes (First_P)'Address),
                      C.Size_T (P - First_P)));
                  Groups (G).RX.Expected_Pointing_Angles :=
                    ((if Q = First_Q then System.Null_Address else Pointings (First_Q)'Address),
                     C.Size_T (Q - First_Q));
                  G := G + 1;
               end;
            end if;
         end loop;
         if Groups'Length > 0 then
            Raw.Element_Groups.Data := Groups (1)'Address;
         end if;
         return Result : Job_Request do
            Check
              (C.RF_VA_Submit_Job_V4
                 (VA.Handle, Raw'Access, Result.Handle'Access, D'Address, D'Length, R'Access),
               D);
         end return;
      end;
   end Submit_Job;

   function Is_Open (Request : Job_Request) return Boolean
   is (Request.Handle /= C.Null_RF_Job_Request);
   procedure Close (Request : in out Job_Request) is
      D : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      R : aliased C.Size_T := 0;
   begin
      Check (C.RF_Job_Request_Close (Request.Handle'Access, D'Address, D'Length, R'Access), D);
   end Close;
   overriding
   procedure Finalize (Request : in out Job_Request) is
      Ignored : Interfaces.Integer_32;
   begin
      Ignored := C.RF_Job_Request_Close (Request.Handle'Access, System.Null_Address, 0, null);
   exception
      when others =>
         Request.Handle := C.Null_RF_Job_Request;
   end Finalize;
   function Outcome (Result : Job_Result) return Request_Outcome
   is (Result.State);
   function Error_Code (Result : Job_Result) return Request_Error_Code
   is (Result.Code);
   function Description (Result : Job_Result) return String
   is (US.To_String (Result.Text));
   function Wait (Request : Job_Request; Timeout_Milliseconds : Natural) return Job_Result is
      D      : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      R      : aliased C.Size_T := 0;
      Raw    : aliased C.RF_Job_Result_V1 := (Error_Code => 0);
      Status : Interfaces.Integer_32;
   begin
      Status :=
        C.RF_Job_Request_Wait
          (Request.Handle,
           Interfaces.Unsigned_32 (Timeout_Milliseconds),
           Raw'Access,
           D'Address,
           D'Length,
           R'Access);
      if Status = C.Timeout then
         raise Timeout_Error;
      end if;
      if Status = C.Success then
         if Raw.Error_Code /= 0 then
            raise Provider_Error with "invalid successful Job result";
         end if;
         return (State => Created, Code => None, Text => US.Null_Unbounded_String);
      elsif Status = C.Provider_Failed then
         if Raw.Error_Code > 8 then
            raise Provider_Error with Message (D);
         end if;
         if R = 0 or else Interfaces.Unsigned_64 (R) > Interfaces.Unsigned_64 (Natural'Last) then
            raise Provider_Error with "invalid Job diagnostic size";
         end if;
         if R > D'Length then
            declare
               Full  : aliased Diagnostic (0 .. R - 1) := [others => Interfaces.C.nul];
               Again : aliased C.RF_Job_Result_V1 := (Error_Code => 0);
               Need  : aliased C.Size_T := 0;
               Retry : constant Interfaces.Integer_32 :=
                 C.RF_Job_Request_Wait
                   (Request.Handle, 0, Again'Access, Full'Address, Full'Length, Need'Access);
            begin
               if Retry /= Status or else Again.Error_Code /= Raw.Error_Code or else Need /= R then
                  raise Provider_Error with "Job terminal result changed";
               end if;
               return
                 (State => Failed,
                  Code  => Request_Error_Code'Enum_Val (Raw.Error_Code),
                  Text  => US.To_Unbounded_String (Message (Full)));
            end;
         end if;
         return
           (State => Failed,
            Code  => Request_Error_Code'Enum_Val (Raw.Error_Code),
            Text  => US.To_Unbounded_String (Message (D)));
      end if;
      raise Provider_Error with Message (D);
   end Wait;

   type Job_Info_Access is access all C.RF_Job_Info_V1;
   function To_Job_Info is new Ada.Unchecked_Conversion (System.Address, Job_Info_Access);
   function Claim (Request : Job_Request'Class) return Job is
      D       : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      R       : aliased C.Size_T := 0;
      Address : aliased System.Address := System.Null_Address;
   begin
      return Result : Job do
         Check
           (C.RF_Job_Request_Claim
              (Request.Handle, Result.Handle'Access, D'Address, D'Length, R'Access),
            D);
         Check (C.RF_Job_View (Result.Handle, Address'Access, D'Address, D'Length, R'Access), D);
         if Address = System.Null_Address then
            raise Provider_Error with "null Job snapshot";
         end if;
         declare
            Info : constant C.RF_Job_Info_V1 := To_Job_Info (Address).all;
         begin
            if (Info.RX_Stream_IDs.Size > 0 and then Info.RX_Stream_IDs.Data = System.Null_Address)
              or else Interfaces.Unsigned_64 (Info.RX_Stream_IDs.Size)
                      > Interfaces.Unsigned_64
                          (Natural'Last
                           / (Interfaces.Unsigned_32'Object_Size / System.Storage_Unit))
            then
               raise Provider_Error with "invalid Job stream span";
            end if;
            Result.Start_Seconds := Info.Actual_Start_Seconds;
            Result.Start_Femtoseconds := Info.Actual_Start_Femtoseconds;
            Result.Duration_Femtoseconds := Info.Total_Job_Duration_Femtoseconds;
            Result.Instance_ID := Info.VA_Instance_ID;
            Result.Definition_ID := Info.VA_Definition_ID;
            Result.Details_ID := Info.Job_Details_ID;
            Result.Request_ID := Info.Job_Request_ID;
            Result.Lookahead := Info.Lookahead_Femtoseconds;
            for I in 1 .. Natural (Info.RX_Stream_IDs.Size) loop
               Result.Streams.Append
                 (To_U32
                    (Info.RX_Stream_IDs.Data
                     + Storage_Offset
                         ((I - 1) * Interfaces.Unsigned_32'Object_Size / System.Storage_Unit)).all);
            end loop;
         end;
      end return;
   end Claim;
   function Is_Open (Object : Job) return Boolean
   is (Object.Handle /= C.Null_RF_Job);
   function Cancelled (Result : Cancel_Result) return Boolean
   is (Result.Was_Cancelled);
   function Error_Code (Result : Cancel_Result) return Cancel_Error
   is (Result.Code);
   procedure Finalize_Job (Object : in out Job) is
      D      : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      R      : aliased C.Size_T := 0;
      Result : Interfaces.Integer_32;
   begin
      Result := C.RF_Job_Finalize (Object.Handle, D'Address, D'Length, R'Access);
      if Result /= C.Success and then R > D'Length then
         declare
            Full : aliased Diagnostic (0 .. R - 1) := [others => Interfaces.C.nul];
         begin
            Check (C.RF_Job_Finalize (Object.Handle, Full'Address, Full'Length, null), Full);
         end;
      end if;
      Check (Result, D);
   end Finalize_Job;
   function Wait_Job_Status (Object : Job; Timeout_Milliseconds : Natural) return Job_Status is
      D      : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      R      : aliased C.Size_T := 0;
      Value  : aliased Interfaces.Unsigned_32 := Interfaces.Unsigned_32'Last;
      Result : Interfaces.Integer_32;
   begin
      if Natural'Size > Interfaces.Unsigned_32'Size
        and then Timeout_Milliseconds > Natural (Interfaces.Unsigned_32'Last)
      then
         raise Constraint_Error with "Job wait timeout exceeds native range";
      end if;
      Result :=
        C.RF_Job_Wait_Status
          (Object.Handle,
           Interfaces.Unsigned_32 (Timeout_Milliseconds),
           Value'Access,
           D'Address,
           D'Length,
           R'Access);
      if Result = C.Timeout then
         raise Timeout_Error with "Job status pending";
      end if;
      if Result /= C.Success and then R > D'Length then
         declare
            Full : aliased Diagnostic (0 .. R - 1) := [others => Interfaces.C.nul];
         begin
            Check
              (C.RF_Job_Wait_Status
                 (Object.Handle, 0, Value'Access, Full'Address, Full'Length, null),
               Full);
         end;
      end if;
      Check (Result, D);
      if Value > C.RF_Job_Status_Failed_Invalid_State then
         raise Provider_Error with "invalid native Job status";
      end if;
      return Job_Status'Val (Integer (Value));
   end Wait_Job_Status;
   function Cancel_Job (Object : in out Job) return Cancel_Result is
      D      : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      R      : aliased C.Size_T := 0;
      Result : aliased C.RF_Job_Cancel_Result_V1 := (others => 0);
      Code   : Interfaces.Integer_32;
   begin
      Code := C.RF_Job_Cancel (Object.Handle, Result'Access, D'Address, D'Length, R'Access);
      if Code /= C.Success and then R > D'Length then
         declare
            Full : aliased Diagnostic (0 .. R - 1) := [others => Interfaces.C.nul];
         begin
            Check
              (C.RF_Job_Cancel (Object.Handle, Result'Access, Full'Address, Full'Length, null),
               Full);
         end;
      end if;
      Check (Code, D);
      if Result.Cancelled > 1 or else Result.Error_Code /= C.RF_Cancel_Error_None then
         raise Provider_Error with "invalid native Job cancellation result";
      end if;
      return (Was_Cancelled => Result.Cancelled = 1, Code => None);
   end Cancel_Job;
   procedure Close (Object : in out Job) is
      D : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      R : aliased C.Size_T := 0;
   begin
      Check (C.RF_Job_Close (Object.Handle'Access, D'Address, D'Length, R'Access), D);
   end Close;
   overriding
   procedure Finalize (Object : in out Job) is
      Ignored : Interfaces.Integer_32;
   begin
      Ignored := C.RF_Job_Close (Object.Handle'Access, System.Null_Address, 0, null);
   exception
      when others =>
         Object.Handle := C.Null_RF_Job;
   end Finalize;
   function Actual_Start_Seconds (Object : Job) return Interfaces.Integer_64
   is (Object.Start_Seconds);
   function Actual_Start_Femtoseconds (Object : Job) return Interfaces.Integer_64
   is (Object.Start_Femtoseconds);
   function Total_Job_Duration_Femtoseconds (Object : Job) return Interfaces.Integer_64
   is (Object.Duration_Femtoseconds);
   function VA_Instance_ID (Object : Job) return Interfaces.Unsigned_32
   is (Object.Instance_ID);
   function VA_Definition_ID (Object : Job) return Interfaces.Unsigned_32
   is (Object.Definition_ID);
   function Job_Details_ID (Object : Job) return Interfaces.Unsigned_32
   is (Object.Details_ID);
   function Job_Request_ID (Object : Job) return Interfaces.Unsigned_32
   is (Object.Request_ID);
   function Lookahead_Femtoseconds (Object : Job) return Interfaces.Integer_64
   is (Object.Lookahead);
   function RX_Stream_ID_Count (Object : Job) return Natural
   is (Natural (Object.Streams.Length));
   function RX_Stream_ID_At (Object : Job; Index : Positive) return Interfaces.Unsigned_32
   is (Object.Streams (Index));
end AMS.MEL.RF.C2;
