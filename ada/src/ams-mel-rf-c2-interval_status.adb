with Ada.Unchecked_Conversion;
with Interfaces.C;
with System;
with System.Storage_Elements;

package body AMS.MEL.RF.C2.Interval_Status is
   package C renames AMS.MEL_C_API;
   use type Interfaces.Integer_32;
   use type Interfaces.Unsigned_32;
   use type C.Size_T;
   use type C.RF_Interval_Status_Handle;
   use type System.Address;
   use System.Storage_Elements;
   type Diagnostic is array (0 .. 511) of aliased Interfaces.C.char with Convention => C;
   procedure Check (Code : Interfaces.Integer_32; Buffer : Diagnostic) is
      Last : Natural := 0;
   begin
      if Code = C.Timeout then
         raise Timeout_Error with "RF interval status pending";
      elsif Code = C.Stream_Stopped then
         raise Stream_Stopped with "RF interval status reception stopped";
      elsif Code /= C.Success then
         while Last < Buffer'Length and then Interfaces.C.char'Pos (Buffer (Last)) /= 0 loop
            Last := Last + 1;
         end loop;
         declare
            Message : String (1 .. Last);
         begin
            for I in Message'Range loop
               Message (I) := Character'Val (Interfaces.C.char'Pos (Buffer (I - 1)));
            end loop;
            raise Provider_Error with Message;
         end;
      end if;
   end Check;
   function Open
     (Object                                       : Job;
      Queue_Capacity                               : Positive;
      Max_Event_Log_Entries, Max_Activity_ID_Bytes : Natural) return Stream
   is
      Options : aliased C.RF_Interval_Status_Options_V1 :=
        (C.Size_T (Queue_Capacity),
         C.Size_T (Max_Event_Log_Entries),
         C.Size_T (Max_Activity_ID_Bytes));
      D       : aliased Diagnostic := [others => Interfaces.C.nul];
   begin
      return Result : Stream do
         Check
           (C.RF_Interval_Status_Open
              (Object.Handle, Options'Access, Result.Handle'Access, D'Address, D'Length, null),
            D);
      end return;
   end Open;
   function Is_Open (Object : Stream) return Boolean
   is (Object.Handle /= C.Null_RF_Interval_Status);
   procedure Close (Object : in out Stream) is
      D : aliased Diagnostic := [others => Interfaces.C.nul];
   begin
      Check (C.RF_Interval_Status_Close (Object.Handle'Access, D'Address, D'Length, null), D);
   end Close;
   overriding
   procedure Finalize (Object : in out Stream) is
      Ignored : Interfaces.Integer_32;
   begin
      Ignored := C.RF_Interval_Status_Close (Object.Handle'Access, System.Null_Address, 0, null);
   exception
      when others =>
         Object.Handle := C.Null_RF_Interval_Status;
   end Finalize;
   type Native_Event_Owner is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased C.RF_Interval_Status_Event_Handle := C.Null_RF_Interval_Status_Event;
   end record;
   overriding
   procedure Finalize (Object : in out Native_Event_Owner) is
      Ignored : Interfaces.Integer_32;
   begin
      Ignored :=
        C.RF_Interval_Status_Event_Close (Object.Handle'Access, System.Null_Address, 0, null);
   exception
      when others =>
         Object.Handle := C.Null_RF_Interval_Status_Event;
   end Finalize;
   function Receive_Event (Object : Stream; Timeout_Milliseconds : Natural) return Status_Event is
      Owner      : Native_Event_Owner;
      Address    : aliased System.Address := System.Null_Address;
      D          : aliased Diagnostic := [others => Interfaces.C.nul];
      type View_Access is access all C.RF_Interval_Status_V1;
      type Log_Access is access all C.RF_Job_Event_Log_Entry_V1;
      type Byte_Access is access all Interfaces.Unsigned_8;
      function To_View is new Ada.Unchecked_Conversion (System.Address, View_Access);
      function To_Log is new Ada.Unchecked_Conversion (System.Address, Log_Access);
      function To_Byte is new Ada.Unchecked_Conversion (System.Address, Byte_Access);
      Entry_Size : constant C.Size_T := C.RF_Job_Event_Log_Entry_V1'Size / System.Storage_Unit;
      procedure Validate_Span (Data : System.Address; Count, Size : C.Size_T) is
      begin
         if (Count > 0 and then Data = System.Null_Address)
           or else Count > C.Size_T (Natural'Last)
           or else Count > C.Size_T (Storage_Offset'Last) / Size
           or else (Count > 0
                    and then To_Integer (Data)
                             > Integer_Address'Last - Integer_Address (Count * Size - 1))
         then
            raise Provider_Error with "invalid native interval status span";
         end if;
      end Validate_Span;
   begin
      Check
        (C.RF_Interval_Status_Receive
           (Object.Handle,
            Interfaces.Unsigned_32 (Timeout_Milliseconds),
            Owner.Handle'Access,
            D'Address,
            D'Length,
            null),
         D);
      Check
        (C.RF_Interval_Status_Event_View (Owner.Handle, Address'Access, D'Address, D'Length, null),
         D);
      if Address = System.Null_Address then
         raise Provider_Error with "null native interval status view";
      end if;
      if To_Integer (Address) mod C.RF_Interval_Status_V1'Alignment /= 0 then
         raise Provider_Error with "unaligned native interval status view";
      end if;
      declare
         View   : constant C.RF_Interval_Status_V1 := To_View (Address).all;
         Result : Status_Event;
      begin
         if View.Completion_Status > 24 then
            raise Provider_Error with "unknown interval completion status";
         end if;
         Validate_Span (View.Event_Log.Data, View.Event_Log.Size, Entry_Size);
         if View.Event_Log.Size > 0
           and then To_Integer (View.Event_Log.Data) mod C.RF_Job_Event_Log_Entry_V1'Alignment /= 0
         then
            raise Provider_Error with "unaligned native event log";
         end if;
         Validate_Span (View.Activity_ID.Data, View.Activity_ID.Size, 1);
         Result.ID := View.Interval_ID;
         Result.State := Completion_Status'Val (Integer (View.Completion_Status));
         for I in 1 .. Natural (View.Event_Log.Size) loop
            declare
               Item : constant C.RF_Job_Event_Log_Entry_V1 :=
                 To_Log (View.Event_Log.Data + Storage_Offset (C.Size_T (I - 1) * Entry_Size)).all;
            begin
               if Item.Trigger > 7 then
                  raise Provider_Error with "unknown event log trigger";
               end if;
               Result.Logs.Append
                 (Event_Log_Entry'
                    (Item.Event_ID,
                     Log_Trigger'Val (Integer (Item.Trigger)),
                     Item.Time_Seconds,
                     Item.Time_Fractional_Femtoseconds));
            end;
         end loop;
         for I in 1 .. Natural (View.Activity_ID.Size) loop
            Result.Activity.Append (To_Byte (View.Activity_ID.Data + Storage_Offset (I - 1)).all);
         end loop;
         Check
           (C.RF_Interval_Status_Event_Close (Owner.Handle'Access, D'Address, D'Length, null), D);
         return Result;
      end;
   end Receive_Event;
   function Statistics (Object : Stream) return Counters is
      Value : aliased C.RF_Interval_Status_Counters_V1;
      D     : aliased Diagnostic := [others => Interfaces.C.nul];
   begin
      Check
        (C.RF_Interval_Status_Get_Counters (Object.Handle, Value'Access, D'Address, D'Length, null),
         D);
      return
        (Value.Callback_Entries,
         Value.Events_Queued,
         Value.Events_Delivered,
         Value.Queue_Full_Drops,
         Value.Malformed_Drops,
         Value.Oversize_Drops,
         Value.Allocation_Failures,
         Value.Callbacks_After_Close);
   end Statistics;
   function Interval_ID (Event : Status_Event) return Interfaces.Unsigned_32
   is (Event.ID);
   function Completion (Event : Status_Event) return Completion_Status
   is (Event.State);
   function Log_Count (Event : Status_Event) return Natural
   is (Natural (Event.Logs.Length));
   function Log_At (Event : Status_Event; Index : Positive) return Event_Log_Entry
   is (Event.Logs (Index));
   function Event_ID (Entry_Value : Event_Log_Entry) return Interfaces.Unsigned_32
   is (Entry_Value.ID);
   function Trigger (Entry_Value : Event_Log_Entry) return Log_Trigger
   is (Entry_Value.Reason);
   function Time_Seconds (Entry_Value : Event_Log_Entry) return Interfaces.Integer_64
   is (Entry_Value.Seconds);
   function Time_Fractional_Femtoseconds
     (Entry_Value : Event_Log_Entry) return Interfaces.Integer_64
   is (Entry_Value.Fraction);
   function Activity_ID_Length (Event : Status_Event) return Natural
   is (Natural (Event.Activity.Length));
   function Activity_ID_Byte (Event : Status_Event; Index : Positive) return Interfaces.Unsigned_8
   is (Event.Activity (Index));
end AMS.MEL.RF.C2.Interval_Status;
