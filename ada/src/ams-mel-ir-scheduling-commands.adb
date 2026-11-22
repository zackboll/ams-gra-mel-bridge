with Ada.Unchecked_Conversion;
with Ada.Unchecked_Deallocation;
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

   function Exec_Track (Channel : Scheduling_Channel; Value : Track_Task) return Track_Request is
      Raw : aliased C.IR_Track_Task_V1 :=
        (Track_Parameters =>
           (Duration_NS => Value.Parameters.Duration_NS,
            Count       => Value.Parameters.Count,
            Targets     => [others => (0, 0.0, 0.0, 0.0, 0.0)]),
         Command_ID       => Value.Command_ID,
         Info             =>
           (Value.Info.System_Time_NS,
            Value.Info.Request_ID,
            Task_List_Action'Pos (Value.Info.Action),
            Value.Info.Precedence_Within_Priority,
            Value.Info.Priority));
      D   : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      R   : aliased C.Size_T := 0;
   begin
      for Index in Value.Parameters.Targets'Range loop
         declare
            Target : Track_Target renames Value.Parameters.Targets (Index);
         begin
            Raw.Track_Parameters.Targets (Index - 1) :=
              (Target.Target_ID,
               Interfaces.C.double (Target.Location_Azimuth_Rad),
               Interfaces.C.double (Target.Location_Elevation_Rad),
               Interfaces.C.double (Target.Rate_Azimuth_Rad_Per_Second),
               Interfaces.C.double (Target.Rate_Elevation_Rad_Per_Second));
         end;
      end loop;
      return Result : Track_Request do
         Check_Submission
           (C.IR_Scheduling_Exec_Track
              (Channel.Handle,
               Raw'Access,
               Result.Owner.Handle'Access,
               D'Address,
               D'Length,
               R'Access),
            Failure_Message (D));
      end return;
   end Exec_Track;

   function Wait (Request : Track_Request; Timeout_Milliseconds : Natural) return Track_Result
   is (Owner => Wait (Request.Owner, Timeout_Milliseconds));

   function Status (Result : Track_Result) return Outcome
   is (Status (Result.Owner));

   function Value (Result : Track_Result) return Command_Return
   is (Value (Result.Owner));

   function Rejection_Code (Result : Track_Result) return Error_Code
   is (Rejection_Code (Result.Owner));

   function Description (Result : Track_Result) return String
   is (Description (Result.Owner));

   procedure Close (Request : in out Track_Request) is
   begin
      Close (Request.Owner);
   end Close;

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
      if Result.Result_Status /= AMS.MEL.IR.Channel.Rejected then
         raise Provider_Error with "Scheduling task result is not rejected";
      end if;
      return Result.Result_Code;
   end Rejection_Code;
   function Description (Result : Abort_Result) return String is
   begin
      if Result.Result_Status /= AMS.MEL.IR.Channel.Rejected then
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
            Result_Code   => AMS.MEL.IR.Channel.None,
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
              (Result_Status => AMS.MEL.IR.Channel.Rejected,
               Result_Value  => Return_Success,
               Result_Code   => Error_Code'Val (Raw.Error_Code),
               Result_Text   => Text);
         end;
      else
         raise Provider_Error with Failure_Message (D);
      end if;
   end Wait;

   function To_Double is new Ada.Unchecked_Conversion (Long_Float, Interfaces.C.double);

   function To_Raw (Value : Range_Az_El) return C.IR_Range_Az_El_V1
   is (To_Double (Value.Range_M),
       To_Double (Value.Azimuth_Rad),
       To_Double (Value.Elevation_Rad),
       To_Double (Value.Range_Rate_MPS));

   type Raw_Track_Array is array (Positive range <>) of C.IR_Track_Data_V1 with Convention => C;
   type Raw_Track_Access is access Raw_Track_Array;
   procedure Free is new Ada.Unchecked_Deallocation (Raw_Track_Array, Raw_Track_Access);

   function Send_Prioritized_Tracks
     (Channel : Scheduling_Channel; Value : Prioritized_Track_List_Cmd) return Track_List_Request
   is
      Data : Raw_Track_Access;
      Raw  : aliased C.IR_Prioritized_Track_List_Cmd_V1 :=
        (Value.Schedule_Time_NS,
         Value.Time_To_Live_NS,
         Value.Command_ID,
         Value.Track_Count,
         (System.Null_Address, C.Size_T (Value.Length)));
      D    : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
   begin
      if Value.Length /= 0 then
         Data := new Raw_Track_Array (Value.Tracks'Range);
         for Index in Value.Tracks'Range loop
            declare
               Item : Track_Data renames Value.Tracks (Index);
            begin
               Data (Index) :=
                 (Item.Timestamp_NS,
                  Item.Track_ID,
                  Item.IRST_Priority,
                  Item.Reserved_Priority,
                  Item.IRST_Precedence,
                  Item.Reserved_Precedence,
                  Boolean'Pos (Item.Is_Skippable),
                  Boolean'Pos (Item.Maintain_Outside_FOS),
                  Item.IRST_Track_ID,
                  To_Raw (Item.Position),
                  To_Raw (Item.Velocity),
                  Metadata.Task_Kind'Enum_Rep (Item.Task_Kind),
                  Item.Command_ID);
            end;
         end loop;
         Raw.Tracks.Data := Data (Data'First)'Address;
      end if;
      return Result : Track_List_Request do
         declare
            Code : constant Interfaces.Integer_32 :=
              C.IR_Scheduling_Send_Prioritized_Tracks
                (Channel.Handle, Raw'Access, Result.Handle'Access, D'Address, D'Length, null);
         begin
            Free (Data);
            Check_Submission (Code, Failure_Message (D));
         end;
      end return;
   exception
      when Storage_Error =>
         Free (Data);
         raise Resource_Exhausted with "could not allocate prioritized track input";
      when others =>
         Free (Data);
         raise;
   end Send_Prioritized_Tracks;

   function Status (Result : Track_List_Result) return Outcome is
   begin
      if not Result.Observed then
         raise Provider_Error with "no TrackListResponse observed";
      end if;
      return Result.Result_Status;
   end Status;

   function Value (Result : Track_List_Result) return Track_List_Response is
   begin
      if Status (Result) /= Success then
         raise Provider_Error with "prioritized track request contains a MEL error";
      end if;
      return Result.Result_Value;
   end Value;

   function Rejection_Code (Result : Track_List_Result) return Error_Code is
   begin
      if Status (Result) /= AMS.MEL.IR.Channel.Rejected then
         raise Provider_Error with "prioritized track request contains no MEL error";
      end if;
      return Result.Result_Code;
   end Rejection_Code;

   function Description (Result : Track_List_Result) return String is
   begin
      if Status (Result) /= AMS.MEL.IR.Channel.Rejected then
         raise Provider_Error with "prioritized track request contains no MEL error";
      end if;
      return US.To_String (Result.Result_Text);
   end Description;

   procedure Close (Request : in out Track_List_Request) is
      Code : constant Interfaces.Integer_32 :=
        C.IR_Track_List_Request_Close (Request.Handle'Access, System.Null_Address, 0, null);
   begin
      if Code /= C.Success then
         raise Provider_Error with "could not close TrackListResponse observer";
      end if;
   end Close;

   overriding
   procedure Finalize (Request : in out Track_List_Request) is
   begin
      Close (Request);
   exception
      when others =>
         Request.Handle := C.Null_Track_List_Request;
   end Finalize;

   function Wait
     (Request : Track_List_Request; Timeout_Milliseconds : Natural) return Track_List_Result
   is
      Raw      : aliased C.IR_Track_List_Result_V1 := (0, 0);
      D        : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      Required : aliased C.Size_T := 0;
      Code     : constant Interfaces.Integer_32 :=
        C.IR_Track_List_Request_Wait
          (Request.Handle,
           Interfaces.Unsigned_32 (Timeout_Milliseconds),
           Raw'Access,
           D'Address,
           D'Length,
           Required'Access);

      function Copy_Description (Buffer : Diagnostic_Array) return US.Unbounded_String is
         Text : String (1 .. Natural (Required - 1));
      begin
         if Buffer (Required - 1) /= Interfaces.C.nul then
            raise Provider_Error with "missing TrackListResponse diagnostic terminator";
         end if;
         for Index in Text'Range loop
            Text (Index) := Character'Val (Interfaces.C.char'Pos (Buffer (C.Size_T (Index - 1))));
         end loop;
         return US.To_Unbounded_String (Text);
      end Copy_Description;

      function Rejection return US.Unbounded_String is
         type Diagnostic_Access is access Diagnostic_Array;
         procedure Release is new Ada.Unchecked_Deallocation (Diagnostic_Array, Diagnostic_Access);
         Full           : Diagnostic_Access;
         Retry_Raw      : aliased C.IR_Track_List_Result_V1 := (0, 0);
         Retry_Required : aliased C.Size_T := 0;
      begin
         if Required = 0 or else Required - 1 > C.Size_T (Natural'Last) then
            raise Provider_Error with "invalid TrackListResponse diagnostic length";
         elsif Required <= D'Length then
            return Copy_Description (D);
         end if;
         Full := new Diagnostic_Array (0 .. Required - 1);
         if C.IR_Track_List_Request_Wait
              (Request.Handle,
               0,
               Retry_Raw'Access,
               Full.all'Address,
               Full'Length,
               Retry_Required'Access)
           /= C.Command_Rejected
           or else Retry_Raw.Error_Code /= Raw.Error_Code
           or else Retry_Required /= Required
         then
            raise Provider_Error with "cached TrackListResponse diagnostic changed";
         end if;
         return Text : constant US.Unbounded_String := Copy_Description (Full.all) do
            Release (Full);
         end return;
      exception
         when others =>
            Release (Full);
            raise;
      end Rejection;
   begin
      if Code = C.Timeout then
         raise Timeout_Error with "TrackListResponse is still pending";
      elsif Code = C.Success
        and then Raw.Value <= Track_List_Response'Pos (Track_List_Response'Last)
      then
         return
           (True,
            Success,
            Track_List_Response'Val (Raw.Value),
            AMS.MEL.IR.Channel.None,
            US.Null_Unbounded_String);
      elsif Code = C.Command_Rejected and then Raw.Error_Code <= Error_Code'Pos (Error_Code'Last)
      then
         return
           (True, AMS.MEL.IR.Channel.Rejected, None, Error_Code'Val (Raw.Error_Code), Rejection);
      else
         raise Provider_Error with Failure_Message (D);
      end if;
   end Wait;

end AMS.MEL.IR.Scheduling.Commands;
