with AMS.MEL_C_API;
with AMS.MEL.IR.Calibration_Configuration_Conversion;
with System;

package body AMS.MEL.IR.Calibration_Configuration.Testing is
   package C renames AMS.MEL_C_API;
   procedure Check_Malformed is
      Raw_Entry : aliased C.IR_Subsystem_Calibration_V1 :=
        (Calibration_ID          =>
           (UUID => [others => 0], Descriptive_Label => (System.Null_Address, 0)),
         Accepted_Interface      => 0,
         Calibration_Item_Names  => (System.Null_Address, 0),
         Subsystem_Component_IDs => (System.Null_Address, 0),
         Capability_IDs          => (System.Null_Address, 0));
      procedure Reject (Value : C.IR_Calibration_Configuration_V1) is
      begin
         declare
            Copied : constant Calibration_Collection :=
              Calibration_Configuration_Conversion.To_Collection (Value);
         begin
            raise Program_Error with "malformed native span accepted " & Length (Copied)'Image;
         end;
      exception
         when Provider_Error =>
            null;
      end Reject;
   begin
      Reject ((System.Null_Address, 1));
      Reject ((Raw_Entry'Address, C.Size_T'Last));
      Raw_Entry.Accepted_Interface := 3;
      Reject ((Raw_Entry'Address, 1));
      Raw_Entry.Accepted_Interface := 16#FFFF_FFFF#;
      Reject ((Raw_Entry'Address, 1));
      Raw_Entry.Accepted_Interface := 0;
      Raw_Entry.Calibration_Item_Names := (System.Null_Address, 1);
      Reject ((Raw_Entry'Address, 1));
      Raw_Entry.Calibration_Item_Names := (System.Null_Address, 0);
      Raw_Entry.Subsystem_Component_IDs := (System.Null_Address, 1);
      Reject ((Raw_Entry'Address, 1));
      Raw_Entry.Subsystem_Component_IDs := (System.Null_Address, 0);
      Raw_Entry.Capability_IDs := (System.Null_Address, 1);
      Reject ((Raw_Entry'Address, 1));
      Raw_Entry.Capability_IDs := (System.Null_Address, 0);
      Raw_Entry.Calibration_ID.Descriptive_Label := (System.Null_Address, 1);
      Reject ((Raw_Entry'Address, 1));
   end Check_Malformed;
end AMS.MEL.IR.Calibration_Configuration.Testing;
