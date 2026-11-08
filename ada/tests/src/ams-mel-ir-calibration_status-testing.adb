with AMS.MEL_C_API;
with AMS.MEL.IR.Calibration_Status_Conversion;
with Interfaces;
with System;

package body AMS.MEL.IR.Calibration_Status.Testing is
   package C renames AMS.MEL_C_API;
   use type C.Size_T;
   Empty_View : constant C.String_View_V1 := (System.Null_Address, 0);

   procedure Check_Representation is
      Item        : constant C.IR_Completed_Calibration_Item_V1 :=
        (Item_Name => Empty_View, Result => 0, Fail_Reason => Empty_View);
      Calibration : constant C.IR_Completed_Calibration_V1 :=
        (Calibration_ID    => (UUID => [others => 0], Descriptive_Label => Empty_View),
         Time_Tag_NS       => 0,
         Result            => 0,
         Fail_Reason       => Empty_View,
         Calibration_Items => (System.Null_Address, 0));
      Result      : constant C.IR_Calibration_Status_Result_V1 :=
        (Status     =>
           (Active_Calibration_IDs => (System.Null_Address, 0),
            Completed_Calibrations => (System.Null_Address, 0)),
         Error_Code => 0);
      Command     : constant C.IR_Calibration_Status_Command_V1 := (0, 0);
   begin
      --  Compiled C test_ir_calibration_status static assertions fix these sizes and offsets.
      if C.IR_Calibration_Status_Command_V1'Object_Size /= 64
        or else Command.Request_ID'Position /= 4
        or else C.IR_Completed_Calibration_Item_V1'Object_Size /= 320
        or else Item.Result'Position /= 16
        or else Item.Fail_Reason'Position /= 24
        or else C.IR_Completed_Calibration_V1'Object_Size /= 640
        or else Calibration.Time_Tag_NS'Position /= 32
        or else Calibration.Result'Position /= 40
        or else Calibration.Fail_Reason'Position /= 48
        or else Calibration.Calibration_Items'Position /= 64
        or else C.IR_Calibration_Status_V1'Object_Size /= 256
        or else Result.Status.Completed_Calibrations'Position /= 16
        or else Result.Error_Code'Position /= 32
        or else C.IR_Calibration_Status_Result_V1'Object_Size /= 320
        or else Calibration_Result'Pos (Not_Set) /= 0
        or else Calibration_Result'Pos (Completed) /= 1
        or else Calibration_Result'Pos (Failed) /= 2
        or else Calibration_Result'Pos (Interrupted) /= 3
        or else Calibration_Result'Pos (Not_Calibrated) /= 4
        or else Calibration_Result'Enum_Rep (Not_Calibrated) /= 4
        or else Nanoseconds'Size /= 64
        or else Nanoseconds'First /= Nanoseconds (Interfaces.Integer_64'First)
        or else Nanoseconds'Last /= Nanoseconds (Interfaces.Integer_64'Last)
      then
         raise Program_Error with "CalibrationStatus C/Ada layout mismatch";
      end if;
   end Check_Representation;

   procedure Check_Malformed is
      Raw_Item      : aliased C.IR_Completed_Calibration_Item_V1 :=
        (Item_Name => Empty_View, Result => 0, Fail_Reason => Empty_View);
      Raw_Completed : aliased C.IR_Completed_Calibration_V1 :=
        (Calibration_ID    => (UUID => [others => 0], Descriptive_Label => Empty_View),
         Time_Tag_NS       => Interfaces.Integer_64'First,
         Result            => 0,
         Fail_Reason       => Empty_View,
         Calibration_Items => (System.Null_Address, 0));
      Raw_ID        : aliased C.UCI_ID_V1 :=
        (UUID => [others => 7], Descriptive_Label => Empty_View);
      NUL           : aliased constant String := "a" & Character'Val (0) & "b";
      procedure Reject (Value : C.IR_Calibration_Status_V1; What : String) is
      begin
         declare
            Copied : constant Status_Value := Calibration_Status_Conversion.To_Value (Value);
         begin
            raise Program_Error
              with "malformed native " & What & " accepted" & Active_Count (Copied)'Image;
         end;
      exception
         when Provider_Error =>
            null;
      end Reject;
      function Completed_View return C.IR_Calibration_Status_V1
      is ((Active_Calibration_IDs => (System.Null_Address, 0),
           Completed_Calibrations => (Raw_Completed'Address, 1)));
   begin
      --  Positive control: a well-formed nested view converts completely.
      Raw_Completed.Calibration_Items := (Raw_Item'Address, 1);
      declare
         Value : constant Status_Value :=
           Calibration_Status_Conversion.To_Value
             ((Active_Calibration_IDs => (Raw_ID'Address, 1),
               Completed_Calibrations => (Raw_Completed'Address, 1)));
      begin
         if Active_Count (Value) /= 1
           or else Completed_Count (Value) /= 1
           or else Item_Count (Completed_At (Value, 1)) /= 1
           or else Time_Tag (Completed_At (Value, 1)) /= Nanoseconds'First
         then
            raise Program_Error with "well-formed native CalibrationStatus view rejected";
         end if;
      end;
      Reject (((System.Null_Address, 1), (System.Null_Address, 0)), "null active span");
      Reject (((System.Null_Address, 0), (System.Null_Address, 1)), "null completed span");
      Reject (((Raw_ID'Address, C.Size_T'Last), (System.Null_Address, 0)), "active overflow");
      Reject
        (((System.Null_Address, 0), (Raw_Completed'Address, C.Size_T'Last)), "completed overflow");
      Reject
        (((Raw_ID'Address, C.Size_T (Interfaces.Integer_32'Last) + 1), (System.Null_Address, 0)),
         "count above INT32_MAX");
      Raw_Completed.Result := 5;
      Reject (Completed_View, "MaxExclusive overall result");
      Raw_Completed.Result := 16#FFFF_FFFF#;
      Reject (Completed_View, "unknown overall result");
      Raw_Completed.Result := 4;
      Raw_Item.Result := 5;
      Reject (Completed_View, "MaxExclusive item result");
      Raw_Item.Result := 99;
      Reject (Completed_View, "unknown item result");
      Raw_Item.Result := 0;
      Raw_Completed.Calibration_Items := (System.Null_Address, 1);
      Reject (Completed_View, "null item span");
      Raw_Completed.Calibration_Items := (Raw_Item'Address, C.Size_T'Last);
      Reject (Completed_View, "item span overflow");
      Raw_Completed.Calibration_Items := (Raw_Item'Address, 1);
      Raw_Item.Item_Name := (System.Null_Address, 1);
      Reject (Completed_View, "null item name");
      Raw_Item.Item_Name := (NUL'Address, 3);
      Reject (Completed_View, "NUL item name");
      Raw_Item.Item_Name := Empty_View;
      Raw_Item.Fail_Reason := (NUL'Address, 3);
      Reject (Completed_View, "NUL item failure description");
      Raw_Item.Fail_Reason := Empty_View;
      Raw_Completed.Fail_Reason := (System.Null_Address, 2);
      Reject (Completed_View, "null overall failure description");
      Raw_Completed.Fail_Reason := Empty_View;
      Raw_Completed.Calibration_ID.Descriptive_Label := (NUL'Address, 3);
      Reject (Completed_View, "NUL completed label");
      Raw_Completed.Calibration_ID.Descriptive_Label := Empty_View;
      Raw_ID.Descriptive_Label := (System.Null_Address, 1);
      Reject (((Raw_ID'Address, 1), (System.Null_Address, 0)), "null active label");
   end Check_Malformed;
end AMS.MEL.IR.Calibration_Status.Testing;
