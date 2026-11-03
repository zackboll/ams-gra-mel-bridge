with AMS.MEL_C_API;

package body AMS.MEL.IR.Erase.Testing is
   procedure Check_Representation is
      package C renames AMS.MEL_C_API;
      Command    : constant C.IR_Erase_Command_V1 := (0, 0);
      Result     : constant C.IR_Erase_Result_V1 := (0, 0);
      Unselected : constant Erase_Command := (others => <>);
   begin
      if C.IR_Erase_Command_V1'Object_Size /= 64
        or else C.IR_Erase_Result_V1'Object_Size /= 64
        or else Command.Erase_Type'Position /= 4
        or else Result.Error_Code'Position /= 4
        or else Erase_Type'Pos (Zeroize) /= 0
        or else Erase_Type'Pos (Classified_Data_Erase) /= 1
        or else Erase_Type'Pos (Disable) /= 2
        or else Erase_Type'Pos (CDE_AND_Zeroize) /= 3
      then
         raise Program_Error with "EraseCommand C/Ada layout mismatch";
      end if;
      begin
         declare
            Ignored : constant Command_ID := ID (Unselected);
         begin
            raise Program_Error with "unselected erase command has ID";
         end;
      exception
         when Provider_Error =>
            null;
      end;
      begin
         declare
            Ignored : constant Erase_Type := Operation (Unselected);
         begin
            raise Program_Error with "unselected erase command has operation";
         end;
      exception
         when Provider_Error =>
            null;
      end;
   end Check_Representation;
end AMS.MEL.IR.Erase.Testing;
