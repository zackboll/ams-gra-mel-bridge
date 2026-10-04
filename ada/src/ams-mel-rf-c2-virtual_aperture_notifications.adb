with Interfaces.C;
with System;

package body AMS.MEL.RF.C2.Virtual_Aperture_Notifications is
   package C renames AMS.MEL_C_API;
   use type Interfaces.Integer_32;
   use type Interfaces.Unsigned_32;
   use type C.RF_VA_Subscription_Handle;
   type Diagnostic is array (0 .. 511) of aliased Interfaces.C.char with Convention => C;
   procedure Check (Code : Interfaces.Integer_32; Buffer : Diagnostic) is
      Last : Natural := 0;
   begin
      if Code = C.Timeout then
         raise Timeout_Error with "RF VA change pending";
      elsif Code = C.Stream_Stopped then
         raise Subscription_Stopped with "RF VA notification reception stopped";
      elsif Code /= C.Success then
         while Last < Buffer'Length and then Interfaces.C.char'Pos (Buffer (Last)) /= 0 loop
            Last := Last + 1;
         end loop;
         declare
            Text : String (1 .. Last);
         begin
            for I in Text'Range loop
               Text (I) := Character'Val (Interfaces.C.char'Pos (Buffer (I - 1)));
            end loop;
            raise Provider_Error with (if Last = 0 then "native VA notification failed" else Text);
         end;
      end if;
   end Check;
   function Open (VA : in out Virtual_Aperture'Class) return Subscription is
      D : aliased Diagnostic := [others => Interfaces.C.nul];
   begin
      return Result : Subscription do
         Check
           (C.RF_VA_Subscription_Open (VA.Handle, Result.Handle'Access, D'Address, D'Length, null),
            D);
      end return;
   end Open;
   function Is_Open (Object : Subscription) return Boolean
   is (Object.Handle /= C.Null_RF_VA_Subscription);
   procedure Wait_For_Change (Object : Subscription; Timeout_Milliseconds : Natural) is
      D : aliased Diagnostic := [others => Interfaces.C.nul];
   begin
      Check
        (C.RF_VA_Subscription_Wait
           (Object.Handle,
            Interfaces.Unsigned_32 (Timeout_Milliseconds),
            D'Address,
            D'Length,
            null),
         D);
   end Wait_For_Change;
   function Statistics (Object : Subscription) return Subscription_Statistics is
      D     : aliased Diagnostic := [others => Interfaces.C.nul];
      Value : aliased C.RF_VA_Subscription_Statistics_V1;
   begin
      Check
        (C.RF_VA_Subscription_Get_Statistics
           (Object.Handle, Value'Access, D'Address, D'Length, null),
         D);
      if Value.Pending > 1 or else Value.Stopped > 1 then
         raise Provider_Error with "invalid VA notification flags";
      end if;
      return
        (Value.Callback_Entries,
         Value.Callbacks_Coalesced,
         Value.Notifications_Delivered,
         Value.Callbacks_After_Stop,
         Value.Pending = 1,
         Value.Stopped = 1);
   end Statistics;
   procedure Unsubscribe (VA : in out Virtual_Aperture'Class; Object : in out Subscription) is
      D : aliased Diagnostic := [others => Interfaces.C.nul];
   begin
      Check
        (C.RF_VA_Subscription_Unsubscribe (VA.Handle, Object.Handle, D'Address, D'Length, null), D);
   end Unsubscribe;
   procedure Close (Object : in out Subscription) is
      D : aliased Diagnostic := [others => Interfaces.C.nul];
   begin
      Check (C.RF_VA_Subscription_Close (Object.Handle'Access, D'Address, D'Length, null), D);
   end Close;
   overriding
   procedure Finalize (Object : in out Subscription) is
      Ignored : Interfaces.Integer_32;
   begin
      Ignored := C.RF_VA_Subscription_Close (Object.Handle'Access, System.Null_Address, 0, null);
   exception
      when others =>
         Object.Handle := C.Null_RF_VA_Subscription;
   end Finalize;
end AMS.MEL.RF.C2.Virtual_Aperture_Notifications;
