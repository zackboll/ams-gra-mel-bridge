with AMS.MEL.IR.Scan_Conversion;
with Interfaces.C;
with System;

package body AMS.MEL.IR.Scheduling.Commands is
   package C renames AMS.MEL_C_API;
   use AMS.MEL.IR.Channel;
   use type Interfaces.Integer_32;
   use type Interfaces.Unsigned_32;
   use type Interfaces.C.char;
   use type Interfaces.C.size_t;

   pragma
     Compile_Time_Error
       (Long_Float'Size /= 64 or else Long_Float'Machine_Mantissa /= 53,
        "Scheduling stare angles require IEEE-754 binary64 Long_Float");

   type Diagnostic_Array is array (C.Size_T range <>) of aliased Interfaces.C.char
   with Convention => C;
   subtype Fixed_Diagnostic is Diagnostic_Array (0 .. 511);

   function Message (Buffer : Diagnostic_Array) return String is
      Length : Natural := 0;
   begin
      while Length < Buffer'Length
        and then Buffer (Buffer'First + C.Size_T (Length)) /= Interfaces.C.nul
      loop
         Length := Length + 1;
      end loop;
      declare
         Result : String (1 .. Length);
      begin
         for Index in Result'Range loop
            Result (Index) :=
              Character'Val (Interfaces.C.char'Pos (Buffer (Buffer'First + C.Size_T (Index - 1))));
         end loop;
         return Result;
      end;
   end Message;

   function Failure_Message (Buffer : Diagnostic_Array) return String is
      Value : constant String := Message (Buffer);
   begin
      return (if Value'Length = 0 then "native common Channel operation failed" else Value);
   end Failure_Message;

   function Exec_Scan (Channel : Scheduling_Channel; Value : Scan_Task) return Scan_Request is
      Raw : aliased C.IR_Scan_Task_V1 :=
        (Scan_Parameters => AMS.MEL.IR.Scan_Conversion.To_Raw (Value.Parameters),
         Command_ID      => Value.Command_ID,
         Info            =>
           (Value.Info.System_Time_NS,
            Value.Info.Request_ID,
            Task_List_Action'Pos (Value.Info.Action),
            Value.Info.Precedence_Within_Priority,
            Value.Info.Priority));
      D   : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      R   : aliased C.Size_T := 0;
   begin
      return Result : Scan_Request do
         Check_Submission
           (C.IR_Scheduling_Exec_Scan
              (Channel.Handle,
               Raw'Access,
               Result.Owner.Handle'Access,
               D'Address,
               D'Length,
               R'Access),
            Failure_Message (D));
      end return;
   end Exec_Scan;

   function Wait (Request : Scan_Request; Timeout_Milliseconds : Natural) return Scan_Result
   is (Owner => Wait (Request.Owner, Timeout_Milliseconds));

   function Status (Result : Scan_Result) return Outcome
   is (Status (Result.Owner));

   function Value (Result : Scan_Result) return Command_Return
   is (Value (Result.Owner));

   function Rejection_Code (Result : Scan_Result) return Error_Code
   is (Rejection_Code (Result.Owner));

   function Description (Result : Scan_Result) return String
   is (Description (Result.Owner));

   procedure Close (Request : in out Scan_Request) is
   begin
      Close (Request.Owner);
   end Close;

   function Exec_Stare (Channel : Scheduling_Channel; Value : Stare_Task) return Stare_Request is
      Raw : aliased C.IR_Stare_Task_V1 :=
        (Center_Azimuth_Rad   => Interfaces.C.double (Value.Parameters.Center_Azimuth_Rad),
         Center_Elevation_Rad => Interfaces.C.double (Value.Parameters.Center_Elevation_Rad),
         Duration_NS          => Value.Parameters.Duration_NS,
         Elevation_Frame      => Coordinate_Frame_Reference'Pos (Value.Parameters.Elevation_Frame),
         Azimuth_Frame        => Coordinate_Frame_Reference'Pos (Value.Parameters.Azimuth_Frame),
         Command_ID           => Value.Command_ID,
         Info                 =>
           (Value.Info.System_Time_NS,
            Value.Info.Request_ID,
            Task_List_Action'Pos (Value.Info.Action),
            Value.Info.Precedence_Within_Priority,
            Value.Info.Priority));
      D   : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      R   : aliased C.Size_T := 0;
   begin
      return Result : Stare_Request do
         Check_Submission
           (C.IR_Scheduling_Exec_Stare
              (Channel.Handle,
               Raw'Access,
               Result.Owner.Handle'Access,
               D'Address,
               D'Length,
               R'Access),
            Failure_Message (D));
      end return;
   end Exec_Stare;

   function Wait (Request : Stare_Request; Timeout_Milliseconds : Natural) return Stare_Result
   is (Owner => Wait (Request.Owner, Timeout_Milliseconds));

   function Status (Result : Stare_Result) return Outcome
   is (Status (Result.Owner));

   function Value (Result : Stare_Result) return Command_Return
   is (Value (Result.Owner));

   function Rejection_Code (Result : Stare_Result) return Error_Code
   is (Rejection_Code (Result.Owner));

   function Description (Result : Stare_Result) return String
   is (Description (Result.Owner));

   procedure Close (Request : in out Stare_Request) is
   begin
      Close (Request.Owner);
   end Close;

   function Exec_Abort (Channel : Scheduling_Channel; Info : Task_Info) return Abort_Request is
      Raw : aliased C.IR_Task_Info_V1 :=
        (Info.System_Time_NS,
         Info.Request_ID,
         Interfaces.Unsigned_32 (Task_List_Action'Pos (Info.Action)),
         Info.Precedence_Within_Priority,
         Info.Priority);
      D   : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      R   : aliased C.Size_T := 0;
   begin
      return Result : Abort_Request do
         declare
            Code : constant Interfaces.Integer_32 :=
              C.IR_Scheduling_Exec_Abort
                (Channel.Handle, Raw'Access, Result.Handle'Access, D'Address, D'Length, R'Access);
         begin
            Check_Submission (Code, Failure_Message (D));
         end;
      end return;
   end Exec_Abort;

   function Status (Result : Abort_Result) return Outcome
   is (Result.Result_Status);
   function Value (Result : Abort_Result) return Command_Return is
   begin
      if Result.Result_Status /= Success then
         raise Provider_Error with "Scheduling task result is rejected";
      end if;
      return Result.Result_Value;
   end Value;
   function Rejection_Code (Result : Abort_Result) return Error_Code is
   begin
      if Result.Result_Status /= Rejected then
         raise Provider_Error with "Scheduling task result is not rejected";
      end if;
      return Result.Result_Code;
   end Rejection_Code;
   function Description (Result : Abort_Result) return String is
   begin
      if Result.Result_Status /= Rejected then
         raise Provider_Error with "Scheduling task result is not rejected";
      end if;
      return US.To_String (Result.Result_Text);
   end Description;

   procedure Close (Request : in out Abort_Request) is
      D    : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      R    : aliased C.Size_T := 0;
      Code : constant Interfaces.Integer_32 :=
        C.IR_Return_Request_Close (Request.Handle'Access, D'Address, D'Length, R'Access);
   begin
      if Code /= C.Success then
         raise Provider_Error with Failure_Message (D);
      end if;
   end Close;

   overriding
   procedure Finalize (Request : in out Abort_Request) is
      Ignored : Interfaces.Integer_32;
   begin
      Ignored := C.IR_Return_Request_Close (Request.Handle'Access, System.Null_Address, 0, null);
   exception
      when others =>
         Request.Handle := C.Null_Return_Request;
   end Finalize;

   function Wait (Request : Abort_Request; Timeout_Milliseconds : Natural) return Abort_Result is
      Raw  : aliased C.IR_Return_Result_V1 := (Value => 0, Error_Code => 0);
      D    : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      R    : aliased C.Size_T := 0;
      Code : constant Interfaces.Integer_32 :=
        C.IR_Return_Request_Wait
          (Request.Handle,
           Interfaces.Unsigned_32 (Timeout_Milliseconds),
           Raw'Access,
           D'Address,
           D'Length,
           R'Access);
   begin
      if Code = C.Timeout then
         raise Timeout_Error with "Scheduling task request timed out";
      elsif Code = C.Success then
         if Raw.Value > Command_Return'Pos (Command_Return'Last) then
            raise Provider_Error with "native common Channel returned unknown IR Return value";
         end if;
         return
           (Result_Status => Success,
            Result_Value  => Command_Return'Val (Raw.Value),
            Result_Code   => None,
            Result_Text   => US.Null_Unbounded_String);
      elsif Code = C.Command_Rejected then
         if Raw.Error_Code > Error_Code'Pos (Error_Code'Last) then
            raise Provider_Error with "native common Channel returned unknown MEL error code";
         end if;
         declare
            Text : US.Unbounded_String := US.To_Unbounded_String (Message (D));
         begin
            if R > D'Length then
               --  Allocate exactly the required diagnostic, repeat the cached
               --  Wait, and fail closed if the cached rejection changed.
               declare
                  Complete       : aliased Diagnostic_Array (0 .. R - 1) :=
                    [others => Interfaces.C.nul];
                  Retry_Raw      : aliased C.IR_Return_Result_V1 := (Value => 0, Error_Code => 0);
                  Retry_Required : aliased C.Size_T := 0;
                  Retry_Code     : constant Interfaces.Integer_32 :=
                    C.IR_Return_Request_Wait
                      (Request.Handle,
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
                       with "native Scheduling task rejection changed during diagnostic retry";
                  end if;
                  Text := US.To_Unbounded_String (Message (Complete));
               end;
            end if;
            return
              (Result_Status => Rejected,
               Result_Value  => Return_Success,
               Result_Code   => Error_Code'Val (Raw.Error_Code),
               Result_Text   => Text);
         end;
      else
         raise Provider_Error with Failure_Message (D);
      end if;
   end Wait;

end AMS.MEL.IR.Scheduling.Commands;
