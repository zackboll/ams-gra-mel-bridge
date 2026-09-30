private with Ada.Finalization;
private with AMS.MEL_C_API;

package AMS.MEL.RF.C2 is
   type C2_MEL is limited private;
   function Open (Library_Path : String; Configuration : String) return C2_MEL;
   function Is_Open (Object : C2_MEL) return Boolean;
   procedure Close (Object : in out C2_MEL);
private
   type C2_MEL is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.RF_C2_Handle := AMS.MEL_C_API.Null_RF_C2;
   end record;
   overriding
   procedure Finalize (Object : in out C2_MEL);
end AMS.MEL.RF.C2;
