with Interfaces.C;
with AMS.MEL_C_API;
with AMS.MEL.IR.Camera_Protection_Conversion;

package body AMS.MEL.IR.Camera_Protection is
   use type Interfaces.Integer_32;
   use type Interfaces.Unsigned_32;
   use type Interfaces.C.char;
   use type Interfaces.C.size_t;
   use type AMS.MEL_C_API.Camera_Protect_Request_Handle;
   package C renames AMS.MEL_C_API;

   type Diagnostic_Array is array (Interfaces.C.size_t range <>) of aliased Interfaces.C.char
   with Convention => C;
   subtype Fixed_Diagnostic is Diagnostic_Array (0 .. 511);

   function Message (Buffer : Diagnostic_Array) return String is
      Length : Natural := 0;
   begin
      while Length < Buffer'Length
        and then Buffer (Buffer'First + Interfaces.C.size_t (Length)) /= Interfaces.C.nul
      loop
         Length := Length + 1;
      end loop;
      declare
         Result : String (1 .. Length);
      begin
         for Index in Result'Range loop
            Result (Index) :=
              Character'Val
                (Interfaces.C.char'Pos (Buffer (Buffer'First + Interfaces.C.size_t (Index - 1))));
         end loop;
         return Result;
      end;
   end Message;

   function Failure (Buffer : Diagnostic_Array) return String is
      Value : constant String := Message (Buffer);
   begin
      return (if Value'Length = 0 then "native IR CameraProtectCmd operation failed" else Value);
   end Failure;

   function Status (Result : Protection_Result) return Outcome
   is (Result.Result_Status);
   function Response (Result : Protection_Result) return Protection_Response
   is (Result.Result_Response);
   function Rejection_Code (Result : Protection_Result) return Error_Code
   is (Result.Result_Code);
   function Description (Result : Protection_Result) return String
   is (US.To_String (Result.Result_Text));

   function Is_Open (Request : Protection_Request) return Boolean
   is (Camera_Protection_Request_Owner (Request).Handle /= C.Null_Camera_Protect_Request);

   function Wait
     (Request : Protection_Request; Timeout_Milliseconds : Natural) return Protection_Result
   is
      Empty    : constant C.IR_Camera_Protect_Result_V1 :=
        (Response   => (Command_ID => 0, Response_Time_NS => 0, Command_Response => 0),
         Error_Code => 0);
      Raw      : aliased C.IR_Camera_Protect_Result_V1 := Empty;
      D        : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      Required : aliased C.Size_T := 0;
      Code     : Interfaces.Integer_32;
   begin
      if Camera_Protection_Request_Owner (Request).Handle = C.Null_Camera_Protect_Request then
         raise Provider_Error with "IR CameraProtectCmd request is closed";
      end if;
      Code :=
        C.IR_Camera_Protect_Request_Wait
          (Camera_Protection_Request_Owner (Request).Handle,
           Interfaces.Unsigned_32 (Timeout_Milliseconds),
           Raw'Access,
           D'Address,
           D'Length,
           Required'Access);
      if Code = C.Timeout then
         raise Timeout_Error with "IR CameraProtectCmd request timed out";
      elsif Code = C.Success then
         return
           (Result_Status   => AMS.MEL.IR.Channel.Success,
            Result_Response => Camera_Protection_Conversion.To_Response (Raw.Response),
            Result_Code     => AMS.MEL.IR.Channel.None,
            Result_Text     => US.Null_Unbounded_String);
      elsif Code = C.Command_Rejected then
         if Raw.Error_Code > 8 then
            raise Provider_Error with "native IR CameraProtectCmd returned unknown MEL error code";
         end if;
         declare
            Text : US.Unbounded_String := US.To_Unbounded_String (Message (D));
         begin
            if Required > D'Length then
               declare
                  Complete       : aliased Diagnostic_Array (0 .. Required - 1) :=
                    [others => Interfaces.C.nul];
                  Retry_Raw      : aliased C.IR_Camera_Protect_Result_V1 := Empty;
                  Retry_Required : aliased C.Size_T := 0;
                  Retry_Code     : constant Interfaces.Integer_32 :=
                    C.IR_Camera_Protect_Request_Wait
                      (Camera_Protection_Request_Owner (Request).Handle,
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
                       with "native IR CameraProtectCmd rejection changed during diagnostic retry";
                  end if;
                  Text := US.To_Unbounded_String (Message (Complete));
               end;
            end if;
            return
              (Result_Status   => AMS.MEL.IR.Channel.Rejected,
               Result_Response => (others => <>),
               Result_Code     => Error_Code'Val (Raw.Error_Code),
               Result_Text     => Text);
         end;
      else
         raise Provider_Error with Failure (D);
      end if;
   end Wait;

   procedure Close (Request : in out Protection_Request) is
      D    : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      R    : aliased C.Size_T := 0;
      Code : constant Interfaces.Integer_32 :=
        C.IR_Camera_Protect_Request_Close
          (Camera_Protection_Request_Owner (Request).Handle'Access, D'Address, D'Length, R'Access);
   begin
      if Code /= C.Success then
         raise Provider_Error with Failure (D);
      end if;
   end Close;
end AMS.MEL.IR.Camera_Protection;
