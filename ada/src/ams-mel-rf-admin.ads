private with Ada.Finalization;
private with AMS.MEL_C_API;
with AMS.MEL.Status;

package AMS.MEL.RF.Admin is
   type Admin_MEL is limited private;
   function Open (Library_Path : String; Configuration : String) return Admin_MEL;
   function Is_Open (Object : Admin_MEL) return Boolean;
   function Command_State (Object : Admin_MEL; State : AMS.MEL.Status.MFA_State) return Boolean;
   procedure Close (Object : in out Admin_MEL);
private
   type Admin_MEL is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.RF_Admin_Handle := AMS.MEL_C_API.Null_RF_Admin;
   end record;
   overriding
   procedure Finalize (Object : in out Admin_MEL);
end AMS.MEL.RF.Admin;
