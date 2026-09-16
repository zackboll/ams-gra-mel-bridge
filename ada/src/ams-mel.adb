with Interfaces;
with Interfaces.C;
with Interfaces.C.Strings;
with System;

package body AMS.MEL is
   use type Interfaces.Integer_32;
   use type Interfaces.C.char;
   use type Interfaces.C.size_t;
   use type AMS.MEL_C_API.Session_Handle;
   package C renames AMS.MEL_C_API;
   package CS renames Interfaces.C.Strings;
   package US renames Ada.Strings.Unbounded;

   Diagnostic_Capacity : constant := 512;
   subtype Diagnostic_Index is Interfaces.C.size_t range
     0 .. Diagnostic_Capacity - 1;
   type Diagnostic_Array is array (Diagnostic_Index) of aliased Interfaces.C.char
     with Convention => C;

   function Message (Buffer : Diagnostic_Array) return String is
      Length : Natural := 0;
   begin
      while Length < Buffer'Length
        and then Buffer (Interfaces.C.size_t (Length)) /= Interfaces.C.nul
      loop
         Length := Length + 1;
      end loop;
      if Length = 0 then
         return "native provider operation failed";
      end if;
      declare
         Result : String (1 .. Length);
      begin
         for Index in Result'Range loop
            Result (Index) := Character'Val
              (Interfaces.C.char'Pos
                 (Buffer (Interfaces.C.size_t (Index - 1))));
         end loop;
         return Result;
      end;
   end Message;

   function ABI_Version return Version is
      Raw    : aliased C.Version_V1 := (Major => 0, Minor => 0);
      Status : constant Interfaces.Integer_32 := C.Get_ABI_Version (Raw'Access);
   begin
      if Status /= C.Success then
         raise Program_Error with "AMS MEL native version query failed";
      end if;
      if Interfaces.Unsigned_32'Pos (Raw.Major) > Natural'Last
        or else Interfaces.Unsigned_32'Pos (Raw.Minor) > Natural'Last
      then
         raise Program_Error with "AMS MEL version is not representable";
      end if;
      return (Major => Natural (Raw.Major), Minor => Natural (Raw.Minor));
   end ABI_Version;

   function API_Version (Value : Provider_Version)
     return Provider_Version_Number is (Value.API_Value);

   function Library_Version (Value : Provider_Version)
     return Provider_Version_Number is (Value.Library_Value);

   function Vendor (Value : Provider_Version) return String is
     (US.To_String (Value.Vendor_Value));

   function Description (Value : Provider_Version) return String is
     (US.To_String (Value.Description_Value));

   function Open
     (Library_Path       : String;
      Instance           : String;
      Aperture_Config_ID : String := "") return Session
   is
      Library_C  : CS.chars_ptr := CS.New_String (Library_Path);
      Instance_C : CS.chars_ptr := CS.New_String (Instance);
      Aperture_C : CS.chars_ptr := CS.New_String (Aperture_Config_ID);
      Diagnostic : aliased Diagnostic_Array := (others => Interfaces.C.nul);
      Required   : aliased C.Size_T := 0;
   begin
      return Result : Session do
         declare
            Status : constant Interfaces.Integer_32 := C.Session_Open
              (Library_C, Instance_C, Aperture_C, Result.Handle'Access,
               Diagnostic'Address, Diagnostic'Length, Required'Access);
         begin
            CS.Free (Library_C);
            CS.Free (Instance_C);
            CS.Free (Aperture_C);
            if Status /= C.Success then
               raise Provider_Error with Message (Diagnostic);
            end if;
         end;
      end return;
   exception
      when others =>
         CS.Free (Library_C);
         CS.Free (Instance_C);
         CS.Free (Aperture_C);
         raise;
   end Open;

   function Is_Open (Object : Session) return Boolean is
     (Object.Handle /= C.Null_Session);

   function Query_Provider_Version
     (Object : Session) return Provider_Version
   is
      Raw        : aliased C.Provider_Version_V1 :=
        (API_Version => 0, Library_Version => 0,
         Vendor => System.Null_Address, Vendor_Capacity => 0,
         Vendor_Required => 0, Description => System.Null_Address,
         Description_Capacity => 0, Description_Required => 0);
      Diagnostic : aliased Diagnostic_Array := (others => Interfaces.C.nul);
      Required   : aliased C.Size_T := 0;
      Status     : Interfaces.Integer_32;
   begin
      if not Is_Open (Object) then
         raise Provider_Error with "provider session is closed";
      end if;
      Status := C.Session_Get_Provider_Version
        (Object.Handle, Raw'Access, Diagnostic'Address, Diagnostic'Length,
         Required'Access);
      if Status /= C.Buffer_Too_Small then
         raise Provider_Error with Message (Diagnostic);
      end if;
      if Raw.Vendor_Required = 0 or else Raw.Description_Required = 0 then
         raise Program_Error with "native provider returned invalid string sizes";
      end if;
      declare
         Vendor_Buffer : aliased Interfaces.C.char_array
           (0 .. Raw.Vendor_Required - 1) := (others => Interfaces.C.nul);
         Description_Buffer : aliased Interfaces.C.char_array
           (0 .. Raw.Description_Required - 1) := (others => Interfaces.C.nul);
      begin
         Raw.Vendor := Vendor_Buffer'Address;
         Raw.Vendor_Capacity := Vendor_Buffer'Length;
         Raw.Description := Description_Buffer'Address;
         Raw.Description_Capacity := Description_Buffer'Length;
         Status := C.Session_Get_Provider_Version
           (Object.Handle, Raw'Access, Diagnostic'Address, Diagnostic'Length,
            Required'Access);
         if Status /= C.Success then
            raise Provider_Error with Message (Diagnostic);
         end if;
         return
           (API_Value => Provider_Version_Number (Raw.API_Version),
            Library_Value => Provider_Version_Number (Raw.Library_Version),
            Vendor_Value => US.To_Unbounded_String
              (Interfaces.C.To_Ada (Vendor_Buffer)),
            Description_Value => US.To_Unbounded_String
              (Interfaces.C.To_Ada (Description_Buffer)));
      end;
   end Query_Provider_Version;

   procedure Close (Object : in out Session) is
      Diagnostic : aliased Diagnostic_Array := (others => Interfaces.C.nul);
      Required   : aliased C.Size_T := 0;
      Status     : constant Interfaces.Integer_32 := C.Session_Close
        (Object.Handle'Access, Diagnostic'Address, Diagnostic'Length,
         Required'Access);
   begin
      if Status /= C.Success then
         raise Provider_Error with Message (Diagnostic);
      end if;
   end Close;

   overriding procedure Finalize (Object : in out Session) is
      Ignored : Interfaces.Integer_32;
   begin
      Ignored := C.Session_Close
        (Object.Handle'Access, System.Null_Address, 0, null);
   exception
      when others =>
         Object.Handle := C.Null_Session;
   end Finalize;

end AMS.MEL;