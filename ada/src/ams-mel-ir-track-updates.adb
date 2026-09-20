with Interfaces.C;
with System;

package body AMS.MEL.IR.Track.Updates is
   package C renames AMS.MEL_C_API;
   use type Interfaces.Integer_32;
   use type Interfaces.Unsigned_32;
   use type Interfaces.C.char;
   use type Interfaces.C.size_t;
   use type C.Track_Handle;
   use type C.Track_Update_Request_Handle;
   use type System.Address;

   type Diagnostic is array (C.Size_T range <>) of aliased Interfaces.C.char with Convention => C;
   subtype Fixed_Diagnostic is Diagnostic (0 .. 511);

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
         return (if Result'Length = 0 then "native IR Track update operation failed" else Result);
      end;
   end Message;

   function String_View (Value : String) return C.String_View_V1
   is (Data => (if Value'Length = 0 then System.Null_Address else Value'Address),
       Size => C.Size_T (Value'Length));

   --  Copies a request-owned C string view into Ada-owned storage. No C
   --  pointer escapes the Wait call.
   function Owned_Text (Value : C.String_View_V1) return US.Unbounded_String is
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

   function Command_ID (Status : Command_Status) return Interfaces.Unsigned_32
   is (Status.Status_Command_ID);
   function State (Status : Command_Status) return Command_State
   is (Status.Status_State);
   function Reason (Status : Command_Status) return Cannot_Comply
   is (Status.Status_Reason);
   function Reason_Description (Status : Command_Status) return String
   is (US.To_String (Status.Status_Text));

   function Status (Result : Update_Result) return Update_Outcome
   is (Result.Result_Status);
   function Command (Result : Update_Result) return Command_Status
   is (Result.Result_Command);
   function Rejection_Code (Result : Update_Result) return Update_Error_Code
   is (Result.Result_Code);
   function Description (Result : Update_Result) return String
   is (US.To_String (Result.Result_Text));

   function Is_Open (Request : Update_Request) return Boolean
   is (Request.Owner.Handle /= C.Null_Track_Update_Request);

   function Submit (Channel : Track_Channel; Value : Track_Data_Update) return Update_Request is
      --  Every borrowed label is kept alive for the whole native call; the
      --  native adapter copies all of them before it returns.
      Capability_Label : aliased constant String := US.To_String (Value.Capability_UUID.Label);
      Activity_Label   : aliased constant String := US.To_String (Value.Activity_UUID.Label);
      Entity_Label     : aliased constant String := US.To_String (Value.Entity_UUID.Label);
      Raw              : aliased C.IR_Track_Data_Update_V1 :=
        (Platform_ID                 => Value.Platform_ID,
         Capability_UUID             =>
           (C.Byte_Array_16 (Value.Capability_UUID.Value), String_View (Capability_Label)),
         Activity_UUID               =>
           (C.Byte_Array_16 (Value.Activity_UUID.Value), String_View (Activity_Label)),
         Track_ID                    => Value.Track_ID,
         Entity_UUID                 =>
           (C.Byte_Array_16 (Value.Entity_UUID.Value), String_View (Entity_Label)),
         Track_Status                => Track_Status'Enum_Rep (Value.Status),
         Time_Of_Validity_Seconds    => Interfaces.C.double (Value.Time_Of_Validity_Seconds),
         Time_Of_Last_Update_Seconds => Interfaces.C.double (Value.Time_Of_Last_Update_Seconds),
         Track_Position_ECEF         =>
           (Interfaces.C.double (Value.Position_ECEF.X),
            Interfaces.C.double (Value.Position_ECEF.Y),
            Interfaces.C.double (Value.Position_ECEF.Z)),
         Track_Velocity_ECEF         =>
           (Interfaces.C.double (Value.Velocity_ECEF.X),
            Interfaces.C.double (Value.Velocity_ECEF.Y),
            Interfaces.C.double (Value.Velocity_ECEF.Z)),
         Covariance                  =>
           (XX    => Interfaces.C.double (Value.Covariance.XX),
            XY    => Interfaces.C.double (Value.Covariance.XY),
            XZ    => Interfaces.C.double (Value.Covariance.XZ),
            X_VX  => Interfaces.C.double (Value.Covariance.X_VX),
            X_VY  => Interfaces.C.double (Value.Covariance.X_VY),
            X_VZ  => Interfaces.C.double (Value.Covariance.X_VZ),
            YY    => Interfaces.C.double (Value.Covariance.YY),
            YZ    => Interfaces.C.double (Value.Covariance.YZ),
            Y_VX  => Interfaces.C.double (Value.Covariance.Y_VX),
            Y_VY  => Interfaces.C.double (Value.Covariance.Y_VY),
            Y_VZ  => Interfaces.C.double (Value.Covariance.Y_VZ),
            ZZ    => Interfaces.C.double (Value.Covariance.ZZ),
            Z_VX  => Interfaces.C.double (Value.Covariance.Z_VX),
            Z_VY  => Interfaces.C.double (Value.Covariance.Z_VY),
            Z_VZ  => Interfaces.C.double (Value.Covariance.Z_VZ),
            VX_VX => Interfaces.C.double (Value.Covariance.VX_VX),
            VX_VY => Interfaces.C.double (Value.Covariance.VX_VY),
            VX_VZ => Interfaces.C.double (Value.Covariance.VX_VZ),
            VY_VY => Interfaces.C.double (Value.Covariance.VY_VY),
            VY_VZ => Interfaces.C.double (Value.Covariance.VY_VZ),
            VZ_VZ => Interfaces.C.double (Value.Covariance.VZ_VZ)),
         Maneuver_Probability        => Interfaces.C.double (Value.Maneuver_Probability),
         Track_Quality               => Interfaces.C.double (Value.Track_Quality));
      D                : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      Required         : aliased C.Size_T := 0;
   begin
      if Channel.Handle = C.Null_Track then
         raise Provider_Error with "IR Track channel is closed";
      end if;
      return Result : Update_Request do
         if C.IR_Track_Submit_Update
              (Channel.Handle,
               Raw'Access,
               Result.Owner.Handle'Access,
               D'Address,
               D'Length,
               Required'Access)
           /= C.Success
         then
            raise Provider_Error with Message (D);
         end if;
      end return;
   end Submit;

   --  Validates and copies a native CommandStatus into Ada-owned storage. The
   --  native adapter already rejected unknown provider values, so an
   --  out-of-range value here means the ABI contract was violated.
   function To_Command_Status (Raw : C.IR_Command_Status_V1) return Command_Status is
   begin
      if Raw.State > 4 then
         raise Provider_Error with "native IR Track update returned unknown CommandState";
      elsif Raw.Reason_ID > 46 then
         raise Provider_Error with "native IR Track update returned unknown CannotComply";
      end if;
      return
        (Status_Command_ID => Raw.Command_ID,
         Status_State      => Command_State'Enum_Val (Raw.State),
         Status_Reason     => Cannot_Comply'Enum_Val (Raw.Reason_ID),
         Status_Text       => Owned_Text (Raw.Reason_Description));
   end To_Command_Status;

   function Wait (Request : Update_Request; Timeout_Milliseconds : Natural) return Update_Result is
      Raw  : aliased C.IR_Track_Update_Result_V1 := ((0, 0, 0, (System.Null_Address, 0)), 0);
      D    : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      R    : aliased C.Size_T := 0;
      Code : constant Interfaces.Integer_32 :=
        C.IR_Track_Update_Request_Wait
          (Request.Owner.Handle,
           Interfaces.Unsigned_32 (Timeout_Milliseconds),
           Raw'Access,
           D'Address,
           D'Length,
           R'Access);
   begin
      if Code = C.Timeout then
         raise Timeout_Error with "IR Track update request timed out";
      elsif Code = C.Success then
         --  The reason description is copied immediately; no C pointer
         --  escapes this call.
         return
           (Result_Status  => Success,
            Result_Command => To_Command_Status (Raw.Status),
            Result_Code    => None,
            Result_Text    => US.Null_Unbounded_String);
      elsif Code = C.Command_Rejected then
         if Raw.Error_Code > 8 then
            raise Provider_Error with "native IR Track update returned unknown MEL error code";
         end if;
         declare
            Text : US.Unbounded_String := US.To_Unbounded_String (Message (D));
         begin
            --  The complete rejection text is recovered from the cached
            --  terminal request using exact storage; a changed status, code,
            --  or required size would mean the cache was not terminal.
            if R > D'Length then
               declare
                  Complete       : aliased Diagnostic (0 .. R - 1) := [others => Interfaces.C.nul];
                  Retry_Raw      : aliased C.IR_Track_Update_Result_V1 :=
                    ((0, 0, 0, (System.Null_Address, 0)), 0);
                  Retry_Required : aliased C.Size_T := 0;
                  Retry_Code     : constant Interfaces.Integer_32 :=
                    C.IR_Track_Update_Request_Wait
                      (Request.Owner.Handle,
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
                       with "native IR Track update rejection changed during diagnostic retry";
                  end if;
                  Text := US.To_Unbounded_String (Message (Complete));
               end;
            end if;
            return
              (Result_Status  => Rejected,
               Result_Command => <>,
               Result_Code    => Update_Error_Code'Val (Raw.Error_Code),
               Result_Text    => Text);
         end;
      else
         raise Provider_Error with Message (D);
      end if;
   end Wait;

   procedure Close (Request : in out Update_Request) is
      Ignored : Interfaces.Integer_32;
   begin
      Ignored :=
        C.IR_Track_Update_Request_Close (Request.Owner.Handle'Access, System.Null_Address, 0, null);
   end Close;

   overriding
   procedure Finalize (Request : in out Update_Request_Owner) is
      Ignored : Interfaces.Integer_32;
   begin
      --  Finalization releases the public owner only; it is not cancellation.
      Ignored :=
        C.IR_Track_Update_Request_Close (Request.Handle'Access, System.Null_Address, 0, null);
   exception
      when others =>
         Request.Handle := C.Null_Track_Update_Request;
   end Finalize;
end AMS.MEL.IR.Track.Updates;
