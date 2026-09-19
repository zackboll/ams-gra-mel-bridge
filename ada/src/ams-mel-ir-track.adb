with Ada.Unchecked_Conversion;
with AMS.MEL.IR.Capability_Conversion;
with Interfaces;
with Interfaces.C;
with System;

package body AMS.MEL.IR.Track is
   package C renames AMS.MEL_C_API;
   package V renames IR.Channel;
   use type Interfaces.Integer_32;
   use type Interfaces.C.char;
   use type C.Session_Handle;
   use type C.Track_Handle;

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
         return (if Result'Length = 0 then "native IR Track operation failed" else Result);
      end;
   end Message;

   function String_View (Value : String) return C.String_View_V1
   is (Data => (if Value'Length = 0 then System.Null_Address else Value'Address),
       Size => C.Size_T (Value'Length));

   function Create_Config
     (Channel_ID : UCI_ID; Platform_ID : UCI_ID; Sensor_Location : Component_Location)
      return Track_Config
   is ((Channel_ID, Platform_ID, Sensor_Location));

   function Open (Parent : Session; Config : Track_Config) return Track_Channel is
      Channel_Label  : aliased constant String := US.To_String (Config.Channel.Label);
      Platform_Label : aliased constant String := US.To_String (Config.Platform.Label);
      Key            : aliased constant String := US.To_String (Config.Location.Key_Value);
      System_Name    : aliased constant String := US.To_String (Config.Location.System_Value);
      --  ChannelType::IRSTTrack is upstream value 0.
      Raw            : aliased C.IR_Track_Config_V1 :=
        ((C.Byte_Array_16 (Config.Channel.Value), String_View (Channel_Label)),
         0,
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
      return Result : Track_Channel do
         if C.IR_Track_Open
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

   function Is_Open (Channel : Track_Channel) return Boolean
   is (Channel.Handle /= C.Null_Track);

   procedure Enable (Channel : in out Track_Channel) is
      D        : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      Required : aliased C.Size_T := 0;
   begin
      if C.IR_Track_Enable (Channel.Handle, D'Address, D'Length, Required'Access) /= C.Success then
         raise Provider_Error with Message (D);
      end if;
   end Enable;

   function Capabilities (Channel : Track_Channel) return V.Channel_Capability is
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
      if C.IR_Track_Get_Capabilities
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

   procedure Close (Channel : in out Track_Channel) is
      D        : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      Required : aliased C.Size_T := 0;
   begin
      if C.IR_Track_Close (Channel.Handle'Access, D'Address, D'Length, Required'Access) /= C.Success
      then
         raise Provider_Error with Message (D);
      end if;
   end Close;

   overriding
   procedure Finalize (Channel : in out Track_Channel) is
      Ignored : Interfaces.Integer_32;
   begin
      --  Non-raising cleanup fallback. A detach failure deliberately leaves
      --  the native owner non-null so the retained graph is never destroyed.
      Ignored := C.IR_Track_Close (Channel.Handle'Access, System.Null_Address, 0, null);
   exception
      when others =>
         Channel.Handle := C.Null_Track;
   end Finalize;
end AMS.MEL.IR.Track;
