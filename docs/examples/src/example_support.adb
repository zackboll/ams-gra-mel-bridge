with Ada.Command_Line;
with Ada.Text_IO;

package body Example_Support is
   function Provider_Path return String is
   begin
      if Ada.Command_Line.Argument_Count /= 2 then
         raise Constraint_Error with "usage: program PROVIDER_LIBRARY MOCK_CONFIGURATION";
      end if;
      return Ada.Command_Line.Argument (1);
   end Provider_Path;

   function Configuration return String
   is (Ada.Command_Line.Argument (2));

   procedure Require (Condition : Boolean; Message : String) is
   begin
      if not Condition then
         raise Program_Error with Message;
      end if;
   end Require;

   procedure Report_Failure (Error : Ada.Exceptions.Exception_Occurrence) is
   begin
      Ada.Text_IO.Put_Line
        (Ada.Text_IO.Standard_Error,
         Ada.Exceptions.Exception_Name (Error) & ": " & Ada.Exceptions.Exception_Message (Error));
      Ada.Command_Line.Set_Exit_Status (Ada.Command_Line.Failure);
   end Report_Failure;
end Example_Support;
