with Interfaces;
with System;

package body AMS.MEL.IR.Buffer is
   package C renames AMS.MEL_C_API;
   use type Interfaces.Integer_32;
   use type C.Buffer_Handle;
   use type C.Session_Handle;
   use type C.Size_T;
   use type Interfaces.Unsigned_32;

   procedure Check (Code : Interfaces.Integer_32; Result : Interfaces.Unsigned_32 := 16#FFFF_FFFF#)
   is
   begin
      if Code /= C.Success then
         raise Provider_Error
           with "Buffer bridge status" & Code'Image & "; provider Return" & Result'Image;
      end if;
   end Check;

   procedure Require_Open (Object : Host_Buffer) is
   begin
      if not Is_Open (Object) then
         raise Provider_Error with "Buffer is closed";
      end if;
   end Require_Open;

   function Open
     (Parent : Session; Size_Bytes : Positive; User_Context : Long_Long_Integer) return Host_Buffer
   is
      Result : aliased Interfaces.Unsigned_32 := 16#FFFF_FFFF#;
      Code   : Interfaces.Integer_32;
   begin
      if Parent.Handle = C.Null_Session then
         raise Provider_Error with "Session is closed";
      end if;
      return Object : Host_Buffer do
         Code :=
           C.IR_Buffer_Open
             (Parent.Handle,
              Interfaces.Unsigned_64 (Size_Bytes),
              Interfaces.Integer_64 (User_Context),
              Object.Handle'Access,
              Result'Access,
              System.Null_Address,
              0,
              null);
         Check (Code, Result);
      end return;
   end Open;

   function Is_Open (Object : Host_Buffer) return Boolean
   is (Object.Handle /= C.Null_Buffer);

   function Properties (Object : Host_Buffer) return Buffer_Properties is
      Raw : aliased C.IR_Buffer_Info_V1;
   begin
      Require_Open (Object);
      Check (C.IR_Buffer_Get_Info (Object.Handle, Raw'Access, System.Null_Address, 0, null));
      return
        (Natural (Raw.Storage_Size),
         Long_Long_Integer (Raw.Initialized_Context),
         Long_Long_Integer (Raw.Provider_Size),
         Long_Long_Integer (Raw.Provider_Context),
         Raw.Buffer_Address_Matches = 1,
         Image_Address_Kind'Val (Raw.Image_Location));
   end Properties;

   function Size_Bytes (Object : Host_Buffer) return Natural
   is (Properties (Object).Capacity_Bytes);

   function User_Context (Object : Host_Buffer) return Long_Long_Integer
   is (Properties (Object).Provider_Context);

   type Raw_Flags is array (C.Size_T range <>) of Interfaces.Unsigned_32 with Convention => C;

   function Flags (Object : Host_Buffer) return Flag_Array is
      Count  : aliased C.Size_T := 0;
      Result : aliased Interfaces.Unsigned_32 := 16#FFFF_FFFF#;
      Code   : Interfaces.Integer_32;
   begin
      Require_Open (Object);
      Code :=
        C.IR_Buffer_Get_Flags
          (Object.Handle,
           System.Null_Address,
           0,
           Count'Access,
           Result'Access,
           System.Null_Address,
           0,
           null);
      if Code /= C.Buffer_Too_Small then
         Check (Code, Result);
      end if;
      if Count > C.Size_T (Natural'Last) then
         raise Provider_Error with "Buffer flag count exceeds Ada bounds";
      end if;
      declare
         Capacity : constant C.Size_T := Count;
         Raw      : aliased Raw_Flags (1 .. Capacity);
      begin
         Code :=
           C.IR_Buffer_Get_Flags
             (Object.Handle,
              Raw'Address,
              Capacity,
              Count'Access,
              Result'Access,
              System.Null_Address,
              0,
              null);
         Check (Code, Result);
         return Values : Flag_Array (1 .. Natural (Count)) do
            for I in Values'Range loop
               Values (I) := Buffer_Flag'Val (Raw (C.Size_T (I)));
            end loop;
         end return;
      end;
   end Flags;

   procedure Set_Flags (Object : in out Host_Buffer; Values : Flag_Array) is
      Raw : aliased Raw_Flags (1 .. C.Size_T (Values'Length));
   begin
      Require_Open (Object);
      for I in Values'Range loop
         Raw (C.Size_T (I - Values'First + 1)) := Buffer_Flag'Pos (Values (I));
      end loop;
      Check
        (C.IR_Buffer_Set_Flags
           (Object.Handle, Raw'Address, Raw'Length, System.Null_Address, 0, null));
   end Set_Flags;

   procedure Add_Flag (Object : in out Host_Buffer; Flag : Buffer_Flag) is
   begin
      Require_Open (Object);
      Check
        (C.IR_Buffer_Add_Flag
           (Object.Handle, Buffer_Flag'Pos (Flag), System.Null_Address, 0, null));
   end Add_Flag;

   procedure Check_Span (Object : Host_Buffer; Offset, Count : Natural) is
      Capacity : constant Natural := Size_Bytes (Object);
   begin
      if Offset > Capacity or else Count > Capacity - Offset then
         raise Constraint_Error with "Buffer storage span exceeds capacity";
      end if;
   end Check_Span;

   function Read_Storage
     (Object : Host_Buffer; Offset : Natural; Count : Natural) return Pixel_Array is
   begin
      Check_Span (Object, Offset, Count);
      return Bytes : Pixel_Array (1 .. Count) do
         Check
           (C.IR_Buffer_Read_Storage
              (Object.Handle,
               Interfaces.Unsigned_64 (Offset),
               Bytes'Address,
               C.Size_T (Count),
               System.Null_Address,
               0,
               null));
      end return;
   end Read_Storage;

   procedure Write_Storage (Object : in out Host_Buffer; Offset : Natural; Bytes : Pixel_Array) is
   begin
      Check_Span (Object, Offset, Bytes'Length);
      Check
        (C.IR_Buffer_Write_Storage
           (Object.Handle,
            Interfaces.Unsigned_64 (Offset),
            Bytes'Address,
            C.Size_T (Bytes'Length),
            System.Null_Address,
            0,
            null));
   end Write_Storage;

   procedure Close (Object : in out Host_Buffer) is
   begin
      Check (C.IR_Buffer_Close (Object.Handle'Access, System.Null_Address, 0, null));
   end Close;

   overriding
   procedure Finalize (Object : in out Host_Buffer) is
      Ignored : Interfaces.Integer_32;
   begin
      Ignored := C.IR_Buffer_Close (Object.Handle'Access, System.Null_Address, 0, null);
   --  Failure leaves the allocated native owner and entire graph intact.
   exception
      when others =>
         null;
   end Finalize;
end AMS.MEL.IR.Buffer;
