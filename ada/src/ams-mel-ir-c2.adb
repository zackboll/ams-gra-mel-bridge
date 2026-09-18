with Interfaces.C;
with System;

package body AMS.MEL.IR.C2 is
   use type Interfaces.Integer_32;
   use type Interfaces.Unsigned_32;
   use type Interfaces.C.char;
   use type Interfaces.C.size_t;
   use type AMS.MEL_C_API.C2_Handle;
   use type AMS.MEL_C_API.Mode_Request_Handle;
   use type AMS.MEL_C_API.Return_Request_Handle;
   use type AMS.MEL_C_API.Session_Handle;
   package C renames AMS.MEL_C_API;

   Diagnostic_Capacity : constant := 512;
   type Diagnostic_Array is array (Interfaces.C.size_t range <>) of
     aliased Interfaces.C.char
     with Convention => C;
   subtype Fixed_Diagnostic is Diagnostic_Array
     (0 .. Diagnostic_Capacity - 1);

   function Message (Buffer : Diagnostic_Array) return String is
      Length : Natural := 0;
   begin
      while Length < Buffer'Length
        and then Buffer (Interfaces.C.size_t (Length)) /= Interfaces.C.nul
      loop
         Length := Length + 1;
      end loop;
      declare
         Result : String (1 .. Length);
      begin
         for Index in Result'Range loop
            Result (Index) := Character'Val
              (Interfaces.C.char'Pos
                 (Buffer (Interfaces.C.size_t (Index - 1))));
         end loop;
         return Result;
      end;
   end Message;

   function Failure_Message (Buffer : Diagnostic_Array) return String is
      Value : constant String := Message (Buffer);
   begin
      return (if Value'Length = 0 then "native IR C2 operation failed" else Value);
   end Failure_Message;

   function String_View (Value : String) return C.String_View_V1 is
     ((Data => (if Value'Length = 0 then System.Null_Address else Value'Address),
       Size => Interfaces.C.size_t (Value'Length)));

   function Create_Config
     (Channel_ID : UCI_ID; Platform_ID : UCI_ID;
      Sensor_Location : Component_Location) return Control_Config is
     ((Channel => Channel_ID, Platform => Platform_ID, Location => Sensor_Location));

   function Open (Parent : Session; Config : Control_Config)
     return Control_Channel
   is
      Channel_Label  : aliased constant String := US.To_String (Config.Channel.Label);
      Platform_Label : aliased constant String := US.To_String (Config.Platform.Label);
      Key            : aliased constant String := US.To_String (Config.Location.Key_Value);
      System_Name    : aliased constant String := US.To_String (Config.Location.System_Value);
      Raw : aliased C.IR_C2_Config_V1 :=
        (Channel_Type => 2,
         Channel_ID => (UUID => C.Byte_Array_16 (Config.Channel.Value),
                        Descriptive_Label => String_View (Channel_Label)),
         Platform_ID => (UUID => C.Byte_Array_16 (Config.Platform.Value),
                         Descriptive_Label => String_View (Platform_Label)),
         Sensor_Location =>
           (Offset_X_M => Interfaces.C.double (Config.Location.X),
            Offset_Y_M => Interfaces.C.double (Config.Location.Y),
            Offset_Z_M => Interfaces.C.double (Config.Location.Z),
            Key => String_View (Key), System_Name => String_View (System_Name)));
      Diagnostic : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      Required   : aliased C.Size_T := 0;
   begin
      if Parent.Handle = C.Null_Session then
         raise Provider_Error with "provider session is closed";
      end if;
      return Result : Control_Channel do
         declare
            Code : constant Interfaces.Integer_32 := C.IR_C2_Open
              (Parent.Handle, Raw'Access, Result.Handle'Access,
               Diagnostic'Address, Diagnostic'Length, Required'Access);
         begin
            if Code /= C.Success then
               raise Provider_Error with Failure_Message (Diagnostic);
            end if;
         end;
      end return;
   end Open;

   function Is_Open (Channel : Control_Channel) return Boolean is
     (Channel.Handle /= C.Null_C2);

   procedure Enable (Channel : in out Control_Channel) is
      Diagnostic : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      Required   : aliased C.Size_T := 0;
      Code : constant Interfaces.Integer_32 := C.IR_C2_Enable
        (Channel.Handle, Diagnostic'Address, Diagnostic'Length, Required'Access);
   begin
      if Code /= C.Success then
         raise Provider_Error with Failure_Message (Diagnostic);
      end if;
   end Enable;

   function Submit_Operate
     (Channel : Control_Channel; ID : Command_ID := 0) return Mode_Request
   is
      Diagnostic : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      Required   : aliased C.Size_T := 0;
   begin
      return Result : Mode_Request do
         declare
            Code : constant Interfaces.Integer_32 := C.IR_C2_Submit_Operate
               (Channel.Handle, Interfaces.Unsigned_32 (ID), Result.Owner.Handle'Access,
               Diagnostic'Address, Diagnostic'Length, Required'Access);
         begin
            if Code /= C.Success then
               raise Provider_Error with Failure_Message (Diagnostic);
            end if;
         end;
      end return;
   end Submit_Operate;

   function Raw_Scan (Value : Scan_Parameters) return C.IR_Scan_Param_V1 is
     ((Elevation_Defined_With_Range_And_Altitude =>
         (if Value.Elevation_Defined_With_Range_And_Altitude then 1 else 0),
       Center_AZ_Rad => Interfaces.C.double (Value.Center_Azimuth_Rad),
       Center_EL_Rad => Interfaces.C.double (Value.Center_Elevation_Rad),
       Center_Frame_Ref_EL => Coordinate_Frame_Reference'Enum_Rep
         (Value.Center_Frame_Reference_EL),
       Center_Frame_Ref_AZ => Coordinate_Frame_Reference'Enum_Rep
         (Value.Center_Frame_Reference_AZ),
       Scan_Width_Rad => Interfaces.C.double (Value.Scan_Width_Rad),
       Scan_Height_Rad => Interfaces.C.double (Value.Scan_Height_Rad),
       Scan_Type => (Interfaces.Unsigned_32 (Value.Continuous_Scan),
                     Interfaces.Unsigned_32 (Value.Returning),
                     Interfaces.Unsigned_32 (Value.Agile_Scan)),
       Scan_ID => Interfaces.Unsigned_32 (Value.Scan_ID),
       Scan_Rate_Rad_Per_Second => Interfaces.C.double
         (Value.Scan_Rate_Rad_Per_Second),
       Preferred_Revisit_Interval_Seconds => Interfaces.C.double
         (Value.Preferred_Revisit_Interval_Seconds),
       Required_Revisit_Interval_Seconds => Interfaces.C.double
         (Value.Required_Revisit_Interval_Seconds),
       Max_Range_Of_Interest_M => Interfaces.Unsigned_32
         (Value.Max_Range_Of_Interest_M),
       Min_Range_Of_Interest_M => Interfaces.Unsigned_32
         (Value.Min_Range_Of_Interest_M),
       Elevation_Scan_Center_Altitude_M => Interfaces.Unsigned_32
         (Value.Elevation_Scan_Center_Altitude_M),
       Elevation_Scan_Center_Range_M => Interfaces.Unsigned_32
         (Value.Elevation_Scan_Center_Range_M),
       Degradation_Method => Degradation_Method'Enum_Rep (Value.Degradation)));

   function Submit_Mode
     (Channel : Control_Channel; ID : Command_ID; State : MFA_State;
      Mode : MFA_Mode; Scan_Parameters : AMS.MEL.IR.C2.Scan_Parameters :=
        Default_Scan_Parameters) return Mode_Request
   is
      Raw : aliased C.IR_Mode_Command_V1 :=
        (Interfaces.Unsigned_32 (ID), MFA_State'Enum_Rep (State),
         MFA_Mode'Enum_Rep (Mode), Raw_Scan (Scan_Parameters));
      Diagnostic : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      Required : aliased C.Size_T := 0;
   begin
      if Channel.Handle = C.Null_C2 then
         raise Provider_Error with "IR C2 channel is closed";
      end if;
      return Result : Mode_Request do
         declare
            Code : constant Interfaces.Integer_32 := C.IR_C2_Submit_Mode
              (Channel.Handle, Raw'Access, Result.Owner.Handle'Access,
               Diagnostic'Address, Diagnostic'Length, Required'Access);
         begin
            if Code /= C.Success then
               raise Provider_Error with Failure_Message (Diagnostic);
            end if;
         end;
      end return;
   end Submit_Mode;

   function Is_Open (Request : Mode_Request) return Boolean is
     (Request.Owner.Handle /= C.Null_Mode_Request);

   function Submit_BIT_No_Op
     (Channel : Control_Channel; ID : Command_ID := 0) return Return_Request
   is
      Diagnostic : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      Required   : aliased C.Size_T := 0;
   begin
      return Result : Return_Request do
         declare
            Code : constant Interfaces.Integer_32 := C.IR_C2_Submit_BIT_No_Op
              (Channel.Handle, Interfaces.Unsigned_32 (ID),
               Result.Owner.Handle'Access, Diagnostic'Address,
               Diagnostic'Length, Required'Access);
         begin
            if Code /= C.Success then
               raise Provider_Error with Failure_Message (Diagnostic);
            end if;
         end;
      end return;
   end Submit_BIT_No_Op;

   type Raw_BIT_ID_Array is array (Positive range <>) of aliased Interfaces.Unsigned_32
     with Convention => C;

   function Submit_BIT_IDs
     (Channel : Control_Channel; IDs : BIT_ID_Array; ID : Command_ID;
      Initiate : Boolean) return Return_Request
   is
      Raw_IDs : aliased Raw_BIT_ID_Array (IDs'Range);
      Raw : aliased C.IR_BIT_Command_V1 :=
        (Interfaces.Unsigned_32 (ID),
         (System.Null_Address, 0), (System.Null_Address, 0),
         (System.Null_Address, 0));
      Diagnostic : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      Required : aliased C.Size_T := 0;
   begin
      if Channel.Handle = C.Null_C2 then
         raise Provider_Error with "IR C2 channel is closed";
      end if;
      for Index in IDs'Range loop
         Raw_IDs (Index) := Interfaces.Unsigned_32 (IDs (Index));
      end loop;
      if Initiate then
         Raw.Initiate_BIT_IDs := (Raw_IDs'Address, Raw_IDs'Length);
      else
         Raw.Cancel_BIT_IDs := (Raw_IDs'Address, Raw_IDs'Length);
      end if;
      return Result : Return_Request do
         declare
            Code : constant Interfaces.Integer_32 := C.IR_C2_Submit_BIT
              (Channel.Handle, Raw'Access, Result.Owner.Handle'Access,
               Diagnostic'Address, Diagnostic'Length, Required'Access);
         begin
            if Code /= C.Success then
               raise Provider_Error with Failure_Message (Diagnostic);
            end if;
         end;
      end return;
   end Submit_BIT_IDs;

   function Submit_BIT_Initiate
     (Channel : Control_Channel; IDs : BIT_ID_Array; ID : Command_ID := 0)
      return Return_Request is (Submit_BIT_IDs (Channel, IDs, ID, True));

   function Submit_BIT_Cancel
     (Channel : Control_Channel; IDs : BIT_ID_Array; ID : Command_ID := 0)
      return Return_Request is (Submit_BIT_IDs (Channel, IDs, ID, False));

   function Submit_BIT_Clear_Faults
     (Channel : Control_Channel; Fault_Codes : Fault_Code_Vectors.Vector;
      ID : Command_ID := 0) return Return_Request
   is
      Views : aliased C.String_View_Array
        (1 .. Positive (Fault_Codes.Length));
      Diagnostic : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      Required : aliased C.Size_T := 0;
   begin
      if Channel.Handle = C.Null_C2 then
         raise Provider_Error with "IR C2 channel is closed";
      end if;
      for Index in Views'Range loop
         declare
            Element : constant Fault_Code_Vectors.Constant_Reference_Type :=
              Fault_Codes.Constant_Reference (Index);
         begin
            Views (Index) := String_View (Element.Element.all);
         end;
      end loop;
      declare
         Raw : aliased C.IR_BIT_Command_V1 :=
           (Interfaces.Unsigned_32 (ID), (System.Null_Address, 0),
            (System.Null_Address, 0), (Views'Address, Views'Length));
      begin
         return Result : Return_Request do
            declare
               Code : constant Interfaces.Integer_32 := C.IR_C2_Submit_BIT
                 (Channel.Handle, Raw'Access, Result.Owner.Handle'Access,
                  Diagnostic'Address, Diagnostic'Length, Required'Access);
            begin
               if Code /= C.Success then
                  raise Provider_Error with Failure_Message (Diagnostic);
               end if;
            end;
         end return;
      end;
   end Submit_BIT_Clear_Faults;

   function Submit_Config_Set
     (Channel : Control_Channel; ID : Command_ID := 0;
      System_Time_NS : System_Time_Nanoseconds := 0; Config : String := "")
      return Return_Request
   is
      Raw : aliased C.IR_Config_Set_Command_V1 :=
        (Interfaces.Unsigned_32 (ID), Interfaces.Integer_64 (System_Time_NS),
         String_View (Config));
      Diagnostic : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      Required : aliased C.Size_T := 0;
   begin
      if Channel.Handle = C.Null_C2 then
         raise Provider_Error with "IR C2 channel is closed";
      end if;
      return Result : Return_Request do
         declare
            Code : constant Interfaces.Integer_32 := C.IR_C2_Submit_Config_Set
              (Channel.Handle, Raw'Access, Result.Owner.Handle'Access,
               Diagnostic'Address, Diagnostic'Length, Required'Access);
         begin
            if Code /= C.Success then
               raise Provider_Error with Failure_Message (Diagnostic);
            end if;
         end;
      end return;
   end Submit_Config_Set;

   function Is_Open (Request : Return_Request) return Boolean is
     (Request.Owner.Handle /= C.Null_Return_Request);

   function Status (Result : Mode_Result) return Outcome is (Result.Result_Status);
   function Mode (Result : Mode_Result) return MFA_Mode is (Result.Result_Mode);
   function Rejection_Code (Result : Mode_Result) return Error_Code is
     (Result.Result_Code);
   function Description (Result : Mode_Result) return String is
     (US.To_String (Result.Result_Text));
   function Status (Result : Return_Result) return Outcome is (Result.Result_Status);
   function Value (Result : Return_Result) return Command_Return is (Result.Result_Value);
   function Rejection_Code (Result : Return_Result) return Error_Code is
     (Result.Result_Code);
   function Description (Result : Return_Result) return String is
     (US.To_String (Result.Result_Text));

   function Wait
     (Request : Mode_Request; Timeout_Milliseconds : Natural) return Mode_Result
   is
      Raw : aliased C.IR_Mode_Result_V1 := (Mode => 0, Error_Code => 0);
      Diagnostic : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      Required   : aliased C.Size_T := 0;
      Timeout : Interfaces.Unsigned_32;
      Code : Interfaces.Integer_32;
   begin
      Timeout := Interfaces.Unsigned_32 (Timeout_Milliseconds);
      Code := C.IR_Mode_Request_Wait
        (Request.Owner.Handle, Timeout, Raw'Access, Diagnostic'Address,
         Diagnostic'Length, Required'Access);
      if Code = C.Timeout then
         raise Timeout_Error with "IR C2 mode request timed out";
      elsif Code = C.Success then
         if Raw.Mode > 3 then
            raise Provider_Error with "native IR C2 returned unknown MFA mode";
         end if;
         return (Result_Status => Success, Result_Mode => MFA_Mode'Val (Raw.Mode),
                 Result_Code => None, Result_Text => US.Null_Unbounded_String);
      elsif Code = C.Command_Rejected then
         if Raw.Error_Code > 8 then
            raise Provider_Error with "native IR C2 returned unknown MEL error code";
         end if;
         declare
            Text : US.Unbounded_String :=
              US.To_Unbounded_String (Message (Diagnostic));
         begin
            if Required > Diagnostic'Length then
               declare
                  Complete : aliased Diagnostic_Array (0 .. Required - 1) :=
                    [others => Interfaces.C.nul];
                  Retry_Raw : aliased C.IR_Mode_Result_V1 :=
                    (Mode => 0, Error_Code => 0);
                  Retry_Required : aliased C.Size_T := 0;
                  Retry_Code : constant Interfaces.Integer_32 :=
                    C.IR_Mode_Request_Wait
                      (Request.Owner.Handle, 0, Retry_Raw'Access,
                       Complete'Address, Complete'Length,
                       Retry_Required'Access);
               begin
                  if Retry_Code /= C.Command_Rejected
                    or else Retry_Raw.Error_Code /= Raw.Error_Code
                    or else Retry_Required /= Required
                  then
                     raise Provider_Error with
                       "native IR C2 rejection changed during diagnostic retry";
                  end if;
                  Text := US.To_Unbounded_String (Message (Complete));
               end;
            end if;
            return (Result_Status => Rejected, Result_Mode => Unused,
                    Result_Code => Error_Code'Val (Raw.Error_Code),
                    Result_Text => Text);
         end;
      else
         raise Provider_Error with Failure_Message (Diagnostic);
      end if;
   end Wait;

   function Wait
     (Request : Return_Request; Timeout_Milliseconds : Natural) return Return_Result
   is
      Raw : aliased C.IR_Return_Result_V1 := (Value => 0, Error_Code => 0);
      Diagnostic : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      Required   : aliased C.Size_T := 0;
      Code : Interfaces.Integer_32;
   begin
      Code := C.IR_Return_Request_Wait
        (Request.Owner.Handle, Interfaces.Unsigned_32 (Timeout_Milliseconds),
         Raw'Access, Diagnostic'Address, Diagnostic'Length, Required'Access);
      if Code = C.Timeout then
         raise Timeout_Error with "IR C2 Return request timed out";
      elsif Code = C.Success then
         if Raw.Value > 4 then
            raise Provider_Error with "native IR C2 returned unknown IR Return value";
         end if;
         return (Result_Status => Success,
                 Result_Value => Command_Return'Val (Raw.Value),
                 Result_Code => None, Result_Text => US.Null_Unbounded_String);
      elsif Code = C.Command_Rejected then
         if Raw.Error_Code > 8 then
            raise Provider_Error with "native IR C2 returned unknown MEL error code";
         end if;
         declare
            Text : US.Unbounded_String := US.To_Unbounded_String (Message (Diagnostic));
         begin
            if Required > Diagnostic'Length then
               declare
                  Complete : aliased Diagnostic_Array (0 .. Required - 1) :=
                    [others => Interfaces.C.nul];
                  Retry_Raw : aliased C.IR_Return_Result_V1 :=
                    (Value => 0, Error_Code => 0);
                  Retry_Required : aliased C.Size_T := 0;
                  Retry_Code : constant Interfaces.Integer_32 :=
                    C.IR_Return_Request_Wait
                      (Request.Owner.Handle, 0, Retry_Raw'Access,
                       Complete'Address, Complete'Length, Retry_Required'Access);
               begin
                  if Retry_Code /= C.Command_Rejected
                    or else Retry_Raw.Error_Code /= Raw.Error_Code
                    or else Retry_Required /= Required
                  then
                     raise Provider_Error with
                       "native IR C2 rejection changed during diagnostic retry";
                  end if;
                  Text := US.To_Unbounded_String (Message (Complete));
               end;
            end if;
            return (Result_Status => Rejected, Result_Value => Return_Success,
                    Result_Code => Error_Code'Val (Raw.Error_Code),
                    Result_Text => Text);
         end;
      else
         raise Provider_Error with Failure_Message (Diagnostic);
      end if;
   end Wait;

   procedure Close (Request : in out Mode_Request) is
      Diagnostic : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      Required   : aliased C.Size_T := 0;
      Code : constant Interfaces.Integer_32 := C.IR_Mode_Request_Close
        (Request.Owner.Handle'Access, Diagnostic'Address, Diagnostic'Length,
         Required'Access);
   begin
      if Code /= C.Success then
         raise Provider_Error with Failure_Message (Diagnostic);
      end if;
   end Close;

   procedure Close (Request : in out Return_Request) is
      Diagnostic : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      Required   : aliased C.Size_T := 0;
      Code : constant Interfaces.Integer_32 := C.IR_Return_Request_Close
        (Request.Owner.Handle'Access, Diagnostic'Address, Diagnostic'Length,
         Required'Access);
   begin
      if Code /= C.Success then
         raise Provider_Error with Failure_Message (Diagnostic);
      end if;
   end Close;

   procedure Close (Channel : in out Control_Channel) is
      Diagnostic : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      Required   : aliased C.Size_T := 0;
      Code : constant Interfaces.Integer_32 := C.IR_C2_Close
        (Channel.Handle'Access, Diagnostic'Address, Diagnostic'Length,
         Required'Access);
   begin
      if Code /= C.Success then
         raise Provider_Error with Failure_Message (Diagnostic);
      end if;
   end Close;

   overriding procedure Finalize (Request : in out Request_Owner) is
      Ignored : Interfaces.Integer_32;
   begin
      Ignored := C.IR_Mode_Request_Close
        (Request.Handle'Access, System.Null_Address, 0, null);
   exception
      when others => Request.Handle := C.Null_Mode_Request;
   end Finalize;

   overriding procedure Finalize (Request : in out Return_Request_Owner) is
      Ignored : Interfaces.Integer_32;
   begin
      Ignored := C.IR_Return_Request_Close
        (Request.Handle'Access, System.Null_Address, 0, null);
   exception
      when others => Request.Handle := C.Null_Return_Request;
   end Finalize;

   overriding procedure Finalize (Channel : in out Control_Channel) is
      Ignored : Interfaces.Integer_32;
   begin
      Ignored := C.IR_C2_Close
        (Channel.Handle'Access, System.Null_Address, 0, null);
   exception
      when others => Channel.Handle := C.Null_C2;
   end Finalize;
end AMS.MEL.IR.C2;
