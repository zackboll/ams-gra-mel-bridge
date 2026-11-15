with Ada.Text_IO;
with AMS.MEL;
with AMS.MEL.IR.Buffer;

package body AMS_MEL_IR_Buffer_Tests is
   package B renames AMS.MEL.IR.Buffer;
   use type AMS.MEL.IR.Pixel_Array;
   use type B.Flag_Array;
   use type B.Image_Address_Kind;

   procedure Require (Condition : Boolean; Message : String) is
   begin
      if not Condition then
         raise Program_Error with Message;
      end if;
   end Require;

   procedure Normal (Provider_Path : String; Context : Long_Long_Integer) is
      Parent   : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "foundation-normal", "");
      Object   : B.Host_Buffer := B.Open (Parent, 32, Context);
      Other    : B.Host_Buffer := B.Open (Parent, 32, -9);
      Pattern  : constant AMS.MEL.IR.Pixel_Array (7 .. 38) := [others => 16#A5#];
      Expected : constant B.Flag_Array := [B.RDMA, B.Host, B.GPU, B.RDMA, B.RDMA, B.GPU];
      Info     : B.Buffer_Properties;
   begin
      Require (B.Is_Open (Object), "Buffer construction");
      Require
        (B.Size_Bytes (Object) = 32 and B.User_Context (Object) = Context,
         "Buffer scalar properties");
      Info := B.Properties (Object);
      Require
        (Info.Provider_Size = 32
         and Info.Initialized_Context = Context
         and Info.Provider_Context = Context,
         "exact provider properties");
      Require
        (Info.Buffer_Address_Matches and Info.Image_Address = B.Inside_Storage,
         "pointer classification");
      Require (B.Read_Storage (Object, 0, 32) = [1 .. 32 => 0], "zeroed bytes");
      B.Write_Storage (Object, 0, Pattern);
      Require (B.Read_Storage (Object, 0, 32) = Pattern, "pattern roundtrip");
      Require (B.Read_Storage (Other, 0, 32) = [1 .. 32 => 0], "independent bytes");
      Require (B.Flags (Object)'Length = 0, "empty flags");
      B.Add_Flag (Object, B.Host);
      Require (B.Flags (Object) = [1 => B.Host], "Host flag");
      B.Set_Flags (Object, Expected (1 .. 5));
      B.Add_Flag (Object, B.GPU);
      Require (B.Flags (Object) = Expected, "flag order and duplicates");
      B.Set_Flags (Object, [1 .. 0 => B.Host]);
      Require (B.Flags (Object)'Length = 0, "empty replacement");
      B.Write_Storage (Object, 32, [1 .. 0 => 0]);
      Require (B.Read_Storage (Object, 32, 0)'Length = 0, "empty access at capacity");
      begin
         B.Write_Storage (Object, 31, Pattern);
         raise Program_Error with "out-of-bounds write accepted";
      exception
         when Constraint_Error =>
            null;
      end;
      begin
         declare
            Unexpected : constant AMS.MEL.IR.Pixel_Array :=
              B.Read_Storage (Object, Natural'Last, Natural'Last);
         begin
            raise Program_Error with "overflow read accepted" & Unexpected'Length'Image;
         end;
      exception
         when Constraint_Error =>
            null;
      end;
      AMS.MEL.Close (Parent);
      Require (B.Read_Storage (Object, 0, 32) = Pattern, "bytes after Session closure");
      Require (B.Properties (Object).Buffer_Address_Matches, "stable address after operations");
      B.Close (Other);
      B.Close (Object);
      B.Close (Object);
      Require (not B.Is_Open (Object), "idempotent Close");
      for Operation in 1 .. 7 loop
         begin
            case Operation is
               when 1 =>
                  Info := B.Properties (Object);

               when 2 =>
                  B.Add_Flag (Object, B.Host);

               when 3 =>
                  B.Set_Flags (Object, [B.Host]);

               when 4 =>
                  B.Write_Storage (Object, 0, Pattern);

               when 5 =>
                  Require (B.Read_Storage (Object, 0, 1)'Length = 1, "closed read");

               when 6 =>
                  Require (B.Flags (Object)'Length = 0, "closed flags");

               when 7 =>
                  Require (B.User_Context (Object) = Context, "closed context");
            end case;
            raise Program_Error with "closed operation accepted";
         exception
            when AMS.MEL.Provider_Error =>
               null;
         end;
      end loop;
      begin
         declare
            Unexpected : constant B.Host_Buffer := B.Open (Parent, 32, 0);
         begin
            raise Program_Error with "closed parent accepted" & B.Is_Open (Unexpected)'Image;
         end;
      exception
         when AMS.MEL.Provider_Error =>
            null;
      end;
   end Normal;

   procedure Failure (Provider_Path, Scenario : String; Operation : Natural) is
      Parent : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, Scenario, "");
   begin
      begin
         declare
            Object : B.Host_Buffer := B.Open (Parent, 16, -23);
         begin
            case Operation is
               when 0      =>
                  null;

               when 1      =>
                  Require (B.Flags (Object)'Length = 0, "unexpected flags");

               when 2      =>
                  B.Add_Flag (Object, B.Host);

               when 3      =>
                  B.Set_Flags (Object, [B.Host]);

               when 4      =>
                  Require (B.Properties (Object).Buffer_Address_Matches, "unexpected properties");

               when 5      =>
                  AMS.MEL.Close (Parent);
                  begin
                     B.Close (Object);
                  exception
                     when AMS.MEL.Provider_Error =>
                        Require (B.Is_Open (Object), "uncertain Close discarded owner");
                        raise;
                  end;

               when others =>
                  raise Program_Error;
            end case;
         end;
         raise Program_Error with "Buffer failure accepted: " & Scenario;
      exception
         when AMS.MEL.Provider_Error =>
            null;
      end;
      AMS.MEL.Close (Parent);
   end Failure;

   procedure Run (Provider_Path : String) is
   begin
      Normal (Provider_Path, 0);
      Normal (Provider_Path, 52);
      Normal (Provider_Path, Long_Long_Integer'First);
      Normal (Provider_Path, Long_Long_Integer'Last);
      Failure (Provider_Path, "foundation-factory-null", 0);
      Failure (Provider_Path, "foundation-factory-throw", 0);
      Failure (Provider_Path, "foundation-factory-allocation", 0);
      Failure (Provider_Path, "foundation-init-refuse", 0);
      Failure (Provider_Path, "foundation-get-refuse", 1);
      Failure (Provider_Path, "foundation-get-throw", 1);
      Failure (Provider_Path, "foundation-unknown-flag", 1);
      Failure (Provider_Path, "foundation-add-throw", 2);
      Failure (Provider_Path, "foundation-set-throw", 3);
      Failure (Provider_Path, "foundation-info-throw", 4);
      declare
         Parent : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "foundation-image-null", "");
         Object : constant B.Host_Buffer := B.Open (Parent, 16, -1);
      begin
         Require (B.Properties (Object).Image_Address = B.Null_Image, "null image address");
         AMS.MEL.Close (Parent);
      --  Successful finalization without explicit Close.
      end;
      declare
         Parent : AMS.MEL.Session := AMS.MEL.Open (Provider_Path, "foundation-image-outside", "");
         Object : constant B.Host_Buffer := B.Open (Parent, 16, -1);
      begin
         Require (B.Properties (Object).Image_Address = B.Outside_Storage, "outside image address");
         AMS.MEL.Close (Parent);
      end;
      Failure (Provider_Path, "foundation-init-throw", 0);
      Failure (Provider_Path, "foundation-retained", 5);
      --  Scope completion above also proves non-raising uncertain finalization.
      Ada.Text_IO.Put_Line ("PASS: Ada standalone host Buffer foundation");
   end Run;
end AMS_MEL_IR_Buffer_Tests;
