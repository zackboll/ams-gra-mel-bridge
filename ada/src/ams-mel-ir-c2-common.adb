with Ada.Unchecked_Conversion;
with AMS.MEL.IR.Capability_Conversion;
with Interfaces.C;
with System;

package body AMS.MEL.IR.C2.Common is
   package C renames AMS.MEL_C_API;
   package V renames IR.Channel;
   use type Interfaces.Integer_32;
   use type Interfaces.C.char;
   use type Interfaces.C.size_t;
   use type Interfaces.Unsigned_32;
   type Diagnostic_Array is array (C.Size_T range <>) of aliased Interfaces.C.char
   with Convention => C;
   subtype Diagnostic is Diagnostic_Array (0 .. 1023);
   function Message (D : Diagnostic_Array) return String is
      N : Natural := 0;
   begin
      while N < D'Length and then D (C.Size_T (N)) /= Interfaces.C.nul loop
         N := N + 1;
      end loop;
      declare
         S : String (1 .. N);
      begin
         for I in S'Range loop
            S (I) := Character'Val (Interfaces.C.char'Pos (D (C.Size_T (I - 1))));
         end loop;
         return (if N = 0 then "native common Channel operation failed" else S);
      end;
   end Message;
   type Cap_Access is access all C.IR_Channel_Capability_V1;
   function To_Cap is new Ada.Unchecked_Conversion (System.Address, Cap_Access);

   function Send_Keep_Alive (Channel : Control_Channel) return Return_Request is
      D : aliased Diagnostic := [others => Interfaces.C.nul];
      R : aliased C.Size_T := 0;
   begin
      return Result : Return_Request do
         declare
            Code : constant Interfaces.Integer_32 :=
              C.IR_C2_Send_Keepalive
                (Channel.Handle, Result.Owner.Handle'Access, D'Address, D'Length, R'Access);
         begin
            Check_Submission (Code, Message (D));
         end;
      end return;
   end Send_Keep_Alive;
   function Submit_Comms_Test
     (Channel    : Control_Channel;
      Channel_ID : V.Comms_Channel_ID;
      Command_ID : C2.Command_ID;
      Request_ID : V.Comms_Request_ID) return Comms_Request
   is
      Raw : aliased C.IR_Channel_Comms_Test_Request_V1 :=
        (Interfaces.Unsigned_32 (Command_ID),
         Interfaces.Unsigned_32 (Channel_ID),
         Interfaces.Unsigned_32 (Request_ID));
      D   : aliased Diagnostic := [others => Interfaces.C.nul];
      R   : aliased C.Size_T := 0;
   begin
      return Result : Comms_Request do
         declare
            Code : constant Interfaces.Integer_32 :=
              C.IR_C2_Submit_Comms_Test
                (Channel.Handle,
                 Raw'Access,
                 Result.Owner.Handle'Access,
                 D'Address,
                 D'Length,
                 R'Access);
         begin
            Check_Submission (Code, Message (D));
         end;
      end return;
   end Submit_Comms_Test;
   function Wait (Request : Comms_Request; Timeout_Milliseconds : Natural) return Comms_Result is
      Raw  : aliased C.IR_Channel_Comms_Test_Result_V1 := (0, 0, 0);
      D    : aliased Diagnostic := [others => Interfaces.C.nul];
      R    : aliased C.Size_T := 0;
      Code : constant Interfaces.Integer_32 :=
        C.IR_Comms_Request_Wait
          (Request.Owner.Handle,
           Interfaces.Unsigned_32 (Timeout_Milliseconds),
           Raw'Access,
           D'Address,
           D'Length,
           R'Access);
   begin
      if Code = C.Success then
         return
           (Success,
            (C2.Command_ID (Raw.Command_ID), V.Comms_Request_ID (Raw.Request_ID)),
            None,
            US.Null_Unbounded_String);
      elsif Code = C.Timeout then
         raise Timeout_Error;
      elsif Code = C.Command_Rejected then
         if R > D'Length then
            declare
               Complete       : aliased Diagnostic_Array (0 .. R - 1) :=
                 [others => Interfaces.C.nul];
               Retry_Raw      : aliased C.IR_Channel_Comms_Test_Result_V1 := (0, 0, 0);
               Retry_Required : aliased C.Size_T := 0;
               Retry          : constant Interfaces.Integer_32 :=
                 C.IR_Comms_Request_Wait
                   (Request.Owner.Handle,
                    0,
                    Retry_Raw'Access,
                    Complete'Address,
                    Complete'Length,
                    Retry_Required'Access);
            begin
               if Retry /= C.Command_Rejected
                 or else Retry_Raw.Error_Code /= Raw.Error_Code
                 or else Retry_Required /= R
               then
                  raise Provider_Error
                    with "native CommsTest rejection changed during diagnostic retry";
               end if;
               return
                 (Rejected,
                  (0, 0),
                  Error_Code'Val (Raw.Error_Code),
                  US.To_Unbounded_String (Message (Complete)));
            end;
         end if;
         return
           (Rejected,
            (0, 0),
            Error_Code'Val (Raw.Error_Code),
            US.To_Unbounded_String (Message (D)));
      else
         raise Provider_Error with Message (D);
      end if;
   end Wait;
   function Status (Result : Comms_Result) return Outcome
   is (Result.Result_Status);
   function Report (Result : Comms_Result) return V.Comms_Test_Report
   is (Result.Result_Report);
   function Rejection_Code (Result : Comms_Result) return Error_Code
   is (Result.Result_Code);
   function Description (Result : Comms_Result) return String
   is (US.To_String (Result.Result_Text));
   procedure Close (Request : in out Comms_Request) is
      D : aliased Diagnostic := [others => Interfaces.C.nul];
      R : aliased C.Size_T := 0;
   begin
      if C.IR_Comms_Request_Close (Request.Owner.Handle'Access, D'Address, D'Length, R'Access)
        /= C.Success
      then
         raise Provider_Error with Message (D);
      end if;
   end Close;
   overriding
   procedure Finalize (Request : in out Comms_Owner) is
      Ignored : Interfaces.Integer_32;
   begin
      Ignored := C.IR_Comms_Request_Close (Request.Handle'Access, System.Null_Address, 0, null);
   exception
      when others =>
         Request.Handle := C.Null_Comms_Request;
   end Finalize;

   function Capabilities (Channel : Control_Channel) return V.Channel_Capability is
      Owner   : aliased C.Capability_Handle := C.Null_Capability;
      Address : aliased System.Address := System.Null_Address;
      D       : aliased Diagnostic := [others => Interfaces.C.nul];
      R       : aliased C.Size_T := 0;
      procedure Release is
         Ignored : Interfaces.Integer_32;
      begin
         Ignored := C.IR_Capability_Close (Owner'Access, System.Null_Address, 0, null);
      end Release;
   begin
      if C.IR_C2_Get_Capabilities (Channel.Handle, Owner'Access, D'Address, D'Length, R'Access)
        /= C.Success
      then
         raise Provider_Error with Message (D);
      end if;
      if C.IR_Capability_View (Owner, Address'Access, D'Address, D'Length, R'Access) /= C.Success
      then
         Release;
         raise Provider_Error with Message (D);
      end if;
      begin
         declare
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
end AMS.MEL.IR.C2.Common;
