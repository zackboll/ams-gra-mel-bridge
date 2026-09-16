with Interfaces;
with AMS.MEL_C_API;

package body AMS.MEL is
   function ABI_Version return Version is
      use type Interfaces.Integer_32;
      package C renames AMS.MEL_C_API;
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
end AMS.MEL;
