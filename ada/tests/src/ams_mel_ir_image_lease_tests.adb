with Ada.Directories;
with Ada.Environment_Variables;
with Ada.Text_IO;
with AMS.MEL;
with AMS.MEL.IR;
with AMS.MEL.IR.Image;
with GNAT.OS_Lib;
with Interfaces;
with System.Storage_Elements;

package body AMS_MEL_IR_Image_Lease_Tests is
   use type Interfaces.Unsigned_32;
   use type AMS.MEL.IR.Byte;
   use type AMS.MEL.IR.Image_Flip;
   use type GNAT.OS_Lib.File_Descriptor;
   use type GNAT.OS_Lib.String_Access;
   use type System.Storage_Elements.Integer_Address;

   Zero     : constant AMS.MEL.IR.UUID := [others => 0];
   Channel  : constant AMS.MEL.IR.UCI_ID := AMS.MEL.IR.Create_UCI_ID (Zero, "lease channel");
   Platform : constant AMS.MEL.IR.UCI_ID := AMS.MEL.IR.Create_UCI_ID (Zero, "lease platform");
   Location : constant AMS.MEL.IR.Component_Location :=
     AMS.MEL.IR.Create_Component_Location (0.0, 0.0, 0.0, "test", "Ada");
   Config   : constant AMS.MEL.IR.Image_Config :=
     AMS.MEL.IR.Create_Image_Config
       (Channel, Platform, Location, Buffer_Count => 3, Buffer_Size => 64, Queue_Capacity => 4);

   Callback_Marker : exception;

   --  Test-only address observation. Normal consumers never need this; the
   --  public safe API deliberately exposes no System.Address.
   Observed_Address : System.Storage_Elements.Integer_Address := 0;
   Observed_Length  : Natural := 0;

   procedure Observe (Pixels : AMS.MEL.IR.Pixel_Array) is
   begin
      Observed_Length := Pixels'Length;
      Observed_Address :=
        (if Pixels'Length = 0
         then 0
         else System.Storage_Elements.To_Integer (Pixels (Pixels'First)'Address));
   end Observe;

   procedure Raise_Inside (Pixels : AMS.MEL.IR.Pixel_Array) is
   begin
      Observed_Length := Pixels'Length;
      raise Callback_Marker with "deliberate borrow-callback exception";
   end Raise_Inside;

   ---------------------------------------------------------------------------
   --  1, 2: acquire one lease and read exact pixel fidelity through the
   --  borrowed view. The "success" scenario writes id*16+i into each of the
   --  12 Mono8 bytes.
   procedure Test_Acquire_And_Fidelity (Provider_Path : String) is
      Parent : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "ada-lease-success");
      Stream : AMS.MEL.IR.Image_Stream := AMS.MEL.IR.Open_Image_Stream (Parent, Config);
      Seen   : Natural := 0;
      procedure Check_Bytes (Pixels : AMS.MEL.IR.Pixel_Array) is
      begin
         Seen := Pixels'Length;
         if Pixels'First /= 1 or else Pixels'Last /= 12 then
            raise Program_Error with "borrowed pixel view has an unexpected index range";
         end if;
         for Index in Pixels'Range loop
            if Pixels (Index) /= AMS.MEL.IR.Byte (1 * 16 + Index - 1) then
               raise Program_Error with "borrowed pixel fidelity failed";
            end if;
         end loop;
      end Check_Bytes;
   begin
      AMS.MEL.IR.Start (Stream);
      declare
         Frame : AMS.MEL.IR.Image.Frame_Lease := AMS.MEL.IR.Image.Acquire_Frame (Stream, 1_000);
      begin
         if not AMS.MEL.IR.Image.Is_Open (Frame)
           or else AMS.MEL.IR.Image.Frame_ID (Frame) /= 1
           or else AMS.MEL.IR.Image.Width (Frame) /= 4
           or else AMS.MEL.IR.Image.Height (Frame) /= 3
           or else AMS.MEL.IR.Image.Bits_Per_Pixel (Frame) /= 8
           or else AMS.MEL.IR.Image.Number_Of_Bands (Frame) /= 1
           or else AMS.MEL.IR.Image.Image_Flip (Frame) /= AMS.MEL.IR.Horizontal
           or else AMS.MEL.IR.Image.Band_Index (Frame) /= 3
           or else AMS.MEL.IR.Image.Pixel_Count (Frame) /= 12
         then
            raise Program_Error with "lease metadata conversion failed";
         end if;
         AMS.MEL.IR.Image.With_Pixels (Frame, Check_Bytes'Access);
         if Seen /= 12 then
            raise Program_Error with "borrow callback did not run";
         end if;
         AMS.MEL.IR.Image.Close (Frame);
      end;
      AMS.MEL.IR.Close (Stream);
      AMS.MEL.Close (Parent);
   end Test_Acquire_And_Fidelity;

   ---------------------------------------------------------------------------
   --  Test-only scratch directory shared by the address-log regression.
   Directory : GNAT.OS_Lib.String_Access;
   Log       : GNAT.OS_Lib.String_Access;

   procedure Open_Address_Log is
      Descriptor : GNAT.OS_Lib.File_Descriptor;
      Closed     : Boolean;
      Deleted    : Boolean;
   begin
      GNAT.OS_Lib.Create_Temp_File (Descriptor, Directory);
      if Descriptor = GNAT.OS_Lib.Invalid_FD or else Directory = null then
         raise Program_Error with "could not reserve an address-log directory";
      end if;
      GNAT.OS_Lib.Close (Descriptor, Closed);
      GNAT.OS_Lib.Delete_File (Directory.all, Deleted);
      if not Closed or else not Deleted then
         raise Program_Error with "could not prepare an address-log directory";
      end if;
      Ada.Directories.Create_Directory (Directory.all);
      Log := new String'(Directory.all & "/snapshot-addresses.log");
      Ada.Environment_Variables.Set ("AMS_MEL_TEST_SNAPSHOT_ADDRESS_LOG", Log.all);
   end Open_Address_Log;

   procedure Close_Address_Log is
   begin
      Ada.Environment_Variables.Clear ("AMS_MEL_TEST_SNAPSHOT_ADDRESS_LOG");
      if Log /= null and then Ada.Directories.Exists (Log.all) then
         Ada.Directories.Delete_File (Log.all);
      end if;
      if Directory /= null and then Ada.Directories.Exists (Directory.all) then
         Ada.Directories.Delete_Directory (Directory.all);
      end if;
      GNAT.OS_Lib.Free (Log);
      GNAT.OS_Lib.Free (Directory);
   end Close_Address_Log;

   --  Reads one field of the first logged line, whose format since Task 030B
   --  is "<frame_id> <snapshot-address> <size> <provider-image-address>".
   --  Field 2 is the published native span; field 4 is the provider's own
   --  irmel::Buffer::getImageAddress, which makes the full three-way identity
   --  observable from Ada without touching the production ABI.
   function Logged_Field (Index : Positive) return System.Storage_Elements.Integer_Address is
      File  : Ada.Text_IO.File_Type;
      Value : System.Storage_Elements.Integer_Address := 0;
   begin
      Ada.Text_IO.Open (File, Ada.Text_IO.In_File, Log.all);
      declare
         Line  : constant String := Ada.Text_IO.Get_Line (File);
         First : Natural := Line'First;
         Last  : Natural;
         Field : Natural := 0;
      begin
         Ada.Text_IO.Close (File);
         while First <= Line'Last loop
            Last := First;
            while Last <= Line'Last and then Line (Last) /= ' ' loop
               Last := Last + 1;
            end loop;
            Field := Field + 1;
            if Field = Index then
               Value := System.Storage_Elements.Integer_Address'Value (Line (First .. Last - 1));
            end if;
            First := Last + 1;
         end loop;
      end;
      return Value;
   exception
      when others =>
         if Ada.Text_IO.Is_Open (File) then
            Ada.Text_IO.Close (File);
         end if;
         raise;
   end Logged_Field;

   function Logged_Provider_Address return System.Storage_Elements.Integer_Address
   is (Logged_Field (4));

   --  Reads field 2 ("<frame_id> <address> <size>") of the first logged line.
   function Logged_Native_Address return System.Storage_Elements.Integer_Address is
      File  : Ada.Text_IO.File_Type;
      Value : System.Storage_Elements.Integer_Address := 0;
   begin
      Ada.Text_IO.Open (File, Ada.Text_IO.In_File, Log.all);
      declare
         Line  : constant String := Ada.Text_IO.Get_Line (File);
         First : Natural := Line'First;
         Last  : Natural;
         Field : Natural := 0;
      begin
         Ada.Text_IO.Close (File);
         while First <= Line'Last loop
            Last := First;
            while Last <= Line'Last and then Line (Last) /= ' ' loop
               Last := Last + 1;
            end loop;
            Field := Field + 1;
            if Field = 2 then
               Value := System.Storage_Elements.Integer_Address'Value (Line (First .. Last - 1));
            end if;
            First := Last + 1;
         end loop;
      end;
      return Value;
   exception
      when others =>
         if Ada.Text_IO.Is_Open (File) then
            Ada.Text_IO.Close (File);
         end if;
         raise;
   end Logged_Native_Address;

   ---------------------------------------------------------------------------
   --  3, 4: deterministic zero-copy alias proof. The test facade logs the
   --  address of the storage each native snapshot owns; the Ada borrowed view
   --  must report exactly that address, which is only possible if no
   --  intermediate Ada copy exists. Acquire_Frame must not build the view
   --  either, so the observed length stays zero until With_Pixels runs.
   procedure Test_Zero_Copy_Alias (Provider_Path : String) is
   begin
      Open_Address_Log;
      declare
         Parent : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "ada-lease-alias");
         Stream : AMS.MEL.IR.Image_Stream := AMS.MEL.IR.Open_Image_Stream (Parent, Config);
      begin
         AMS.MEL.IR.Start (Stream);
         Observed_Address := 0;
         Observed_Length := 0;
         declare
            Frame : AMS.MEL.IR.Image.Frame_Lease := AMS.MEL.IR.Image.Acquire_Frame (Stream, 1_000);
         begin
            --  Acquisition itself must not have materialized any payload view
            --  or payload-sized Ada container.
            if Observed_Length /= 0 or else Observed_Address /= 0 then
               raise Program_Error with "acquisition constructed a pixel view";
            end if;
            if AMS.MEL.IR.Image.Pixel_Count (Frame) /= 12 then
               raise Program_Error with "lease pixel count failed";
            end if;
            AMS.MEL.IR.Image.With_Pixels (Frame, Observe'Access);
            if Observed_Length /= 12 then
               raise Program_Error with "borrowed view length failed";
            end if;
            if Observed_Address /= Logged_Native_Address then
               raise Program_Error
                 with "Ada borrowed pixel storage is not the native snapshot storage";
            end if;
            --  Task 030B defining proof. The native span is not merely
            --  consistent with Ada; it IS the provider's own image memory:
            --
            --      Buffer.getImageAddress
            --          = native snapshot pixels.data
            --          = Ada With_Pixels first-element address
            --
            --  This can only hold if no bulk payload copy exists anywhere
            --  inside the bridge.
            if Logged_Provider_Address = 0 then
               raise Program_Error with "provider image address was not observed";
            end if;
            if Logged_Provider_Address /= Logged_Native_Address then
               raise Program_Error
                 with "native snapshot storage is not the provider buffer image memory";
            end if;
            if Observed_Address /= Logged_Provider_Address then
               raise Program_Error
                 with "Ada borrowed pixel storage is not the provider buffer image memory";
            end if;
            declare
               First_Address : constant System.Storage_Elements.Integer_Address := Observed_Address;
               Owned         : constant AMS.MEL.IR.Pixel_Array :=
                 AMS.MEL.IR.Image.Copy_Pixels (Frame);
            begin
               --  A second borrow must alias the same storage, not a new copy.
               AMS.MEL.IR.Image.With_Pixels (Frame, Observe'Access);
               if Observed_Address /= First_Address then
                  raise Program_Error with "repeated borrow did not alias the same storage";
               end if;
               --  An explicit owned copy must be independent storage.
               if Owned'Length /= 12
                 or else System.Storage_Elements.To_Integer (Owned (Owned'First)'Address)
                         = First_Address
               then
                  raise Program_Error with "Copy_Pixels did not produce independent storage";
               end if;
            end;
            AMS.MEL.IR.Image.Close (Frame);
         end;
         AMS.MEL.IR.Close (Stream);
         AMS.MEL.Close (Parent);
      end;
      Close_Address_Log;
   exception
      when others =>
         Close_Address_Log;
         raise;
   end Test_Zero_Copy_Alias;

   ---------------------------------------------------------------------------
   --  5, 6, 7: several simultaneous live leases; a later acquisition does not
   --  invalidate an earlier one, and an earlier lease is still processable
   --  after later frames arrived.
   procedure Test_Multiple_Leases (Provider_Path : String) is
      Parent : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "ada-lease-multi");
      Stream : AMS.MEL.IR.Image_Stream := AMS.MEL.IR.Open_Image_Stream (Parent, Config);
   begin
      AMS.MEL.IR.Start (Stream);
      declare
         First  : constant AMS.MEL.IR.Image.Frame_Lease :=
           AMS.MEL.IR.Image.Acquire_Frame (Stream, 1_000);
         Second : AMS.MEL.IR.Image.Frame_Lease := AMS.MEL.IR.Image.Acquire_Frame (Stream, 1_000);
         Third  : constant AMS.MEL.IR.Image.Frame_Lease :=
           AMS.MEL.IR.Image.Acquire_Frame (Stream, 1_000);
      begin
         if not AMS.MEL.IR.Image.Is_Open (First)
           or else not AMS.MEL.IR.Image.Is_Open (Second)
           or else not AMS.MEL.IR.Image.Is_Open (Third)
           or else AMS.MEL.IR.Image.Frame_ID (First) /= 1
           or else AMS.MEL.IR.Image.Frame_ID (Second) /= 2
           or else AMS.MEL.IR.Image.Frame_ID (Third) /= 3
         then
            raise Program_Error with "three simultaneous leases failed";
         end if;
         --  Process the oldest lease last, after two later frames arrived.
         AMS.MEL.IR.Image.With_Pixels (Third, Observe'Access);
         AMS.MEL.IR.Image.With_Pixels (First, Observe'Access);
         if Observed_Length /= 12 then
            raise Program_Error with "earliest lease lost its payload";
         end if;
         declare
            Bytes : constant AMS.MEL.IR.Pixel_Array := AMS.MEL.IR.Image.Copy_Pixels (First);
         begin
            for Index in Bytes'Range loop
               if Bytes (Index) /= AMS.MEL.IR.Byte (1 * 16 + Index - 1) then
                  raise Program_Error with "earliest lease payload changed";
               end if;
            end loop;
         end;
         --  Closing one lease must not disturb the others.
         AMS.MEL.IR.Image.Close (Second);
         if AMS.MEL.IR.Image.Is_Open (Second)
           or else not AMS.MEL.IR.Image.Is_Open (First)
           or else not AMS.MEL.IR.Image.Is_Open (Third)
         then
            raise Program_Error with "closing one lease disturbed another";
         end if;
         AMS.MEL.IR.Image.With_Pixels (Third, Observe'Access);
         if Observed_Length /= 12 then
            raise Program_Error with "sibling lease invalidated by an unrelated close";
         end if;
      end;
      AMS.MEL.IR.Close (Stream);
      AMS.MEL.Close (Parent);
   end Test_Multiple_Leases;

   ---------------------------------------------------------------------------
   --  8, 9, 10, 11: explicit close, double close, automatic finalization, and
   --  borrow-callback exception safety.
   procedure Test_Lifecycle_And_Exceptions (Provider_Path : String) is
      Parent : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "ada-lease-lifecycle");
      Stream : AMS.MEL.IR.Image_Stream := AMS.MEL.IR.Open_Image_Stream (Parent, Config);
      Raised : Boolean := False;
   begin
      AMS.MEL.IR.Start (Stream);
      declare
         Frame : AMS.MEL.IR.Image.Frame_Lease := AMS.MEL.IR.Image.Acquire_Frame (Stream, 1_000);
      begin
         Observed_Length := 0;
         begin
            AMS.MEL.IR.Image.With_Pixels (Frame, Raise_Inside'Access);
         exception
            when Callback_Marker =>
               Raised := True;
         end;
         if not Raised or else Observed_Length /= 12 then
            raise Program_Error with "borrow callback exception did not propagate";
         end if;
         --  The lease survives a callback exception.
         if not AMS.MEL.IR.Image.Is_Open (Frame) then
            raise Program_Error with "callback exception invalidated the lease";
         end if;
         AMS.MEL.IR.Image.With_Pixels (Frame, Observe'Access);
         if Observed_Length /= 12 then
            raise Program_Error with "lease unusable after a callback exception";
         end if;
         AMS.MEL.IR.Image.Close (Frame);
         if AMS.MEL.IR.Image.Is_Open (Frame) then
            raise Program_Error with "explicit close did not clear the lease";
         end if;
         --  Repeated close is safe, and the following finalization of an
         --  explicitly closed lease must also remain harmless.
         AMS.MEL.IR.Image.Close (Frame);
         AMS.MEL.IR.Image.Close (Frame);
      end;
      --  Automatic finalization of a never-closed lease releases the snapshot.
      declare
         Frame : constant AMS.MEL.IR.Image.Frame_Lease :=
           AMS.MEL.IR.Image.Acquire_Frame (Stream, 1_000);
      begin
         if not AMS.MEL.IR.Image.Is_Open (Frame) then
            raise Program_Error with "second acquisition failed";
         end if;
      end;
      AMS.MEL.IR.Close (Stream);
      AMS.MEL.Close (Parent);
   end Test_Lifecycle_And_Exceptions;

   ---------------------------------------------------------------------------
   --  15, 16, 17, 18, 19: the lease keeps its payload valid across stream
   --  Stop, stream Close, and Session close, in that order, and an owned copy
   --  survives the lease itself.
   --
   --  Since Task 030B the borrowed bytes are the provider's own buffer, so
   --  this holds because actual provider teardown is DEFERRED while the lease
   --  is live, not because the payload was copied. The C suite asserts the
   --  ordering directly (channel not destroyed and library not unloaded until
   --  after the final Buffer.release). The owned Copy_Pixels result remains
   --  genuinely independent and is still valid after the lease is closed.
   procedure Test_Teardown_With_Live_Lease (Provider_Path : String) is
      Parent : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "ada-lease-teardown");
      Stream : AMS.MEL.IR.Image_Stream := AMS.MEL.IR.Open_Image_Stream (Parent, Config);
   begin
      AMS.MEL.IR.Start (Stream);
      declare
         Frame : AMS.MEL.IR.Image.Frame_Lease := AMS.MEL.IR.Image.Acquire_Frame (Stream, 1_000);
         procedure Verify (Stage : String) is
            Bytes : constant AMS.MEL.IR.Pixel_Array := AMS.MEL.IR.Image.Copy_Pixels (Frame);
         begin
            AMS.MEL.IR.Image.With_Pixels (Frame, Observe'Access);
            if Observed_Length /= 12 or else Bytes'Length /= 12 then
               raise Program_Error with "live lease lost its payload after " & Stage;
            end if;
            for Index in Bytes'Range loop
               if Bytes (Index) /= AMS.MEL.IR.Byte (1 * 16 + Index - 1) then
                  raise Program_Error with "live lease payload changed after " & Stage;
               end if;
            end loop;
         end Verify;
      begin
         Verify ("acquisition");
         AMS.MEL.IR.Stop (Stream);
         Verify ("stream Stop");
         AMS.MEL.IR.Close (Stream);
         Verify ("stream Close");
         AMS.MEL.Close (Parent);
         Verify ("Session close");
         declare
            Owned : constant AMS.MEL.IR.Pixel_Array := AMS.MEL.IR.Image.Copy_Pixels (Frame);
         begin
            AMS.MEL.IR.Image.Close (Frame);
            for Index in Owned'Range loop
               if Owned (Index) /= AMS.MEL.IR.Byte (1 * 16 + Index - 1) then
                  raise Program_Error with "owned copy did not survive lease destruction";
               end if;
            end loop;
         end;
      end;
   end Test_Teardown_With_Live_Lease;

   ---------------------------------------------------------------------------
   --  12, 13, 14: the test facade publishes the three degenerate pixel spans a
   --  real provider cannot produce. A zero-length payload is valid and yields
   --  an empty borrowed view; a null pointer with a nonzero size and a size
   --  that cannot be an Ada index both fail closed with Provider_Error and
   --  leak no snapshot.
   procedure Test_Malformed_Spans (Provider_Path : String) is
      procedure With_Span (Shape : String; Expect_Rejection : Boolean) is
         Parent : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "ada-lease-span");
         Stream : AMS.MEL.IR.Image_Stream := AMS.MEL.IR.Open_Image_Stream (Parent, Config);
         Failed : Boolean := False;
      begin
         Ada.Environment_Variables.Set ("AMS_MEL_TEST_SNAPSHOT_PIXEL_SPAN", Shape);
         AMS.MEL.IR.Start (Stream);
         begin
            declare
               Frame : AMS.MEL.IR.Image.Frame_Lease :=
                 AMS.MEL.IR.Image.Acquire_Frame (Stream, 1_000);
            begin
               if Expect_Rejection then
                  raise Program_Error with "malformed span " & Shape & " was accepted";
               end if;
               --  Zero-length payloads are valid and must borrow safely.
               Observed_Length := 12;
               Observed_Address := 1;
               AMS.MEL.IR.Image.With_Pixels (Frame, Observe'Access);
               if AMS.MEL.IR.Image.Pixel_Count (Frame) /= 0
                 or else Observed_Length /= 0
                 or else Observed_Address /= 0
               then
                  raise Program_Error with "zero-length payload borrow failed";
               end if;
               if AMS.MEL.IR.Image.Copy_Pixels (Frame)'Length /= 0 then
                  raise Program_Error with "zero-length owned copy failed";
               end if;
               AMS.MEL.IR.Image.Close (Frame);
            end;
         exception
            when AMS.MEL.Provider_Error =>
               Failed := True;
         end;
         if Failed /= Expect_Rejection then
            raise Program_Error with "unexpected outcome for malformed span " & Shape;
         end if;
         Ada.Environment_Variables.Clear ("AMS_MEL_TEST_SNAPSHOT_PIXEL_SPAN");
         AMS.MEL.IR.Close (Stream);
         AMS.MEL.Close (Parent);
      exception
         when others =>
            Ada.Environment_Variables.Clear ("AMS_MEL_TEST_SNAPSHOT_PIXEL_SPAN");
            raise;
      end With_Span;
   begin
      With_Span ("empty", Expect_Rejection => False);
      With_Span ("null-nonzero", Expect_Rejection => True);
      With_Span ("oversize", Expect_Rejection => True);
   end Test_Malformed_Spans;

   ---------------------------------------------------------------------------
   --  Failed acquisition must leak nothing and must leave the returned owner
   --  safely uninitialized, so Is_Open stays False and finalization is inert.
   procedure Test_Failed_Acquisition (Provider_Path : String) is
      Parent : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "idle");
      Stream : AMS.MEL.IR.Image_Stream := AMS.MEL.IR.Open_Image_Stream (Parent, Config);
      Timed  : Boolean := False;
      Ended  : Boolean := False;
   begin
      AMS.MEL.IR.Start (Stream);
      begin
         declare
            Frame : constant AMS.MEL.IR.Image.Frame_Lease :=
              AMS.MEL.IR.Image.Acquire_Frame (Stream, 2);
         begin
            raise Program_Error
              with "idle stream leased" & AMS.MEL.IR.Image.Pixel_Count (Frame)'Image;
         end;
      exception
         when AMS.MEL.IR.Timeout_Error =>
            Timed := True;
      end;
      AMS.MEL.IR.Stop (Stream);
      begin
         declare
            Frame : constant AMS.MEL.IR.Image.Frame_Lease :=
              AMS.MEL.IR.Image.Acquire_Frame (Stream);
         begin
            raise Program_Error
              with "stopped stream leased" & AMS.MEL.IR.Image.Pixel_Count (Frame)'Image;
         end;
      exception
         when AMS.MEL.IR.Stream_Stopped =>
            Ended := True;
      end;
      if not Timed or else not Ended then
         raise Program_Error with "lease acquisition terminal semantics failed";
      end if;
      AMS.MEL.IR.Close (Stream);
      AMS.MEL.Close (Parent);
   end Test_Failed_Acquisition;

   ---------------------------------------------------------------------------
   --  Repeated acquire/borrow/release, multiple outstanding leases, and
   --  callback exception paths, run enough times to expose a leak or a
   --  double release under the contract-test facade.
   procedure Test_Stress (Provider_Path : String) is
      Rounds : constant := 40;
   begin
      for Round in 1 .. Rounds loop
         declare
            Parent : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "ada-lease-stress");
            Stream : AMS.MEL.IR.Image_Stream := AMS.MEL.IR.Open_Image_Stream (Parent, Config);
         begin
            AMS.MEL.IR.Start (Stream);
            declare
               First  : AMS.MEL.IR.Image.Frame_Lease :=
                 AMS.MEL.IR.Image.Acquire_Frame (Stream, 1_000);
               Second : constant AMS.MEL.IR.Image.Frame_Lease :=
                 AMS.MEL.IR.Image.Acquire_Frame (Stream, 1_000);
            begin
               AMS.MEL.IR.Image.With_Pixels (First, Observe'Access);
               AMS.MEL.IR.Image.With_Pixels (Second, Observe'Access);
               if Observed_Length /= 12 then
                  raise Program_Error with "stress borrow failed in round" & Round'Image;
               end if;
               begin
                  AMS.MEL.IR.Image.With_Pixels (Second, Raise_Inside'Access);
                  raise Program_Error with "stress callback exception was swallowed";
               exception
                  when Callback_Marker =>
                     null;
               end;
               --  First is closed explicitly; Second is left to finalization.
               AMS.MEL.IR.Image.Close (First);
            end;
            if Round mod 2 = 0 then
               AMS.MEL.IR.Stop (Stream);
            end if;
            AMS.MEL.IR.Close (Stream);
            AMS.MEL.Close (Parent);
         end;
      end loop;
   end Test_Stress;

   ---------------------------------------------------------------------------
   --  Task 030B backpressure, observed from Ada. With Buffer_Count = 3, three
   --  simultaneously live leases hold all three provider buffers, so the
   --  provider cannot produce a fourth frame until a lease is released. This
   --  asserts the intended ownership model rather than treating it as a bug:
   --  acquisition with a short timeout must time out, and must succeed again
   --  once exactly one lease has been closed.
   procedure Test_Backpressure (Provider_Path : String) is
      Parent  : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "ada-lease-backpressure");
      Stream  : AMS.MEL.IR.Image_Stream := AMS.MEL.IR.Open_Image_Stream (Parent, Config);
      Starved : Boolean := False;
   begin
      AMS.MEL.IR.Start (Stream);
      declare
         First  : AMS.MEL.IR.Image.Frame_Lease := AMS.MEL.IR.Image.Acquire_Frame (Stream, 1_000);
         Second : constant AMS.MEL.IR.Image.Frame_Lease :=
           AMS.MEL.IR.Image.Acquire_Frame (Stream, 1_000);
         Third  : constant AMS.MEL.IR.Image.Frame_Lease :=
           AMS.MEL.IR.Image.Acquire_Frame (Stream, 1_000);
      begin
         if not AMS.MEL.IR.Image.Is_Open (First)
           or else not AMS.MEL.IR.Image.Is_Open (Second)
           or else not AMS.MEL.IR.Image.Is_Open (Third)
         then
            raise Program_Error with "three simultaneous leases failed";
         end if;
         --  All three provider buffers are checked out, so no further frame
         --  can arrive while every lease is retained.
         begin
            declare
               Extra : constant AMS.MEL.IR.Image.Frame_Lease :=
                 AMS.MEL.IR.Image.Acquire_Frame (Stream, 50);
            begin
               raise Program_Error
                 with
                   "a fourth frame arrived while every provider buffer was leased:"
                   & AMS.MEL.IR.Image.Pixel_Count (Extra)'Image;
            end;
         exception
            when AMS.MEL.IR.Timeout_Error | AMS.MEL.IR.Stream_Stopped =>
               Starved := True;
         end;
         if not Starved then
            raise Program_Error
              with "a fourth frame arrived while every provider buffer was leased";
         end if;
         --  Releasing exactly one lease frees exactly one provider buffer,
         --  and the still-live leases are unaffected.
         AMS.MEL.IR.Image.Close (First);
         AMS.MEL.IR.Image.With_Pixels (Second, Observe'Access);
         if Observed_Length /= 12 then
            raise Program_Error with "a sibling lease was disturbed by an unrelated release";
         end if;
         AMS.MEL.IR.Image.With_Pixels (Third, Observe'Access);
         if Observed_Length /= 12 then
            raise Program_Error with "a sibling lease was disturbed by an unrelated release";
         end if;
      end;
      AMS.MEL.IR.Close (Stream);
      AMS.MEL.Close (Parent);
   end Test_Backpressure;

   ---------------------------------------------------------------------------
   --  Owned copying compatibility. Image.Receive/Full_Frame must stay owned:
   --  the payload is copied into Ada storage and the provider buffer is
   --  released before Receive returns, so the result is fully independent of
   --  stream, Session, and provider lifetime. Task 030B must not have made
   --  this API borrowed.
   procedure Test_Owned_Full_Frame (Provider_Path : String) is
      Parent : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "ada-lease-owned");
      Stream : AMS.MEL.IR.Image_Stream := AMS.MEL.IR.Open_Image_Stream (Parent, Config);
   begin
      AMS.MEL.IR.Start (Stream);
      declare
         Owned : constant AMS.MEL.IR.Image.Full_Frame := AMS.MEL.IR.Image.Receive (Stream, 1_000);
         Bytes : constant AMS.MEL.IR.Pixel_Array := AMS.MEL.IR.Image.Pixels (Owned);
      begin
         if Bytes'Length /= 12 or else AMS.MEL.IR.Image.Frame_ID (Owned) /= 1 then
            raise Program_Error with "owned Full_Frame reception failed";
         end if;
         --  Tear the whole provider graph down, with no lease outstanding.
         AMS.MEL.IR.Close (Stream);
         AMS.MEL.Close (Parent);
         --  The owned value is still intact after actual provider teardown.
         declare
            After : constant AMS.MEL.IR.Pixel_Array := AMS.MEL.IR.Image.Pixels (Owned);
         begin
            if After'Length /= 12 then
               raise Program_Error with "owned Full_Frame did not survive provider teardown";
            end if;
            for Index in After'Range loop
               if After (Index) /= AMS.MEL.IR.Byte (1 * 16 + Index - 1) then
                  raise Program_Error with "owned Full_Frame payload changed";
               end if;
            end loop;
         end;
      end;
   end Test_Owned_Full_Frame;

   procedure Run (Provider_Path : String) is
   begin
      Test_Acquire_And_Fidelity (Provider_Path);
      Test_Zero_Copy_Alias (Provider_Path);
      Test_Multiple_Leases (Provider_Path);
      Test_Lifecycle_And_Exceptions (Provider_Path);
      Test_Teardown_With_Live_Lease (Provider_Path);
      Test_Malformed_Spans (Provider_Path);
      Test_Failed_Acquisition (Provider_Path);
      --  Task 030B provider-buffer zero copy.
      Test_Backpressure (Provider_Path);
      Test_Owned_Full_Frame (Provider_Path);
      Test_Stress (Provider_Path);
      Ada.Text_IO.Put_Line ("PASS: Ada IR zero-copy frame lease contract");
   end Run;
end AMS_MEL_IR_Image_Lease_Tests;
