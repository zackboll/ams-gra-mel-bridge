with Ada.Unchecked_Conversion;
with Interfaces;
with Interfaces.C;
with System;

package body AMS.MEL.IR.Instrumentation.Metadata is
   package C renames AMS.MEL_C_API;
   use type Interfaces.Integer_32;
   use type Interfaces.Unsigned_32;
   use type Interfaces.C.char;
   use type C.Instrumentation_Handle;
   use type C.Instrumentation_Metadata_Handle;
   use type C.Instrumentation_Event_Handle;

   type Diagnostic is array (C.Size_T range <>) of aliased Interfaces.C.char with Convention => C;
   subtype Fixed_Diagnostic is Diagnostic (0 .. 511);
   type Event_Access is access all C.IR_Instrumentation_Event_V1;
   function To_Event is new Ada.Unchecked_Conversion (System.Address, Event_Access);

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
           (if Result'Length = 0
            then "native IR Instrumentation metadata operation failed"
            else Result);
      end;
   end Message;

   function Copy_Event (Raw : C.IR_Instrumentation_Event_V1) return Instrumentation_Report is
   begin
      if Raw.Kind /= 1 then
         raise Provider_Error with "invalid native Instrumentation metadata kind";
      end if;
      if Raw.Report.Priority > Priority'Enum_Rep (Debug) then
         raise Provider_Error with "native IR Instrumentation returned unknown Priority";
      end if;
      return
        (Command_ID   => Raw.Report.Command_ID,
         Size         => Raw.Report.Size,
         Timestamp_NS => Long_Long_Integer (Raw.Report.Timestamp_NS),
         Priority     => Priority'Enum_Val (Raw.Report.Priority));
   end Copy_Event;

   function Open
     (Channel : Instrumentation_Channel; Queue_Capacity : Positive := 16) return Metadata_Channel
   is
      D        : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      Required : aliased C.Size_T := 0;
   begin
      if Channel.Handle = C.Null_Instrumentation then
         raise Provider_Error with "Instrumentation channel is closed";
      end if;
      return Result : Metadata_Channel do
         if C.IR_Instrumentation_Metadata_Open
              (Channel.Handle,
               C.Size_T (Queue_Capacity),
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

   function Is_Open (Stream : Metadata_Channel) return Boolean
   is (Stream.Handle /= C.Null_Instrumentation_Metadata);

   function Receive
     (Stream : Metadata_Channel; Timeout_Milliseconds : Natural := 0) return Instrumentation_Report
   is
      Owner    : aliased C.Instrumentation_Event_Handle := C.Null_Instrumentation_Event;
      Address  : aliased System.Address := System.Null_Address;
      D        : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      Required : aliased C.Size_T := 0;
      Code     : Interfaces.Integer_32;
      procedure Release is
         Ignored : Interfaces.Integer_32;
      begin
         Ignored := C.IR_Instrumentation_Event_Close (Owner'Access, System.Null_Address, 0, null);
      end Release;
   begin
      Code :=
        C.IR_Instrumentation_Metadata_Receive
          (Stream.Handle,
           Interfaces.Unsigned_32 (Timeout_Milliseconds),
           Owner'Access,
           D'Address,
           D'Length,
           Required'Access);
      if Code = C.Timeout then
         raise Timeout_Error;
      elsif Code = C.Stream_Stopped then
         raise Stream_Stopped;
      elsif Code /= C.Success then
         raise Provider_Error with Message (D);
      end if;
      if C.IR_Instrumentation_Event_View
           (Owner, Address'Access, D'Address, D'Length, Required'Access)
        /= C.Success
      then
         Release;
         raise Provider_Error with Message (D);
      end if;
      declare
         Result : constant Instrumentation_Report := Copy_Event (To_Event (Address).all);
      begin
         Release;
         return Result;
      end;
   exception
      when others =>
         if Owner /= C.Null_Instrumentation_Event then
            Release;
         end if;
         raise;
   end Receive;

   function Counters (Stream : Metadata_Channel) return Metadata_Counters is
      Raw      : aliased C.Metadata_Counters_V1 := (others => 0);
      D        : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      Required : aliased C.Size_T := 0;
   begin
      if C.IR_Instrumentation_Metadata_Get_Counters
           (Stream.Handle, Raw'Access, D'Address, D'Length, Required'Access)
        /= C.Success
      then
         raise Provider_Error with Message (D);
      end if;
      return
        (Counter (Raw.Events_Received),
         Counter (Raw.Events_Dropped_Queue_Full),
         Counter (Raw.Malformed_Or_Unsupported));
   end Counters;

   procedure Close (Stream : in out Metadata_Channel) is
      D        : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      Required : aliased C.Size_T := 0;
   begin
      if C.IR_Instrumentation_Metadata_Close
           (Stream.Handle'Access, D'Address, D'Length, Required'Access)
        /= C.Success
      then
         raise Provider_Error with Message (D);
      end if;
   end Close;

   overriding
   procedure Finalize (Stream : in out Metadata_Channel) is
      Ignored : Interfaces.Integer_32;
   begin
      Ignored :=
        C.IR_Instrumentation_Metadata_Close (Stream.Handle'Access, System.Null_Address, 0, null);
   exception
      when others =>
         Stream.Handle := C.Null_Instrumentation_Metadata;
   end Finalize;
end AMS.MEL.IR.Instrumentation.Metadata;
