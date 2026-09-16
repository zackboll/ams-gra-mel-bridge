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
   Lifetime_Log : constant String := Workspace_Root
     & "/native/build/provider-lifetime.log";

   procedure Remove_Lifetime_Log is
   begin
      if Ada.Directories.Exists (Lifetime_Log) then
         Ada.Directories.Delete_File (Lifetime_Log);
      end if;
   end Remove_Lifetime_Log;

   function Lifetime_Events return String is
      Stream : Ada.Text_IO.File_Type;
      Result : String (1 .. 1_024);
      Last   : Natural := 0;
   begin
      Ada.Text_IO.Open (Stream, Ada.Text_IO.In_File, Lifetime_Log);
      while not Ada.Text_IO.End_Of_File (Stream) loop
         declare
            Line : constant String := Ada.Text_IO.Get_Line (Stream);
         begin
            Result (Last + 1 .. Last + Line'Length) := Line;
            Last := Last + Line'Length;
            Result (Last + 1) := Character'Val (10);
            Last := Last + 1;
         end;
      end loop;
      Ada.Text_IO.Close (Stream);
      return Result (1 .. Last);
   exception
      when others =>
         if Ada.Text_IO.Is_Open (Stream) then
            Ada.Text_IO.Close (Stream);
         end if;
         raise;
   end Lifetime_Events;

   Expected_Lifetime : constant String :=
     "manager_factory_called" & Character'Val (10)
     & "control_factory_called" & Character'Val (10)
     & "init_called" & Character'Val (10)
     & "control_destroyed" & Character'Val (10)
     & "manager_destroyed" & Character'Val (10)
     & "library_unloaded" & Character'Val (10);

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
   begin
      Remove_Lifetime_Log;
      declare
         Object : AMS.MEL.Session := AMS.MEL.Open
           (Provider_Path, "ada-finalize", "aperture-B");
      begin
         if not AMS.MEL.Is_Open (Object) then
            raise Program_Error with "provider session did not open";
         end if;
      end;
      if Lifetime_Events /= Expected_Lifetime then
         raise Program_Error with
           "finalization did not destroy provider resources in order";
      end if;
   end Test_Finalization_Only;

   procedure Test_Embedded_NUL is
      NUL : constant Character := Character'Val (0);
   begin
      Remove_Lifetime_Log;
      begin
         declare
            Object : AMS.MEL.Session := AMS.MEL.Open
              (Provider_Path & NUL & "ignored", "nul-library");
         begin
            AMS.MEL.Close (Object);
            raise Program_Error with "Library_Path NUL was accepted";
         end;
      exception
         when Constraint_Error => null;
      end;
      begin
         declare
            Object : AMS.MEL.Session := AMS.MEL.Open
              (Provider_Path, "nul" & NUL & "instance");
         begin
            AMS.MEL.Close (Object);
            raise Program_Error with "Instance NUL was accepted";
         end;
      exception
         when Constraint_Error => null;
      end;
      begin
         declare
            Object : AMS.MEL.Session := AMS.MEL.Open
              (Provider_Path, "nul-aperture", "aperture" & NUL & "ignored");
         begin
            AMS.MEL.Close (Object);
            raise Program_Error with "Aperture_Config_ID NUL was accepted";
         end;
      exception
         when Constraint_Error => null;
      end;
      if Ada.Directories.Exists (Lifetime_Log) then
         raise Program_Error with "rejected NUL input invoked the provider";
      end if;
   end Test_Embedded_NUL;

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
   Test_Embedded_NUL;
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
