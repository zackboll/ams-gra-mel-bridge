with Interfaces;
with Interfaces.C;
with Interfaces.C.Strings;
with System;

package body AMS.MEL.RF.C2 is
   package C renames AMS.MEL_C_API;
   package CS renames Interfaces.C.Strings;
   use type Interfaces.Integer_32;
   use type C.RF_C2_Handle;
   use type Interfaces.C.char;

   type Diagnostic is array (C.Size_T range <>) of aliased Interfaces.C.char with Convention => C;
   subtype Fixed_Diagnostic is Diagnostic (0 .. 511);
   type String_Owner is new Ada.Finalization.Limited_Controlled with record
      Value : CS.chars_ptr := CS.Null_Ptr;
   end record;
   overriding
   procedure Finalize (Value : in out String_Owner) is
   begin
      CS.Free (Value.Value);
   end Finalize;

   function Message (Buffer : Diagnostic) return String is
      Last : Natural := 0;
   begin
      while Last < Buffer'Length and then Buffer (C.Size_T (Last)) /= Interfaces.C.nul loop
         Last := Last + 1;
      end loop;
      declare
         Text : String (1 .. Last);
      begin
         for I in Text'Range loop
            Text (I) := Character'Val (Interfaces.C.char'Pos (Buffer (C.Size_T (I - 1))));
         end loop;
         return (if Last = 0 then "native RF C2 operation failed" else Text);
      end;
   end Message;

   procedure Check (Code : Interfaces.Integer_32; Buffer : Diagnostic) is
   begin
      if Code /= C.Success then
         raise Provider_Error with Message (Buffer);
      end if;
   end Check;

   function Open (Library_Path : String; Configuration : String) return C2_MEL is
      Library_C, Configuration_C : String_Owner;
      D                          : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      R                          : aliased C.Size_T := 0;
   begin
      for Item of Library_Path loop
         if Item = Character'Val (0) then
            raise Constraint_Error with "Library_Path contains an embedded NUL";
         end if;
      end loop;
      for Item of Configuration loop
         if Item = Character'Val (0) then
            raise Constraint_Error with "Configuration contains an embedded NUL";
         end if;
      end loop;
      Library_C.Value := CS.New_String (Library_Path);
      Configuration_C.Value := CS.New_String (Configuration);
      return Result : C2_MEL do
         Check
           (C.RF_C2_Open
              (Library_C.Value,
               Configuration_C.Value,
               Result.Handle'Access,
               D'Address,
               D'Length,
               R'Access),
            D);
      end return;
   end Open;

   function Is_Open (Object : C2_MEL) return Boolean
   is (Object.Handle /= C.Null_RF_C2);

   procedure Close (Object : in out C2_MEL) is
      D : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      R : aliased C.Size_T := 0;
   begin
      Check (C.RF_C2_Close (Object.Handle'Access, D'Address, D'Length, R'Access), D);
   end Close;

   overriding
   procedure Finalize (Object : in out C2_MEL) is
      Ignored : Interfaces.Integer_32;
   begin
      Ignored := C.RF_C2_Close (Object.Handle'Access, System.Null_Address, 0, null);
   exception
      when others =>
         Object.Handle := C.Null_RF_C2;
   end Finalize;
end AMS.MEL.RF.C2;
