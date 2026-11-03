with AMS.MEL.IR.System_Track_Data_Conversion;
with Interfaces.C;
with System;

package body AMS.MEL.IR.C2.System_Data is
   package C renames AMS.MEL_C_API;
   package Sys renames AMS.MEL.IR.Track.System_Data;
   use type Interfaces.Integer_32;
   use type Interfaces.Unsigned_32;
   use type Interfaces.C.char;
   use type Interfaces.C.size_t;
   use type C.C2_Handle;
   use type C.C2_System_Response_Request_Handle;

   type Diagnostic is array (C.Size_T range <>) of aliased Interfaces.C.char with Convention => C;
   subtype Fixed_Diagnostic is Diagnostic (0 .. 511);

   function Message (Value : Diagnostic) return String is
      Last : Natural := 0;
   begin
      while Last < Value'Length and then Value (Value'First + C.Size_T (Last)) /= Interfaces.C.nul
      loop
         Last := Last + 1;
      end loop;
      declare
         Result : String (1 .. Last);
      begin
         for I in Result'Range loop
            Result (I) :=
              Character'Val (Interfaces.C.char'Pos (Value (Value'First + C.Size_T (I - 1))));
         end loop;
         return Result;
      end;
   end Message;

   function Failure (Value : Diagnostic) return String is
      Text : constant String := Message (Value);
   begin
      return
        (if Text'Length = 0 then "native IR C2 SystemTrackDataResponse operation failed" else Text);
   end Failure;

   Empty : constant C.IR_C2_System_Response_Result_V1 :=
     (Status => (0, 0, 0, (System.Null_Address, 0)), Error_Code => 0);

   function Command_ID (Status : Command_Status) return Interfaces.Unsigned_32
   is (Status.Status_Command_ID);
   function State (Status : Command_Status) return Command_State
   is (Status.Status_State);
   function Reason (Status : Command_Status) return Cannot_Comply
   is (Status.Status_Reason);
   function Reason_Description (Status : Command_Status) return String
   is (US.To_String (Status.Status_Text));

   function Status (Result : Response_Result) return Response_Outcome
   is (Result.Result_Status);
   function Command (Result : Response_Result) return Command_Status
   is (Result.Result_Command);
   function Rejection_Code (Result : Response_Result) return Response_Error_Code
   is (Result.Result_Code);
   function Description (Result : Response_Result) return String
   is (US.To_String (Result.Result_Text));

   function Is_Open (Request : Response_Request) return Boolean
   is (Request.Owner.Handle /= C.Null_C2_System_Response_Request);

   function Submit_System_Track_Data_Response
     (Channel : Control_Channel; Value : System_Track_Data_Response) return Response_Request
   is
      --  Shared state-free value conversion; the C2 handle and C2 export only.
      Raw      : aliased constant C.IR_System_Track_Data_Response_V1 :=
        AMS.MEL.IR.System_Track_Data_Conversion.To_Raw (Value);
      D        : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      Required : aliased C.Size_T := 0;
   begin
      if Channel.Handle = C.Null_C2 then
         raise Provider_Error with "IR C2 channel is closed";
      end if;
      return Result : Response_Request do
         declare
            Code : constant Interfaces.Integer_32 :=
              C.IR_C2_Submit_System_Track_Data_Response
                (Channel.Handle,
                 Raw'Access,
                 Result.Owner.Handle'Access,
                 D'Address,
                 D'Length,
                 Required'Access);
         begin
            Check_Submission (Code, Failure (D));
         end;
      end return;
   end Submit_System_Track_Data_Response;

   --  Copies a request-owned C string view into Ada-owned storage. No C
   --  pointer escapes the Wait call.
   function Owned_Text (Value : C.String_View_V1) return US.Unbounded_String is
      use type System.Address;
      type Bytes is array (C.Size_T range <>) of aliased Interfaces.C.char with Convention => C;
   begin
      if Value.Data = System.Null_Address or else Value.Size = 0 then
         return US.Null_Unbounded_String;
      end if;
      declare
         Source : Bytes (0 .. Value.Size - 1)
         with Import, Convention => C, Address => Value.Data;
         Result : String (1 .. Natural (Value.Size));
      begin
         for I in Result'Range loop
            Result (I) := Character'Val (Interfaces.C.char'Pos (Source (C.Size_T (I - 1))));
         end loop;
         return US.To_Unbounded_String (Result);
      end;
   end Owned_Text;

   --  The native adapter already rejected unknown provider values; an
   --  out-of-range value here means the ABI contract itself was violated.
   function To_Command_Status (Raw : C.IR_Command_Status_V1) return Command_Status is
   begin
      if Raw.State > 4 then
         raise Provider_Error with "native IR C2 system response returned unknown CommandState";
      elsif Raw.Reason_ID > 46 then
         raise Provider_Error with "native IR C2 system response returned unknown CannotComply";
      end if;
      return
        (Status_Command_ID => Raw.Command_ID,
         Status_State      => Command_State'Enum_Val (Raw.State),
         Status_Reason     => Cannot_Comply'Enum_Val (Raw.Reason_ID),
         Status_Text       => Owned_Text (Raw.Reason_Description));
   end To_Command_Status;

   function Wait (Request : Response_Request; Timeout_Milliseconds : Natural) return Response_Result
   is
      Raw      : aliased C.IR_C2_System_Response_Result_V1 := Empty;
      D        : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      Required : aliased C.Size_T := 0;
      Code     : Interfaces.Integer_32;
   begin
      if Request.Owner.Handle = C.Null_C2_System_Response_Request then
         raise Provider_Error with "IR C2 SystemTrackDataResponse request is closed";
      end if;
      Code :=
        C.IR_C2_System_Response_Request_Wait
          (Request.Owner.Handle,
           Interfaces.Unsigned_32 (Timeout_Milliseconds),
           Raw'Access,
           D'Address,
           D'Length,
           Required'Access);
      if Code = C.Timeout then
         raise Timeout_Error with "IR C2 SystemTrackDataResponse request timed out";
      elsif Code = C.Success then
         --  The reason text is copied now, while the request owns it.
         return
           (Result_Status  => Sys.Success,
            Result_Command => To_Command_Status (Raw.Status),
            Result_Code    => Sys.None,
            Result_Text    => US.Null_Unbounded_String);
      elsif Code = C.Command_Rejected then
         if Raw.Error_Code > 8 then
            raise Provider_Error
              with "native IR C2 SystemTrackDataResponse returned unknown MEL error code";
         end if;
         declare
            Text : US.Unbounded_String := US.To_Unbounded_String (Message (D));
         begin
            --  Long text is recovered from the cached terminal result with an
            --  exact buffer; the command is never resent.
            if Required > D'Length then
               declare
                  Complete       : aliased Diagnostic (0 .. Required - 1) :=
                    [others => Interfaces.C.nul];
                  Retry_Raw      : aliased C.IR_C2_System_Response_Result_V1 := Empty;
                  Retry_Required : aliased C.Size_T := 0;
                  Retry_Code     : constant Interfaces.Integer_32 :=
                    C.IR_C2_System_Response_Request_Wait
                      (Request.Owner.Handle,
                       0,
                       Retry_Raw'Access,
                       Complete'Address,
                       Complete'Length,
                       Retry_Required'Access);
               begin
                  if Retry_Code /= C.Command_Rejected
                    or else Retry_Raw.Error_Code /= Raw.Error_Code
                    or else Retry_Required /= Required
                  then
                     raise Provider_Error
                       with
                         "native IR C2 SystemTrackDataResponse rejection changed during "
                         & "diagnostic retry";
                  end if;
                  Text := US.To_Unbounded_String (Message (Complete));
               end;
            end if;
            return
              (Result_Status  => Sys.Rejected,
               Result_Command => <>,
               Result_Code    => Response_Error_Code'Val (Raw.Error_Code),
               Result_Text    => Text);
         end;
      else
         raise Provider_Error with Failure (D);
      end if;
   end Wait;

   procedure Close (Request : in out Response_Request) is
      D    : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      R    : aliased C.Size_T := 0;
      Code : constant Interfaces.Integer_32 :=
        C.IR_C2_System_Response_Request_Close
          (Request.Owner.Handle'Access, D'Address, D'Length, R'Access);
   begin
      if Code /= C.Success then
         raise Provider_Error with Failure (D);
      end if;
   end Close;

   overriding
   procedure Finalize (Request : in out Response_Request_Owner) is
      Ignored : Interfaces.Integer_32;
   begin
      --  Drops only the public observer; pending native work is not cancelled.
      Ignored :=
        C.IR_C2_System_Response_Request_Close (Request.Handle'Access, System.Null_Address, 0, null);
   exception
      when others =>
         Request.Handle := C.Null_C2_System_Response_Request;
   end Finalize;
end AMS.MEL.IR.C2.System_Data;
