with Interfaces.C;
with AMS.MEL_C_API;
with System;
with AMS.MEL.IR.Calibration_Status_Conversion;

package body AMS.MEL.IR.Calibration_Status is
   use type Interfaces.Integer_32;
   use type Interfaces.Unsigned_32;
   use type Interfaces.C.char;
   use type Interfaces.C.size_t;
   use type AMS.MEL_C_API.Calibration_Status_Request_Handle;
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
      return
        (if Value'Length = 0 then "native IR CalibrationStatusCmd operation failed" else Value);
   end Failure;

   function Item_Name (Item : Completed_Item) return String
   is (US.To_String (Item.Name));
   function Result (Item : Completed_Item) return Calibration_Result
   is (Item.Item_Result);
   function Fail_Reason (Item : Completed_Item) return String
   is (US.To_String (Item.Reason));
   function Calibration_ID (Calibration : Completed_Calibration) return UCI_ID
   is (Calibration.ID);
   function Time_Tag (Calibration : Completed_Calibration) return Nanoseconds
   is (Calibration.Time);
   function Result (Calibration : Completed_Calibration) return Calibration_Result
   is (Calibration.Overall);
   function Fail_Reason (Calibration : Completed_Calibration) return String
   is (US.To_String (Calibration.Reason));
   function Item_Count (Calibration : Completed_Calibration) return Natural
   is (Natural (Calibration.Items.Length));
   function Item_At (Calibration : Completed_Calibration; Index : Positive) return Completed_Item
   is (Calibration.Items.Element (Index));
   function Active_Count (Value : Status_Value) return Natural
   is (Natural (Value.Active.Length));
   function Active_ID (Value : Status_Value; Index : Positive) return UCI_ID
   is (Value.Active.Element (Index));
   function Completed_Count (Value : Status_Value) return Natural
   is (Natural (Value.Completions.Length));
   function Completed_At (Value : Status_Value; Index : Positive) return Completed_Calibration
   is (Value.Completions.Element (Index));
   function Create_Item
     (Item_Name : String; Result : Calibration_Result; Fail_Reason : String) return Completed_Item
   is ((Name        => US.To_Unbounded_String (Item_Name),
        Item_Result => Result,
        Reason      => US.To_Unbounded_String (Fail_Reason)));
   function Create_Calibration
     (Calibration_ID : UCI_ID;
      Time_Tag       : Nanoseconds;
      Result         : Calibration_Result;
      Fail_Reason    : String) return Completed_Calibration
   is ((ID      => Calibration_ID,
        Time    => Time_Tag,
        Overall => Result,
        Reason  => US.To_Unbounded_String (Fail_Reason),
        Items   => Item_Vectors.Empty_Vector));
   procedure Append_Item (Calibration : in out Completed_Calibration; Item : Completed_Item) is
   begin
      Calibration.Items.Append (Item);
   end Append_Item;
   procedure Append_Active (Value : in out Status_Value; ID : UCI_ID) is
   begin
      Value.Active.Append (ID);
   end Append_Active;
   procedure Append_Completed (Value : in out Status_Value; Calibration : Completed_Calibration) is
   begin
      Value.Completions.Append (Calibration);
   end Append_Completed;

   function Status (Result : Status_Result) return Outcome
   is (Result.Result_Status);
   function Value (Result : Status_Result) return Status_Value
   is (Result.Result_Value);
   function Rejection_Code (Result : Status_Result) return Error_Code
   is (Result.Result_Code);
   function Description (Result : Status_Result) return String
   is (US.To_String (Result.Result_Text));

   function Is_Open (Request : Status_Request) return Boolean
   is (Calibration_Status_Request_Owner (Request).Handle /= C.Null_Calibration_Status_Request);

   function Wait (Request : Status_Request; Timeout_Milliseconds : Natural) return Status_Result is
      Empty    : constant C.IR_Calibration_Status_Result_V1 :=
        (Status     =>
           (Active_Calibration_IDs => (Data => System.Null_Address, Size => 0),
            Completed_Calibrations => (Data => System.Null_Address, Size => 0)),
         Error_Code => 0);
      Raw      : aliased C.IR_Calibration_Status_Result_V1 := Empty;
      D        : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      Required : aliased C.Size_T := 0;
      Code     : Interfaces.Integer_32;
   begin
      if Calibration_Status_Request_Owner (Request).Handle = C.Null_Calibration_Status_Request then
         raise Provider_Error with "IR CalibrationStatusCmd request is closed";
      end if;
      Code :=
        C.IR_Calibration_Status_Request_Wait
          (Calibration_Status_Request_Owner (Request).Handle,
           Interfaces.Unsigned_32 (Timeout_Milliseconds),
           Raw'Access,
           D'Address,
           D'Length,
           Required'Access);
      if Code = C.Timeout then
         raise Timeout_Error with "IR CalibrationStatusCmd request timed out";
      elsif Code = C.Success then
         return
           (Result_Status => AMS.MEL.IR.Channel.Success,
            Result_Value  => Calibration_Status_Conversion.To_Value (Raw.Status),
            Result_Code   => AMS.MEL.IR.Channel.None,
            Result_Text   => US.Null_Unbounded_String);
      elsif Code = C.Command_Rejected then
         if Raw.Error_Code > 8 then
            raise Provider_Error
              with "native IR CalibrationStatusCmd returned unknown MEL error code";
         end if;
         declare
            Reason : US.Unbounded_String := US.To_Unbounded_String (Message (D));
         begin
            if Required > D'Length then
               declare
                  Complete       : aliased Diagnostic_Array (0 .. Required - 1) :=
                    [others => Interfaces.C.nul];
                  Retry_Raw      : aliased C.IR_Calibration_Status_Result_V1 := Empty;
                  Retry_Required : aliased C.Size_T := 0;
                  Retry_Code     : constant Interfaces.Integer_32 :=
                    C.IR_Calibration_Status_Request_Wait
                      (Calibration_Status_Request_Owner (Request).Handle,
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
                         "native IR CalibrationStatusCmd rejection changed during diagnostic retry";
                  end if;
                  Reason := US.To_Unbounded_String (Message (Complete));
               end;
            end if;
            return
              (Result_Status => AMS.MEL.IR.Channel.Rejected,
               Result_Value  => (others => <>),
               Result_Code   => Error_Code'Val (Raw.Error_Code),
               Result_Text   => Reason);
         end;
      else
         raise Provider_Error with Failure (D);
      end if;
   end Wait;

   procedure Close (Request : in out Status_Request) is
      D    : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      R    : aliased C.Size_T := 0;
      Code : constant Interfaces.Integer_32 :=
        C.IR_Calibration_Status_Request_Close
          (Calibration_Status_Request_Owner (Request).Handle'Access, D'Address, D'Length, R'Access);
   begin
      if Code /= C.Success then
         raise Provider_Error with Failure (D);
      end if;
   end Close;
end AMS.MEL.IR.Calibration_Status;
