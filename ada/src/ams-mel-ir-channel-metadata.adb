with Interfaces.C;
with System;

package body AMS.MEL.IR.Channel.Metadata is
   package C renames AMS.MEL_C_API;
   use type Interfaces.Integer_32;
   use type C.Comms_Metadata_Handle;
   use type Interfaces.C.char;
   use type C.Size_T;
   type Diagnostic is array (C.Size_T range 0 .. 511) of aliased Interfaces.C.char
   with Convention => C;

   function Message (D : Diagnostic) return String is
      Length : Natural := 0;
   begin
      while Length < D'Length and then D (C.Size_T (Length)) /= Interfaces.C.nul loop
         Length := Length + 1;
      end loop;
      declare
         Text : String (1 .. Length);
      begin
         for I in Text'Range loop
            Text (I) := Character'Val (Interfaces.C.char'Pos (D (C.Size_T (I - 1))));
         end loop;
         return (if Length = 0 then "native common metadata operation failed" else Text);
      end;
   end Message;

   function Open (Channel : View; Queue_Capacity : Positive := 16) return Comms_Metadata_Stream is
      D            : aliased Diagnostic := [others => Interfaces.C.nul];
      R            : aliased C.Size_T := 0;
      Registration : aliased Interfaces.Unsigned_32 := 0;
   begin
      return Result : Comms_Metadata_Stream do
         if C.IR_Channel_Comms_Metadata_Open
              (Channel.Handle,
               C.Size_T (Queue_Capacity),
               Result.Owner.Handle'Access,
               Registration'Access,
               D'Address,
               D'Length,
               R'Access)
           /= C.Success
         then
            raise Provider_Error with Message (D);
         end if;
      end return;
   end Open;

   function Is_Open (Stream : Comms_Metadata_Stream) return Boolean
   is (Stream.Owner.Handle /= C.Null_Comms_Metadata);

   function Receive
     (Stream : Comms_Metadata_Stream; Timeout_Milliseconds : Natural := 0) return Comms_Test_Report
   is
      Raw  : aliased C.IR_Channel_Comms_Test_Report_V1;
      D    : aliased Diagnostic := [others => Interfaces.C.nul];
      R    : aliased C.Size_T := 0;
      Code : constant Interfaces.Integer_32 :=
        C.IR_Channel_Comms_Metadata_Receive
          (Stream.Owner.Handle,
           Interfaces.Unsigned_32 (Timeout_Milliseconds),
           Raw'Access,
           D'Address,
           D'Length,
           R'Access);
   begin
      if Code = C.Timeout then
         raise Timeout_Error with "common metadata receive timed out";
      elsif Code = C.Stream_Stopped then
         raise Stream_Stopped with "common metadata stream stopped";
      elsif Code /= C.Success then
         raise Provider_Error with Message (D);
      end if;
      return (Command_ID (Raw.Command_ID), Comms_Request_ID (Raw.Request_ID));
   end Receive;

   function Counters (Stream : Comms_Metadata_Stream) return Metadata_Counters is
      Raw : aliased C.Metadata_Counters_V1;
      D   : aliased Diagnostic := [others => Interfaces.C.nul];
      R   : aliased C.Size_T := 0;
   begin
      if C.IR_Channel_Comms_Metadata_Get_Counters
           (Stream.Owner.Handle, Raw'Access, D'Address, D'Length, R'Access)
        /= C.Success
      then
         raise Provider_Error with Message (D);
      end if;
      return
        (Counter (Raw.Events_Received),
         Counter (Raw.Events_Dropped_Queue_Full),
         Counter (Raw.Malformed_Or_Unsupported));
   end Counters;

   procedure Close (Stream : in out Comms_Metadata_Stream) is
      D : aliased Diagnostic := [others => Interfaces.C.nul];
      R : aliased C.Size_T := 0;
   begin
      if C.IR_Channel_Comms_Metadata_Close
           (Stream.Owner.Handle'Access, D'Address, D'Length, R'Access)
        /= C.Success
      then
         raise Provider_Error with Message (D);
      end if;
   end Close;

   overriding
   procedure Finalize (Owner : in out Stream_Owner) is
      Ignored : Interfaces.Integer_32;
   begin
      Ignored :=
        C.IR_Channel_Comms_Metadata_Close (Owner.Handle'Access, System.Null_Address, 0, null);
   exception
      when others =>
         Owner.Handle := C.Null_Comms_Metadata;
   end Finalize;
end AMS.MEL.IR.Channel.Metadata;
