with Ada.Unchecked_Conversion;
with Interfaces.C;
with System;

package body AMS.MEL.IR.Scheduling.Metadata is
   package C renames AMS.MEL_C_API;
   use type Interfaces.C.char;
   use type Interfaces.C.size_t;
   use type Interfaces.Integer_32;
   use type Interfaces.Unsigned_32;
   Not_Supported_Return : constant Interfaces.Unsigned_32 := 3;
   type Diagnostic is array (C.Size_T range 0 .. 511) of aliased Interfaces.C.char
   with Convention => C;
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
           (if Result'Length = 0 then "native Scheduling metadata operation failed" else Result);
      end;
   end Message;
   procedure Check (Code : Interfaces.Integer_32; D : Diagnostic) is
   begin
      if Code = C.Timeout then
         raise Timeout_Error;
      elsif Code = C.Stream_Stopped then
         raise Stream_Stopped;
      elsif Code /= C.Success then
         raise Provider_Error with Message (D);
      end if;
   end Check;
   --  IRMEL-ADA-012 independent Scheduling ExecuteTaskAck owner.
   type Ack_View_Access is access all C.IR_Execute_Task_Ack_V1;
   function To_Ack_View is new Ada.Unchecked_Conversion (System.Address, Ack_View_Access);
   use type C.Scheduling_Ack_Metadata_Handle;

   function Open_Execute_Task_Ack
     (Channel : Scheduling_Channel; Queue_Capacity : Positive) return Execute_Task_Ack_Stream
   is
      D            : aliased Diagnostic := [others => Interfaces.C.nul];
      R            : aliased C.Size_T := 0;
      Registration : aliased Interfaces.Unsigned_32 := 16#FFFF_FFFF#;
   begin
      return Result : Execute_Task_Ack_Stream do
         declare
            Code : constant Interfaces.Integer_32 :=
              C.IR_Scheduling_Ack_Open
                (Channel.Handle,
                 C.Size_T (Queue_Capacity),
                 Result.Handle'Access,
                 Registration'Access,
                 D'Address,
                 D'Length,
                 R'Access);
         begin
            if Code = C.Success then
               null;
            elsif Code = C.Provider_Failed and then Registration = Not_Supported_Return then
               raise Not_Supported with Message (D);
            else
               raise Provider_Error with Message (D);
            end if;
         end;
      end return;
   end Open_Execute_Task_Ack;

   function Receive_Execute_Task_Ack
     (Stream : Execute_Task_Ack_Stream; Timeout_Milliseconds : Natural := 0) return Execute_Task_Ack
   is
      Event   : aliased C.Scheduling_Ack_Event_Handle := C.Null_Scheduling_Ack_Event;
      Address : aliased System.Address := System.Null_Address;
      D       : aliased Diagnostic := [others => Interfaces.C.nul];
      R       : aliased C.Size_T := 0;
      procedure Release is
         Ignored : Interfaces.Integer_32;
      begin
         Ignored := C.IR_Scheduling_Ack_Event_Close (Event'Access, System.Null_Address, 0, null);
      end Release;
   begin
      if Stream.Handle = C.Null_Scheduling_Ack_Metadata then
         raise Provider_Error with "ExecuteTaskAck metadata owner is closed";
      end if;
      Check
        (C.IR_Scheduling_Ack_Receive
           (Stream.Handle,
            Interfaces.Unsigned_32 (Timeout_Milliseconds),
            Event'Access,
            D'Address,
            D'Length,
            R'Access),
         D);
      begin
         Check
           (C.IR_Scheduling_Ack_Event_View (Event, Address'Access, D'Address, D'Length, R'Access),
            D);
         declare
            Result : constant Execute_Task_Ack :=
              (Command_ID => To_Ack_View (Address).Command_ID,
               Request_ID => To_Ack_View (Address).Request_ID);
         begin
            --  The owned Ada copy is complete before the native event is released.
            Release;
            return Result;
         end;
      exception
         when others =>
            Release;
            raise;
      end;
   end Receive_Execute_Task_Ack;

   function Execute_Task_Ack_Counters (Stream : Execute_Task_Ack_Stream) return Metadata_Counters is
      Raw : aliased C.Metadata_Counters_V1 := (others => 0);
      D   : aliased Diagnostic := [others => Interfaces.C.nul];
      R   : aliased C.Size_T := 0;
   begin
      if Stream.Handle = C.Null_Scheduling_Ack_Metadata then
         raise Provider_Error with "ExecuteTaskAck metadata owner is closed";
      end if;
      Check
        (C.IR_Scheduling_Ack_Get_Counters
           (Stream.Handle, Raw'Access, D'Address, D'Length, R'Access),
         D);
      return
        (AMS.MEL.IR.Counter (Raw.Events_Received),
         AMS.MEL.IR.Counter (Raw.Events_Dropped_Queue_Full),
         AMS.MEL.IR.Counter (Raw.Malformed_Or_Unsupported));
   end Execute_Task_Ack_Counters;

   procedure Close (Stream : in out Execute_Task_Ack_Stream) is
      D : aliased Diagnostic := [others => Interfaces.C.nul];
      R : aliased C.Size_T := 0;
   begin
      Check (C.IR_Scheduling_Ack_Close (Stream.Handle'Access, D'Address, D'Length, R'Access), D);
   end Close;

   overriding
   procedure Finalize (Stream : in out Execute_Task_Ack_Stream) is
      Ignored : Interfaces.Integer_32;
   begin
      Ignored := C.IR_Scheduling_Ack_Close (Stream.Handle'Access, System.Null_Address, 0, null);
   exception
      when others =>
         Stream.Handle := C.Null_Scheduling_Ack_Metadata;
   end Finalize;

   --  IRMEL-ADA-013 independent Scheduling TaskExecutingRep owner.
   function Is_Operational (Kind : Task_Kind) return Boolean
   is (Kind in Operational_Task_Kind);

   type Rep_View_Access is access all C.IR_Task_Executing_Rep_V1;
   function To_Rep_View is new Ada.Unchecked_Conversion (System.Address, Rep_View_Access);
   use type C.Scheduling_Rep_Metadata_Handle;

   --  Checked native-to-Ada conversions. The native owner already rejects these values; a
   --  value outside the pinned domain is never reinterpreted, it fails the whole report.
   function To_Task (Raw : Interfaces.Unsigned_32) return Task_Kind is
   begin
      if Raw > Interfaces.Unsigned_32 (Task_Kind'Enum_Rep (Task_Kind'Last)) then
         raise Provider_Error with "native Scheduling returned unknown Task";
      end if;
      return Task_Kind'Enum_Val (Raw);
   end To_Task;

   function To_Status (Raw : Interfaces.Unsigned_32) return Task_Completion_Status is
   begin
      if Raw
        > Interfaces.Unsigned_32 (Task_Completion_Status'Enum_Rep (Task_Completion_Status'Last))
      then
         raise Provider_Error with "native Scheduling returned unknown TaskCompStatus";
      end if;
      return Task_Completion_Status'Enum_Val (Raw);
   end To_Status;

   function To_Report (Raw : C.IR_Task_Executing_Rep_V1) return Task_Executing_Report
   is ((Command_ID            => Raw.Command_ID,
        Activity_ID           => Raw.Activity_ID,
        Request_ID            => Raw.Request_ID,
        Current_Task          => To_Task (Raw.Task_Value),
        Estimated_Duration_NS => Raw.Estimated_Duration_NS,
        Track_ID              => Raw.Track_ID,
        Previous_Activity_ID  => Raw.Previous_Activity_ID,
        Previous_Request_ID   => Raw.Previous_Request_ID,
        Previous_Task         => To_Task (Raw.Previous_Task),
        Previous_Task_Status  => To_Status (Raw.Previous_Task_Status)));

   function Open_Task_Executing
     (Channel : Scheduling_Channel; Queue_Capacity : Positive) return Task_Executing_Stream
   is
      D            : aliased Diagnostic := [others => Interfaces.C.nul];
      R            : aliased C.Size_T := 0;
      Registration : aliased Interfaces.Unsigned_32 := 16#FFFF_FFFF#;
   begin
      return Result : Task_Executing_Stream do
         declare
            Code : constant Interfaces.Integer_32 :=
              C.IR_Scheduling_Rep_Open
                (Channel.Handle,
                 C.Size_T (Queue_Capacity),
                 Result.Handle'Access,
                 Registration'Access,
                 D'Address,
                 D'Length,
                 R'Access);
         begin
            if Code = C.Success then
               null;
            elsif Code = C.Provider_Failed and then Registration = Not_Supported_Return then
               raise Not_Supported with Message (D);
            else
               raise Provider_Error with Message (D);
            end if;
         end;
      end return;
   end Open_Task_Executing;

   function Receive_Task_Executing
     (Stream : Task_Executing_Stream; Timeout_Milliseconds : Natural := 0)
      return Task_Executing_Report
   is
      Event   : aliased C.Scheduling_Rep_Event_Handle := C.Null_Scheduling_Rep_Event;
      Address : aliased System.Address := System.Null_Address;
      D       : aliased Diagnostic := [others => Interfaces.C.nul];
      R       : aliased C.Size_T := 0;
      procedure Release is
         Ignored : Interfaces.Integer_32;
      begin
         Ignored := C.IR_Scheduling_Rep_Event_Close (Event'Access, System.Null_Address, 0, null);
      end Release;
   begin
      if Stream.Handle = C.Null_Scheduling_Rep_Metadata then
         raise Provider_Error with "TaskExecutingRep metadata owner is closed";
      end if;
      Check
        (C.IR_Scheduling_Rep_Receive
           (Stream.Handle,
            Interfaces.Unsigned_32 (Timeout_Milliseconds),
            Event'Access,
            D'Address,
            D'Length,
            R'Access),
         D);
      begin
         Check
           (C.IR_Scheduling_Rep_Event_View (Event, Address'Access, D'Address, D'Length, R'Access),
            D);
         declare
            Result : constant Task_Executing_Report := To_Report (To_Rep_View (Address).all);
         begin
            --  The owned Ada copy is complete before the native event is released.
            Release;
            return Result;
         end;
      exception
         when others =>
            Release;
            raise;
      end;
   end Receive_Task_Executing;

   function Task_Executing_Counters (Stream : Task_Executing_Stream) return Metadata_Counters is
      Raw : aliased C.Metadata_Counters_V1 := (others => 0);
      D   : aliased Diagnostic := [others => Interfaces.C.nul];
      R   : aliased C.Size_T := 0;
   begin
      if Stream.Handle = C.Null_Scheduling_Rep_Metadata then
         raise Provider_Error with "TaskExecutingRep metadata owner is closed";
      end if;
      Check
        (C.IR_Scheduling_Rep_Get_Counters
           (Stream.Handle, Raw'Access, D'Address, D'Length, R'Access),
         D);
      return
        (AMS.MEL.IR.Counter (Raw.Events_Received),
         AMS.MEL.IR.Counter (Raw.Events_Dropped_Queue_Full),
         AMS.MEL.IR.Counter (Raw.Malformed_Or_Unsupported));
   end Task_Executing_Counters;

   procedure Close (Stream : in out Task_Executing_Stream) is
      D : aliased Diagnostic := [others => Interfaces.C.nul];
      R : aliased C.Size_T := 0;
   begin
      Check (C.IR_Scheduling_Rep_Close (Stream.Handle'Access, D'Address, D'Length, R'Access), D);
   end Close;

   overriding
   procedure Finalize (Stream : in out Task_Executing_Stream) is
      Ignored : Interfaces.Integer_32;
   begin
      Ignored := C.IR_Scheduling_Rep_Close (Stream.Handle'Access, System.Null_Address, 0, null);
   exception
      when others =>
         Stream.Handle := C.Null_Scheduling_Rep_Metadata;
   end Finalize;
end AMS.MEL.IR.Scheduling.Metadata;
