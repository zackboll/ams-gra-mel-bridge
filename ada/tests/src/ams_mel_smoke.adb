with Ada.Text_IO;
with Ada.Command_Line;
with Ada.Directories;
with AMS.MEL;

procedure AMS_MEL_Smoke is
   use type AMS.MEL.Version;
   use type AMS.MEL.Provider_Version_Number;

   Workspace_Root : constant String := Ada.Directories.Full_Name
     (Ada.Directories.Containing_Directory
        (Ada.Directories.Containing_Directory
           (Ada.Directories.Containing_Directory
              (Ada.Directories.Containing_Directory
                 (Ada.Command_Line.Command_Name)))));
   Provider_Path : constant String := Workspace_Root
     & "/native/build/test-providers/libmock_ir_provider.so";

   procedure Test_Explicit_Close is
      Object : AMS.MEL.Session := AMS.MEL.Open
        (Provider_Path, "ada-explicit", "aperture-A");
      Value  : constant AMS.MEL.Provider_Version :=
        AMS.MEL.Query_Provider_Version (Object);
   begin
      if not AMS.MEL.Is_Open (Object)
        or else AMS.MEL.API_Version (Value) /= 16#1234_5678#
        or else AMS.MEL.Library_Version (Value) /= 16#90AB_CDEF#
        or else AMS.MEL.Vendor (Value) /= "Mock IR Provider µ"
        or else AMS.MEL.Description (Value) /=
          "Deterministic task 001 provider"
      then
         raise Program_Error with "provider version conversion failed";
      end if;
      AMS.MEL.Close (Object);
      if AMS.MEL.Is_Open (Object) then
         raise Program_Error with "explicit close did not clear owner";
      end if;
      AMS.MEL.Close (Object);
      --  Finalization of this explicitly closed object must remain harmless.
   end Test_Explicit_Close;

   procedure Test_Finalization_Only is
      Object : AMS.MEL.Session := AMS.MEL.Open
        (Provider_Path, "ada-finalize", "aperture-B");
   begin
      if not AMS.MEL.Is_Open (Object) then
         raise Program_Error with "provider session did not open";
      end if;
      --  Scope exit exercises the non-raising finalization fallback.
   end Test_Finalization_Only;

   procedure Test_Open_Failure is
      Object : AMS.MEL.Session := AMS.MEL.Open
        ("/definitely/missing/libirmel.so", "missing");
   begin
      if AMS.MEL.Is_Open (Object) then
         raise Program_Error with "missing provider unexpectedly opened";
      end if;
   end Test_Open_Failure;

   Value : constant AMS.MEL.Version := AMS.MEL.ABI_Version;
begin
   if Value /= (Major => 0, Minor => 1)
     or else AMS.MEL.ABI_Version /= Value
   then
      raise Program_Error with "unexpected AMS MEL C ABI version";
   end if;
   Test_Explicit_Close;
   Test_Finalization_Only;
   begin
      Test_Open_Failure;
      raise Program_Error with "missing-library exception was not raised";
   exception
      when AMS.MEL.Provider_Error =>
         null;
   end;
   Ada.Text_IO.Put_Line
     ("PASS: Ada provider load/init/version/close/finalization contract");
end AMS_MEL_Smoke;