with Ada.Strings.Unbounded;
with Ada.Unchecked_Conversion;
with AMS.MEL.IR.Capability_Conversion;
with Interfaces.C;
with System;

package body AMS.MEL.IR.Scheduling is
   package C renames AMS.MEL_C_API;
   package V renames IR.Channel;
   package US renames Ada.Strings.Unbounded;
   use type Interfaces.Integer_32;
   use type Interfaces.C.char;
   use type C.Session_Handle;
   use type C.Scheduling_Handle;

   --  Upstream ChannelType::Scheduling.
   Scheduling_Channel_Type : constant Interfaces.Unsigned_32 := 3;

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
         return (if Result'Length = 0 then "native IR Scheduling operation failed" else Result);
      end;
   end Message;

   function String_View (Value : String) return C.String_View_V1
   is (Data => (if Value'Length = 0 then System.Null_Address else Value'Address),
       Size => C.Size_T (Value'Length));

   function Create_Config
     (Channel_ID : UCI_ID; Platform_ID : UCI_ID; Sensor_Location : Component_Location)
      return Scheduling_Config
   is ((Channel_ID, Platform_ID, Sensor_Location));

   function Open (Parent : Session; Config : Scheduling_Config) return Scheduling_Channel is
      Channel_Label  : aliased constant String := US.To_String (Config.Channel.Label);
      Platform_Label : aliased constant String := US.To_String (Config.Platform.Label);
      Key            : aliased constant String := US.To_String (Config.Location.Key_Value);
      System_Name    : aliased constant String := US.To_String (Config.Location.System_Value);
      Raw            : aliased C.IR_Scheduling_Config_V1 :=
        ((C.Byte_Array_16 (Config.Channel.Value), String_View (Channel_Label)),
         Scheduling_Channel_Type,
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
      return Result : Scheduling_Channel do
         if C.IR_Scheduling_Open
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

   function Is_Open (Channel : Scheduling_Channel) return Boolean
   is (Channel.Handle /= C.Null_Scheduling);

   procedure Enable (Channel : in out Scheduling_Channel) is
      D        : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      Required : aliased C.Size_T := 0;
   begin
      if Channel.Handle = C.Null_Scheduling then
         raise Provider_Error with "IR Scheduling channel is closed";
      end if;
      if C.IR_Scheduling_Enable (Channel.Handle, D'Address, D'Length, Required'Access) /= C.Success
      then
         raise Provider_Error with Message (D);
      end if;
   end Enable;

   function Capabilities (Channel : Scheduling_Channel) return V.Channel_Capability is
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
      if Channel.Handle = C.Null_Scheduling then
         raise Provider_Error with "IR Scheduling channel is closed";
      end if;
      if C.IR_Scheduling_Get_Capabilities
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
            --  Reuses the one shared Ada ChannelCapability converter; the
            --  result owns every string and collection.
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

   function As_Channel (Channel : Scheduling_Channel) return V.View is
      D        : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      Required : aliased C.Size_T := 0;
   begin
      if Channel.Handle = C.Null_Scheduling then
         raise Provider_Error with "IR Scheduling channel is closed";
      end if;
      return Result : V.View do
         if C.IR_Channel_From_Scheduling
              (Channel.Handle, Result.Handle'Access, D'Address, D'Length, Required'Access)
           /= C.Success
         then
            raise Provider_Error with Message (D);
         end if;
      end return;
   end As_Channel;

   procedure Close (Channel : in out Scheduling_Channel) is
      D        : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      Required : aliased C.Size_T := 0;
   begin
      if C.IR_Scheduling_Close (Channel.Handle'Access, D'Address, D'Length, Required'Access)
        /= C.Success
      then
         raise Provider_Error with Message (D);
      end if;
   end Close;

   overriding
   procedure Finalize (Channel : in out Scheduling_Channel) is
      Ignored : Interfaces.Integer_32;
   begin
      --  Non-raising cleanup fallback. A detach failure deliberately leaves
      --  the native owner non-null so the retained graph is never destroyed.
      Ignored := C.IR_Scheduling_Close (Channel.Handle'Access, System.Null_Address, 0, null);
   exception
      when others =>
         Channel.Handle := C.Null_Scheduling;
   end Finalize;
end AMS.MEL.IR.Scheduling;
