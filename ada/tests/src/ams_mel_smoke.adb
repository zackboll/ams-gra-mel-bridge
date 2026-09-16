with Ada.Text_IO;
with AMS.MEL;

procedure AMS_MEL_Smoke is
   use type AMS.MEL.Version;
   Value : constant AMS.MEL.Version := AMS.MEL.ABI_Version;
begin
   --  Use explicit checks: do not let a build switch remove test assertions.
   if Value /= (Major => 0, Minor => 1) then
      raise Program_Error with "Unexpected AMS MEL C ABI version";
   end if;
   if AMS.MEL.ABI_Version /= Value then
      raise Program_Error with "Repeated native query changed its result";
   end if;
   Ada.Text_IO.Put_Line ("PASS: Ada -> private C import -> C++ implementation");
end AMS_MEL_Smoke;
