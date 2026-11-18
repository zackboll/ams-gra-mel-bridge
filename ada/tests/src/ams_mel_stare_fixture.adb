with Ada.Unchecked_Conversion;
with Interfaces;
with Interfaces.C;
with System;

package body AMS_MEL_Stare_Fixture is
   package IC renames Interfaces.C;
   use type IC.int;
   use type System.Address;
   function Dlopen (Path : IC.char_array; Flags : IC.int) return System.Address
   with Import, Convention => C, External_Name => "dlopen";
   function Dlsym (Handle : System.Address; Name : IC.char_array) return System.Address
   with Import, Convention => C, External_Name => "dlsym";
   function Dlclose (Handle : System.Address) return IC.int
   with Import, Convention => C, External_Name => "dlclose";
   type Release_Function is access function (Family, Amount : IC.unsigned) return IC.int
   with Convention => C;
   function To_Release is new Ada.Unchecked_Conversion (System.Address, Release_Function);
   type Notify_Function is
     access function
       (Family, Operation : IC.unsigned;
        Target            : Interfaces.Unsigned_64;
        Observed          : access Interfaces.Unsigned_64) return IC.int
   with Convention => C;
   function To_Notify is new Ada.Unchecked_Conversion (System.Address, Notify_Function);
   Pin : System.Address := System.Null_Address;

   procedure Open (Provider_Path : String) is
   begin
      Pin := Dlopen (IC.To_C (Provider_Path), 2);
      if Pin = System.Null_Address then
         raise Program_Error with "StareTask fixture pin failed";
      end if;
   end Open;

   procedure Release_Pending (Count : Positive) is
      Release : constant Release_Function :=
        To_Release (Dlsym (Pin, IC.To_C ("mock_completion_release_family")));
   begin
      if Release = null or else Release (16, IC.unsigned (Count)) /= 1 then
         raise Program_Error with "StareTask fixture release failed";
      end if;
   end Release_Pending;

   procedure Drain is
      Notify   : constant Notify_Function :=
        To_Notify (Dlsym (System.Null_Address, IC.To_C ("ams_mel_test_completion_owner_notify")));
      Observed : aliased Interfaces.Unsigned_64 := 0;
   begin
      if Notify = null or else Notify (1, 0, 0, Observed'Access) /= 1 then
         raise Program_Error with "StareTask completion notification did not retire";
      end if;
   end Drain;

   procedure Close is
   begin
      Drain;
      if Dlclose (Pin) /= 0 then
         raise Program_Error with "StareTask fixture unpin failed";
      end if;
      Pin := System.Null_Address;
   end Close;
end AMS_MEL_Stare_Fixture;
