with Interfaces.C;
with System;

package body AMS.MEL.IR.C2 is
   use type Interfaces.Integer_32;
   use type Interfaces.Unsigned_32;
   use type Interfaces.C.char;
   use type Interfaces.C.size_t;
   use type AMS.MEL_C_API.C2_Handle;
   use type AMS.MEL_C_API.Mode_Request_Handle;
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

   function Is_Open (Request : Mode_Request) return Boolean is
     (Request.Owner.Handle /= C.Null_Mode_Request);

   function Status (Result : Mode_Result) return Outcome is (Result.Result_Status);
   function Mode (Result : Mode_Result) return MFA_Mode is (Result.Result_Mode);
   function Rejection_Code (Result : Mode_Result) return Error_Code is
     (Result.Result_Code);
   function Description (Result : Mode_Result) return String is
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

   overriding procedure Finalize (Channel : in out Control_Channel) is
      Ignored : Interfaces.Integer_32;
   begin
      Ignored := C.IR_C2_Close
        (Channel.Handle'Access, System.Null_Address, 0, null);
   exception
      when others => Channel.Handle := C.Null_C2;
   end Finalize;
end AMS.MEL.IR.C2;
