with Interfaces.C;

package body AMS.MEL.RF.C2.Transmit_Power is
   package C renames AMS.MEL_C_API;
   use type Interfaces.Integer_32;
   use type Interfaces.C.char;
   pragma
     Compile_Time_Error
       (Long_Float'Digits < Interfaces.C.double'Digits
          or else Long_Float'Machine_Mantissa < Interfaces.C.double'Machine_Mantissa
          or else Long_Float'Machine_Emax < Interfaces.C.double'Machine_Emax
          or else Long_Float'Machine_Emin > Interfaces.C.double'Machine_Emin
          or else Long_Float'Machine_Radix /= Interfaces.C.double'Machine_Radix,
        "TX queries require Long_Float to represent normal C double values");
   type Diagnostic is array (C.Size_T range 0 .. 511) of aliased Interfaces.C.char
   with Convention => C;
   procedure Check (Code : Interfaces.Integer_32; Buffer : Diagnostic) is
      Last : Natural := 0;
   begin
      if Code = C.Success then
         return;
      end if;
      while Last < Buffer'Length and then Buffer (C.Size_T (Last)) /= Interfaces.C.nul loop
         Last := Last + 1;
      end loop;
      declare
         Text : String (1 .. Last);
      begin
         for I in Text'Range loop
            Text (I) := Character'Val (Interfaces.C.char'Pos (Buffer (C.Size_T (I - 1))));
         end loop;
         raise Provider_Error with (if Last = 0 then "native TX power query failed" else Text);
      end;
   end Check;
   function To_C (Value : Long_Float) return Interfaces.C.double is
      --  E3's narrowly scoped IEEE conversion pattern, not project-wide settings.
      pragma Validity_Checks ("F");
   begin
      return Interfaces.C.double (Value);
   end To_C;
   function From_C (Value : Interfaces.C.double) return Long_Float is
      pragma Validity_Checks ("F");
   begin
      return Long_Float (Value);
   end From_C;
   function Radiated_Power_DBW
     (Object              : Virtual_Aperture'Class;
      Element_Group       : Element_Group_ID;
      Power_Mode_ID       : Interfaces.Unsigned_32;
      Attenuation_DB      : Long_Float;
      Weight_Type         : Weight_Type_ID;
      Center_Frequency_Hz : Long_Float;
      Line_Of_Sight       : UV_Line_Of_Sight;
      VA_Instance_ID      : Interfaces.Unsigned_32 := 0) return Long_Float
   is
      pragma Validity_Checks ("F");
      D : aliased Diagnostic := [others => Interfaces.C.nul];
      V : aliased Interfaces.C.double := 0.0;
   begin
      Check
        (C.RF_VA_Get_TX_Radiated_Power
           (Object.Handle,
            Element_Group,
            Power_Mode_ID,
            To_C (Attenuation_DB),
            Weight_Type,
            To_C (Center_Frequency_Hz),
            To_C (Line_Of_Sight.U),
            To_C (Line_Of_Sight.V),
            VA_Instance_ID,
            V'Access,
            D'Address,
            D'Length,
            null),
         D);
      return From_C (V);
   end Radiated_Power_DBW;
   function Peak_Radiated_Power_DBW
     (Object              : Virtual_Aperture'Class;
      Element_Group       : Element_Group_ID;
      Power_Mode_ID       : Interfaces.Unsigned_32;
      Attenuation_DB      : Long_Float;
      Center_Frequency_Hz : Long_Float;
      VA_Instance_ID      : Interfaces.Unsigned_32 := 0) return Long_Float
   is
      pragma Validity_Checks ("F");
      D : aliased Diagnostic := [others => Interfaces.C.nul];
      V : aliased Interfaces.C.double := 0.0;
   begin
      Check
        (C.RF_VA_Get_TX_Peak_Radiated_Power
           (Object.Handle,
            Element_Group,
            Power_Mode_ID,
            To_C (Attenuation_DB),
            To_C (Center_Frequency_Hz),
            VA_Instance_ID,
            V'Access,
            D'Address,
            D'Length,
            null),
         D);
      return From_C (V);
   end Peak_Radiated_Power_DBW;
   function Aperture_Gain_DB
     (Object              : Virtual_Aperture'Class;
      Element_Group       : Element_Group_ID;
      Power_Mode_ID       : Interfaces.Unsigned_32;
      Weight_Type         : Weight_Type_ID;
      Center_Frequency_Hz : Long_Float;
      Line_Of_Sight       : UV_Line_Of_Sight;
      VA_Instance_ID      : Interfaces.Unsigned_32 := 0) return Long_Float
   is
      pragma Validity_Checks ("F");
      D : aliased Diagnostic := [others => Interfaces.C.nul];
      V : aliased Interfaces.C.double := 0.0;
   begin
      Check
        (C.RF_VA_Get_TX_Aperture_Gain
           (Object.Handle,
            Element_Group,
            Power_Mode_ID,
            Weight_Type,
            To_C (Center_Frequency_Hz),
            To_C (Line_Of_Sight.U),
            To_C (Line_Of_Sight.V),
            VA_Instance_ID,
            V'Access,
            D'Address,
            D'Length,
            null),
         D);
      return From_C (V);
   end Aperture_Gain_DB;
   function Max_Attenuation_DB
     (Object         : Virtual_Aperture'Class;
      Element_Group  : Element_Group_ID;
      Power_Mode_ID  : Interfaces.Unsigned_32;
      VA_Instance_ID : Interfaces.Unsigned_32 := 0) return Long_Float
   is
      pragma Validity_Checks ("F");
      D : aliased Diagnostic := [others => Interfaces.C.nul];
      V : aliased Interfaces.C.double := 0.0;
   begin
      Check
        (C.RF_VA_Get_Max_TX_Attenuation
           (Object.Handle,
            Element_Group,
            Power_Mode_ID,
            VA_Instance_ID,
            V'Access,
            D'Address,
            D'Length,
            null),
         D);
      return From_C (V);
   end Max_Attenuation_DB;
end AMS.MEL.RF.C2.Transmit_Power;
