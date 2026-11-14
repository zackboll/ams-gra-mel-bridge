with Ada.Unchecked_Conversion;
with AMS.MEL.IR.Capability_Conversion;
with Interfaces.C;
with System;

package body AMS.MEL.IR.Discovery is
   package C renames AMS.MEL_C_API;
   use type Interfaces.Integer_32;
   use type Interfaces.C.size_t;

   type Cap_Access is access all C.IR_Channel_Capability_V1;
   function To_Cap is new Ada.Unchecked_Conversion (System.Address, Cap_Access);

   function Capabilities (Session : AMS.MEL.Session) return Capability_Vectors.Vector is
      List     : aliased C.Capability_List_Handle := C.Null_Capability_List;
      Item     : aliased C.Capability_Handle := C.Null_Capability;
      Count    : aliased C.Size_T := 0;
      View     : aliased System.Address := System.Null_Address;
      D        : aliased Interfaces.C.char_array (0 .. 511) := [others => Interfaces.C.nul];
      Required : aliased C.Size_T := 0;
      Result   : Capability_Vectors.Vector;

      procedure Check (Code : Interfaces.Integer_32) is
      begin
         if Code /= C.Success then
            raise Provider_Error with Interfaces.C.To_Ada (D);
         end if;
      end Check;

      procedure Cleanup is
         Ignored : Interfaces.Integer_32;
      begin
         --  Native snapshots protect their borrowed views only during copying.
         --  Neither a partially converted vector nor an Ada allocation failure
         --  may strand an entry or list owner. Close is non-allocating here.
         Ignored := C.IR_Capability_Close (Item'Access, System.Null_Address, 0, null);
         Ignored := C.IR_Capability_List_Close (List'Access, System.Null_Address, 0, null);
      end Cleanup;
   begin
      Check
        (C.IR_Capability_List_Create
           (Session.Handle, List'Access, D'Address, D'Length, Required'Access));
      Check (C.IR_Capability_List_Count (List, Count'Access, D'Address, D'Length, Required'Access));
      if Count > 0 then
         for I in 0 .. Count - 1 loop
            Check
              (C.IR_Capability_List_Get
                 (List, I, Item'Access, D'Address, D'Length, Required'Access));
            Check (C.IR_Capability_View (Item, View'Access, D'Address, D'Length, Required'Access));
            Result.Append (Capability_Conversion.To_Channel_Capability (To_Cap (View).all));
            Check (C.IR_Capability_Close (Item'Access, D'Address, D'Length, Required'Access));
         end loop;
      end if;
      Check (C.IR_Capability_List_Close (List'Access, D'Address, D'Length, Required'Access));
      return Result;
   exception
      when others =>
         Cleanup;
         raise;
   end Capabilities;
end AMS.MEL.IR.Discovery;
