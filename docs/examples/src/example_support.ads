with Ada.Exceptions;

package Example_Support is
   function Provider_Path return String;
   function Configuration return String;
   procedure Require (Condition : Boolean; Message : String);
   procedure Report_Failure (Error : Ada.Exceptions.Exception_Occurrence);
end Example_Support;
