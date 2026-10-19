with Ada.Text_IO;
with AMS.MEL;
with Example_Support;

procedure Provider_Session is
   use Example_Support;
   use type AMS.MEL.Provider_Version_Number;
begin
   declare
      Session : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, Configuration);
      Version : constant AMS.MEL.Provider_Version := AMS.MEL.Query_Provider_Version (Session);
   begin
      Require (AMS.MEL.API_Version (Version) = 16#1234_5678#, "mock API version");
      Require (AMS.MEL.Library_Version (Version) = 16#90AB_CDEF#, "mock library version");
      Require (AMS.MEL.Vendor (Version) = "Mock IR Provider µ", "mock vendor");
      AMS.MEL.Close (Session);
      Require (not AMS.MEL.Is_Open (Session), "Session still open");
      Require
        (AMS.MEL.Description (Version) = "Deterministic task 001 provider", "owned description");
      Ada.Text_IO.Put_Line ("vendor after Close: " & AMS.MEL.Vendor (Version));
      Ada.Text_IO.Put_Line ("description: " & AMS.MEL.Description (Version));
      Ada.Text_IO.Put_Line ("API=0x12345678 library=0x90ABCDEF");
   end;
exception
   when Error : others =>
      Report_Failure (Error);
end Provider_Session;
