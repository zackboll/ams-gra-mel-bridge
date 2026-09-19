with Ada.Unchecked_Conversion;
with AMS.MEL.IR.Capability_Conversion;
with Interfaces.C;
with System;

package body AMS.MEL.IR.Instrumentation is
   package C renames AMS.MEL_C_API;
   package V renames IR.Channel;
   use type Interfaces.Integer_32;
   use type Interfaces.Unsigned_32;
   use type Interfaces.C.char;
   use type Interfaces.C.size_t;
   use type C.Instrumentation_Handle;
   use type C.Instrumentation_Request_Handle;
   use type C.Session_Handle;

   type Diagnostic is array (C.Size_T range <>) of aliased Interfaces.C.char with Convention => C;
   subtype Fixed_Diagnostic is Diagnostic (0 .. 511);
   type Cap_Access is access all C.IR_Channel_Capability_V1;
   function To_Cap is new Ada.Unchecked_Conversion (System.Address, Cap_Access);

   function Message (Value : Diagnostic) return String is
      Last : Natural := 0;
   begin
      while Last < Value'Length and then Value (C.Size_T (Last)) /= Interfaces.C.nul loop
         Last := Last + 1;
      end loop;
      declare
         Result : String (1 .. Last);
      begin
         for I in Result'Range loop
            Result (I) := Character'Val (Interfaces.C.char'Pos (Value (C.Size_T (I - 1))));
         end loop;
         return
           (if Result'Length = 0 then "native IR Instrumentation operation failed" else Result);
      end;
   end Message;

   function String_View (Value : String) return C.String_View_V1
   is (Data => (if Value'Length = 0 then System.Null_Address else Value'Address),
       Size => C.Size_T (Value'Length));

   function Create_Config
     (Channel_ID : UCI_ID; Platform_ID : UCI_ID; Sensor_Location : Component_Location)
      return Instrumentation_Config
   is ((Channel_ID, Platform_ID, Sensor_Location));

   function Open (Parent : Session; Config : Instrumentation_Config) return Instrumentation_Channel
   is
      Channel_Label  : aliased constant String := US.To_String (Config.Channel.Label);
      Platform_Label : aliased constant String := US.To_String (Config.Platform.Label);
      Key            : aliased constant String := US.To_String (Config.Location.Key_Value);
      System_Name    : aliased constant String := US.To_String (Config.Location.System_Value);
      Raw            : aliased C.IR_Instrumentation_Config_V1 :=
        ((C.Byte_Array_16 (Config.Channel.Value), String_View (Channel_Label)),
         5,
         (C.Byte_Array_16 (Config.Platform.Value), String_View (Platform_Label)),
         (Interfaces.C.double (Config.Location.X),
          Interfaces.C.double (Config.Location.Y),
          Interfaces.C.double (Config.Location.Z),
          String_View (Key),
          String_View (System_Name)));
      D              : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      Required       : aliased C.Size_T := 0;
   begin
      if Parent.Handle = C.Null_Session then
         raise Provider_Error with "provider session is closed";
      end if;
      return Result : Instrumentation_Channel do
         if C.IR_Instrumentation_Open
              (Parent.Handle,
               Raw'Access,
               Result.Handle'Access,
               D'Address,
               D'Length,
               Required'Access)
           /= C.Success
         then
            raise Provider_Error with Message (D);
         end if;
      end return;
   end Open;

   function Is_Open (Channel : Instrumentation_Channel) return Boolean
   is (Channel.Handle /= C.Null_Instrumentation);

   procedure Enable (Channel : in out Instrumentation_Channel) is
      D        : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      Required : aliased C.Size_T := 0;
   begin
      if C.IR_Instrumentation_Enable (Channel.Handle, D'Address, D'Length, Required'Access)
        /= C.Success
      then
         raise Provider_Error with Message (D);
      end if;
   end Enable;

   function Capabilities (Channel : Instrumentation_Channel) return V.Channel_Capability is
      Owner    : aliased C.Capability_Handle := C.Null_Capability;
      Address  : aliased System.Address := System.Null_Address;
      D        : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      Required : aliased C.Size_T := 0;
      procedure Release is
         Ignored : Interfaces.Integer_32;
      begin
         Ignored := C.IR_Capability_Close (Owner'Access, System.Null_Address, 0, null);
      end Release;
   begin
      if C.IR_Instrumentation_Get_Capabilities
           (Channel.Handle, Owner'Access, D'Address, D'Length, Required'Access)
        /= C.Success
      then
         raise Provider_Error with Message (D);
      end if;
      if C.IR_Capability_View (Owner, Address'Access, D'Address, D'Length, Required'Access)
        /= C.Success
      then
         Release;
         raise Provider_Error with Message (D);
      end if;
      begin
         declare
            --  Reuses the one shared Ada ChannelCapability converter.
            Result : constant V.Channel_Capability :=
              Capability_Conversion.To_Channel_Capability (To_Cap (Address).all);
         begin
            Release;
            return Result;
         end;
      exception
         when others =>
            Release;
            raise;
      end;
   end Capabilities;

   procedure Close (Channel : in out Instrumentation_Channel) is
      D        : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      Required : aliased C.Size_T := 0;
   begin
      if C.IR_Instrumentation_Close (Channel.Handle'Access, D'Address, D'Length, Required'Access)
        /= C.Success
      then
         raise Provider_Error with Message (D);
      end if;
   end Close;

   overriding
   procedure Finalize (Channel : in out Instrumentation_Channel) is
      Ignored : Interfaces.Integer_32;
   begin
      Ignored := C.IR_Instrumentation_Close (Channel.Handle'Access, System.Null_Address, 0, null);
   exception
      when others =>
         Channel.Handle := C.Null_Instrumentation;
   end Finalize;

   function Status (Result : Instrumentation_Result) return Instrumentation_Outcome
   is (Result.Result_Status);
   function Report (Result : Instrumentation_Result) return Instrumentation_Report
   is (Result.Result_Report);
   function Rejection_Code (Result : Instrumentation_Result) return Instrumentation_Error_Code
   is (Result.Result_Code);
   function Description (Result : Instrumentation_Result) return String
   is (US.To_String (Result.Result_Text));

   function Submit
     (Channel : Instrumentation_Channel; Command : Instrumentation_Level_Command)
      return Instrumentation_Request
   is
      Raw      : aliased C.IR_Instrumentation_Level_Command_V1 :=
        (Command_ID => Command.Command_ID, Priority => Priority'Enum_Rep (Command.Priority));
      D        : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      Required : aliased C.Size_T := 0;
   begin
      return Result : Instrumentation_Request do
         if C.IR_Instrumentation_Submit_Level
              (Channel.Handle,
               Raw'Access,
               Result.Owner.Handle'Access,
               D'Address,
               D'Length,
               Required'Access)
           /= C.Success
         then
            raise Provider_Error with Message (D);
         end if;
      end return;
   end Submit;

   function Is_Open (Request : Instrumentation_Request) return Boolean
   is (Request.Owner.Handle /= C.Null_Instrumentation_Request);

   function To_Report (Raw : C.IR_Instrumentation_Report_V1) return Instrumentation_Report is
   begin
      if Raw.Priority > Priority'Enum_Rep (Debug) then
         raise Provider_Error with "native IR Instrumentation returned unknown Priority";
      end if;
      return
        (Command_ID   => Raw.Command_ID,
         Size         => Raw.Size,
         Timestamp_NS => Long_Long_Integer (Raw.Timestamp_NS),
         Priority     => Priority'Enum_Val (Raw.Priority));
   end To_Report;

   function Wait
     (Request : Instrumentation_Request; Timeout_Milliseconds : Natural)
      return Instrumentation_Result
   is
      Raw  : aliased C.IR_Instrumentation_Result_V1 := ((0, 0, 0, 0), 0);
      D    : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      R    : aliased C.Size_T := 0;
      Code : constant Interfaces.Integer_32 :=
        C.IR_Instrumentation_Request_Wait
          (Request.Owner.Handle,
           Interfaces.Unsigned_32 (Timeout_Milliseconds),
           Raw'Access,
           D'Address,
           D'Length,
           R'Access);
   begin
      if Code = C.Timeout then
         raise Timeout_Error with "IR Instrumentation request timed out";
      elsif Code = C.Success then
         return
           (Result_Status => Success,
            Result_Report => To_Report (Raw.Report),
            Result_Code   => None,
            Result_Text   => US.Null_Unbounded_String);
      elsif Code = C.Command_Rejected then
         if Raw.Error_Code > 8 then
            raise Provider_Error with "native IR Instrumentation returned unknown MEL error code";
         end if;
         declare
            Text : US.Unbounded_String := US.To_Unbounded_String (Message (D));
         begin
            --  The complete rejection text is recovered from the cached
            --  terminal request using exact storage; a changed status, code,
            --  or required size would mean the cache was not terminal.
            if R > D'Length then
               declare
                  Complete       : aliased Diagnostic (0 .. R - 1) := [others => Interfaces.C.nul];
                  Retry_Raw      : aliased C.IR_Instrumentation_Result_V1 := ((0, 0, 0, 0), 0);
                  Retry_Required : aliased C.Size_T := 0;
                  Retry_Code     : constant Interfaces.Integer_32 :=
                    C.IR_Instrumentation_Request_Wait
                      (Request.Owner.Handle,
                       0,
                       Retry_Raw'Access,
                       Complete'Address,
                       Complete'Length,
                       Retry_Required'Access);
               begin
                  if Retry_Code /= C.Command_Rejected
                    or else Retry_Raw.Error_Code /= Raw.Error_Code
                    or else Retry_Required /= R
                  then
                     raise Provider_Error
                       with "native IR Instrumentation rejection changed during diagnostic retry";
                  end if;
                  Text := US.To_Unbounded_String (Message (Complete));
               end;
            end if;
            return
              (Result_Status => Rejected,
               Result_Report => (0, 0, 0, Normal),
               Result_Code   => Instrumentation_Error_Code'Val (Raw.Error_Code),
               Result_Text   => Text);
         end;
      else
         raise Provider_Error with Message (D);
      end if;
   end Wait;

   procedure Close (Request : in out Instrumentation_Request) is
      Ignored : Interfaces.Integer_32;
   begin
      Ignored :=
        C.IR_Instrumentation_Request_Close
          (Request.Owner.Handle'Access, System.Null_Address, 0, null);
   end Close;

   overriding
   procedure Finalize (Request : in out Instrumentation_Request_Owner) is
      Ignored : Interfaces.Integer_32;
   begin
      --  Finalization releases the public owner only; it is not cancellation.
      Ignored :=
        C.IR_Instrumentation_Request_Close (Request.Handle'Access, System.Null_Address, 0, null);
   exception
      when others =>
         Request.Handle := C.Null_Instrumentation_Request;
   end Finalize;
end AMS.MEL.IR.Instrumentation;
