with Ada.Containers;
with Ada.Unchecked_Conversion;
with Interfaces.C;
with System;
with System.Storage_Elements;

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
   use type C.Scheduling_Created_Metadata_Handle;

   function Length (Activities : Activity_Collection) return Natural
   is (Natural (Activities.Values.Length));

   function Element (Activities : Activity_Collection; Index : Positive) return Activity_Record
   is (Activities.Values.Element (Index));

   pragma
     Compile_Time_Error
       (Long_Float'Size /= 64
          or else Long_Float'Machine_Radix /= 2
          or else Long_Float'Machine_Mantissa /= 53
          or else Long_Float'Machine_Emax /= 1024,
        "SchedCreatedRep requires binary64 Long_Float");
   --  Copy the composite representation without evaluating its floating scalars;
   --  GNAT scalar validity checks would reject infinities and NaN payloads.
   function To_Position is new Ada.Unchecked_Conversion (C.IR_Range_Az_El_V1, Range_Az_El);
   type Created_View_Access is access constant C.IR_Sched_Created_Rep_V1;
   type Activity_View_Access is access constant C.IR_Activity_Record_V1;
   function To_Created_View is new Ada.Unchecked_Conversion (System.Address, Created_View_Access);
   function To_Activity_View is new Ada.Unchecked_Conversion (System.Address, Activity_View_Access);

   function Copy_Created (Address : System.Address) return Schedule_Created_Report is
      use System.Storage_Elements;
      use type System.Address;
      Bytes : constant C.Size_T := C.IR_Activity_Record_V1'Object_Size / System.Storage_Unit;
   begin
      if Address = System.Null_Address then
         raise Provider_Error with "missing native SchedCreatedRep view";
      end if;
      declare
         Raw   : C.IR_Sched_Created_Rep_V1 renames To_Created_View (Address).all;
         Count : constant C.Size_T := Raw.Activities.Size;
         Data  : constant System.Address := Raw.Activities.Data;
      begin
         if Count > C.Size_T (Natural'Last)
           or else Count > C.Size_T (Ada.Containers.Count_Type'Last)
           or else Count > C.Size_T (Storage_Offset'Last) / Bytes
           or else (Count > 0
                    and then (Data = System.Null_Address
                              or else To_Integer (Data) mod C.IR_Activity_Record_V1'Alignment /= 0
                              or else To_Integer (Data)
                                      > Integer_Address'Last - Integer_Address (Count * Bytes)))
         then
            raise Provider_Error with "invalid or unrepresentable native activity span";
         end if;
         return Result : Schedule_Created_Report do
            Result.Command_ID := Raw.Command_ID;
            Result.Scan_ID := Raw.Scan_ID;
            Result.Estimated_Duration_NS := Raw.Estimated_Duration_NS;
            Result.Activity_Count := Raw.Activity_Count;
            Result.Activities.Values.Reserve_Capacity (Ada.Containers.Count_Type (Count));
            for I in 1 .. Natural (Count) loop
               declare
                  A : C.IR_Activity_Record_V1 renames
                    To_Activity_View (Data + Storage_Offset (C.Size_T (I - 1) * Bytes)).all;
               begin
                  Result.Activities.Values.Append
                    (New_Item =>
                       Activity_Record'
                         (A.Activity_ID,
                          To_Task (A.Task_ID),
                          A.Track_ID,
                          To_Position (A.Position),
                          A.Duration_NS));
               end;
            end loop;
         end return;
      end;
   end Copy_Created;

   function Open_Schedule_Created
     (Channel : Scheduling_Channel; Queue_Capacity : Positive) return Schedule_Created_Stream
   is
      D            : aliased Diagnostic := [others => Interfaces.C.nul];
      R            : aliased C.Size_T := 0;
      Registration : aliased Interfaces.Unsigned_32 := 16#FFFF_FFFF#;
   begin
      return Result : Schedule_Created_Stream do
         declare
            Code : constant Interfaces.Integer_32 :=
              C.IR_Scheduling_Created_Open
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
   end Open_Schedule_Created;

   function Receive_Schedule_Created
     (Stream : Schedule_Created_Stream; Timeout_Milliseconds : Natural := 0)
      return Schedule_Created_Report
   is
      Event   : aliased C.Scheduling_Created_Event_Handle := C.Null_Scheduling_Created_Event;
      Address : aliased System.Address := System.Null_Address;
      D       : aliased Diagnostic := [others => Interfaces.C.nul];
      R       : aliased C.Size_T := 0;
      procedure Release is
         Ignored : Interfaces.Integer_32;
      begin
         Ignored :=
           C.IR_Scheduling_Created_Event_Close (Event'Access, System.Null_Address, 0, null);
      end Release;
   begin
      if Stream.Handle = C.Null_Scheduling_Created_Metadata then
         raise Provider_Error with "SchedCreatedRep metadata owner is closed";
      end if;
      Check
        (C.IR_Scheduling_Created_Receive
           (Stream.Handle,
            Interfaces.Unsigned_32 (Timeout_Milliseconds),
            Event'Access,
            D'Address,
            D'Length,
            R'Access),
         D);
      begin
         Check
           (C.IR_Scheduling_Created_Event_View
              (Event, Address'Access, D'Address, D'Length, R'Access),
            D);
         declare
            Result : constant Schedule_Created_Report := Copy_Created (Address);
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
   end Receive_Schedule_Created;

   function Schedule_Created_Counters (Stream : Schedule_Created_Stream) return Metadata_Counters is
      Raw : aliased C.Metadata_Counters_V1 := (others => 0);
      D   : aliased Diagnostic := [others => Interfaces.C.nul];
      R   : aliased C.Size_T := 0;
   begin
      if Stream.Handle = C.Null_Scheduling_Created_Metadata then
         raise Provider_Error with "SchedCreatedRep metadata owner is closed";
      end if;
      Check
        (C.IR_Scheduling_Created_Get_Counters
           (Stream.Handle, Raw'Access, D'Address, D'Length, R'Access),
         D);
      return
        (AMS.MEL.IR.Counter (Raw.Events_Received),
         AMS.MEL.IR.Counter (Raw.Events_Dropped_Queue_Full),
         AMS.MEL.IR.Counter (Raw.Malformed_Or_Unsupported));
   end Schedule_Created_Counters;

   procedure Close (Stream : in out Schedule_Created_Stream) is
      D : aliased Diagnostic := [others => Interfaces.C.nul];
      R : aliased C.Size_T := 0;
   begin
      Check
        (C.IR_Scheduling_Created_Close (Stream.Handle'Access, D'Address, D'Length, R'Access), D);
   end Close;

   overriding
   procedure Finalize (Stream : in out Schedule_Created_Stream) is
      Ignored : Interfaces.Integer_32;
   begin
      Ignored := C.IR_Scheduling_Created_Close (Stream.Handle'Access, System.Null_Address, 0, null);
   exception
      when others =>
         Stream.Handle := C.Null_Scheduling_Created_Metadata;
   end Finalize;
   type Update_View_Access is access constant C.IR_Update_Track_List_Response_V1;
   function To_Update_View is new Ada.Unchecked_Conversion (System.Address, Update_View_Access);
   use type C.Scheduling_Update_Track_List_Metadata_Handle;

   function Open_Update_Track_List
     (Channel : Scheduling_Channel; Queue_Capacity : Positive) return Update_Track_List_Stream
   is
      D            : aliased Diagnostic := [others => Interfaces.C.nul];
      R            : aliased C.Size_T := 0;
      Registration : aliased Interfaces.Unsigned_32 := 16#FFFF_FFFF#;
   begin
      return Result : Update_Track_List_Stream do
         declare
            Code : constant Interfaces.Integer_32 :=
              C.IR_Scheduling_Update_Track_List_Open
                (Channel.Handle,
                 C.Size_T (Queue_Capacity),
                 Result.Handle'Access,
                 Registration'Access,
                 D'Address,
                 D'Length,
                 R'Access);
         begin
            if Code = C.Provider_Failed and then Registration = Not_Supported_Return then
               raise Not_Supported with Message (D);
            end if;
            Check (Code, D);
         end;
      end return;
   end Open_Update_Track_List;

   function Receive_Update_Track_List
     (Stream : Update_Track_List_Stream; Timeout_Milliseconds : Natural := 0)
      return Update_Track_List_Response
   is
      use type System.Address;
      Event   : aliased C.Scheduling_Update_Track_List_Event_Handle :=
        C.Null_Scheduling_Update_Track_List_Event;
      Address : aliased System.Address := System.Null_Address;
      D       : aliased Diagnostic := [others => Interfaces.C.nul];
      R       : aliased C.Size_T := 0;
      Result  : Update_Track_List_Response;
      procedure Release is
         Ignored : Interfaces.Integer_32;
      begin
         Ignored :=
           C.IR_Scheduling_Update_Track_List_Event_Close
             (Event'Access, System.Null_Address, 0, null);
      end Release;
   begin
      if Stream.Handle = C.Null_Scheduling_Update_Track_List_Metadata then
         raise Provider_Error with "UpdateTrackListResponse metadata owner is closed";
      end if;
      Check
        (C.IR_Scheduling_Update_Track_List_Receive
           (Stream.Handle,
            Interfaces.Unsigned_32 (Timeout_Milliseconds),
            Event'Access,
            D'Address,
            D'Length,
            R'Access),
         D);
      Check
        (C.IR_Scheduling_Update_Track_List_Event_View
           (Event, Address'Access, D'Address, D'Length, R'Access),
         D);
      if Address = System.Null_Address then
         raise Provider_Error with "null UpdateTrackListResponse view";
      end if;
      declare
         Raw : constant C.IR_Update_Track_List_Response_V1 := To_Update_View (Address).all;
      begin
         if Raw.Response > Track_List_Receipt'Enum_Rep (Track_List_Receipt'Last) then
            raise Provider_Error with "unknown UpdateTrackListResponse receipt";
         end if;
         Result := (Raw.Command_ID, Track_List_Receipt'Enum_Val (Raw.Response));
      end;
      Release;
      return Result;
   exception
      when others =>
         Release;
         raise;
   end Receive_Update_Track_List;

   function Update_Track_List_Counters (Stream : Update_Track_List_Stream) return Metadata_Counters
   is
      Raw : aliased C.Metadata_Counters_V1 := (others => 0);
      D   : aliased Diagnostic := [others => Interfaces.C.nul];
      R   : aliased C.Size_T := 0;
   begin
      Check
        (C.IR_Scheduling_Update_Track_List_Get_Counters
           (Stream.Handle, Raw'Access, D'Address, D'Length, R'Access),
         D);
      return
        (AMS.MEL.IR.Counter (Raw.Events_Received),
         AMS.MEL.IR.Counter (Raw.Events_Dropped_Queue_Full),
         AMS.MEL.IR.Counter (Raw.Malformed_Or_Unsupported));
   end Update_Track_List_Counters;

   procedure Close (Stream : in out Update_Track_List_Stream) is
      D : aliased Diagnostic := [others => Interfaces.C.nul];
      R : aliased C.Size_T := 0;
   begin
      Check
        (C.IR_Scheduling_Update_Track_List_Close
           (Stream.Handle'Access, D'Address, D'Length, R'Access),
         D);
   end Close;

   overriding
   procedure Finalize (Stream : in out Update_Track_List_Stream) is
      Ignored : Interfaces.Integer_32;
   begin
      Ignored :=
        C.IR_Scheduling_Update_Track_List_Close
          (Stream.Handle'Access, System.Null_Address, 0, null);
   exception
      when others =>
         Stream.Handle := C.Null_Scheduling_Update_Track_List_Metadata;
   end Finalize;
end AMS.MEL.IR.Scheduling.Metadata;
