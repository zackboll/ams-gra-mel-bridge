with Ada.Unchecked_Conversion;
with Interfaces.C.Strings;
with System;

package body AMS.MEL.RF is
   package C renames AMS.MEL_C_API;
   package CS renames Interfaces.C.Strings;
   package US renames Ada.Strings.Unbounded;
   use type Interfaces.Integer_32;
   use type Interfaces.Unsigned_32;
   use type Interfaces.Unsigned_64;
   use type Interfaces.C.char;
   use type C.Size_T;
   use type C.RF_Data_Handle;
   use type System.Address;

   type Diagnostic is array (C.Size_T range <>) of aliased Interfaces.C.char with Convention => C;
   subtype Fixed_Diagnostic is Diagnostic (0 .. 511);
   type String_Owner is new Ada.Finalization.Limited_Controlled with record
      Value : CS.chars_ptr := CS.Null_Ptr;
   end record;
   overriding
   procedure Finalize (Value : in out String_Owner) is
   begin
      CS.Free (Value.Value);
   end Finalize;

   function Message (Buffer : Diagnostic) return String is
      Last : Natural := 0;
   begin
      while Last < Buffer'Length and then Buffer (C.Size_T (Last)) /= Interfaces.C.nul loop
         Last := Last + 1;
      end loop;
      declare
         Result : String (1 .. Last);
      begin
         for I in Result'Range loop
            Result (I) := Character'Val (Interfaces.C.char'Pos (Buffer (C.Size_T (I - 1))));
         end loop;
         return (if Last = 0 then "native RF operation failed" else Result);
      end;
   end Message;

   procedure Check (Code : Interfaces.Integer_32; Buffer : Diagnostic) is
   begin
      if Code /= C.Success then
         raise Provider_Error with Message (Buffer);
      end if;
   end Check;

   function Open (Library_Path : String; Configuration : String) return Data_MEL is
      Library_C, Configuration_C : String_Owner;
      D                          : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      R                          : aliased C.Size_T := 0;
   begin
      for Item of Library_Path loop
         if Item = Character'Val (0) then
            raise Constraint_Error with "Library_Path contains an embedded NUL";
         end if;
      end loop;
      for Item of Configuration loop
         if Item = Character'Val (0) then
            raise Constraint_Error with "Configuration contains an embedded NUL";
         end if;
      end loop;
      Library_C.Value := CS.New_String (Library_Path);
      Configuration_C.Value := CS.New_String (Configuration);
      return Result : Data_MEL do
         Check
           (C.RF_Data_Open
              (Library_C.Value,
               Configuration_C.Value,
               Result.Handle'Access,
               D'Address,
               D'Length,
               R'Access),
            D);
      end return;
   end Open;

   function Is_Open (Object : Data_MEL) return Boolean
   is (Object.Handle /= C.Null_RF_Data);

   procedure Close (Object : in out Data_MEL) is
      D : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      R : aliased C.Size_T := 0;
   begin
      Check (C.RF_Data_Close (Object.Handle'Access, D'Address, D'Length, R'Access), D);
   end Close;

   overriding
   procedure Finalize (Object : in out Data_MEL) is
      Ignored : Interfaces.Integer_32;
   begin
      Ignored := C.RF_Data_Close (Object.Handle'Access, System.Null_Address, 0, null);
   exception
      when others =>
         Object.Handle := C.Null_RF_Data;
   end Finalize;

   function Query_Provider_Version (Object : Data_MEL) return AMS.MEL.Provider_Version is
      Raw    : aliased C.Provider_Version_V1 :=
        (API_Version          => 0,
         Library_Version      => 0,
         Vendor               => System.Null_Address,
         Vendor_Capacity      => 0,
         Vendor_Required      => 0,
         Description          => System.Null_Address,
         Description_Capacity => 0,
         Description_Required => 0);
      D      : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      R      : aliased C.Size_T := 0;
      Status : Interfaces.Integer_32;
   begin
      if not Is_Open (Object) then
         raise Provider_Error with "RF DataMEL is closed";
      end if;
      Status :=
        C.RF_Data_Get_Provider_Version (Object.Handle, Raw'Access, D'Address, D'Length, R'Access);
      if Status /= C.Buffer_Too_Small then
         raise Provider_Error with Message (D);
      end if;
      if Raw.Vendor_Required = 0 or else Raw.Description_Required = 0 then
         raise Provider_Error with "invalid native RF provider version lengths";
      end if;
      if Interfaces.Unsigned_64 (Raw.Vendor_Required) > Interfaces.Unsigned_64 (Natural'Last)
        or else Interfaces.Unsigned_64 (Raw.Description_Required)
                > Interfaces.Unsigned_64 (Natural'Last)
      then
         raise Provider_Error with "RF provider version strings are too large";
      end if;
      declare
         Vendor      : aliased Interfaces.C.char_array (0 .. Raw.Vendor_Required - 1) :=
           [others => Interfaces.C.nul];
         Description : aliased Interfaces.C.char_array (0 .. Raw.Description_Required - 1) :=
           [others => Interfaces.C.nul];
      begin
         Raw.Vendor := Vendor'Address;
         Raw.Vendor_Capacity := Vendor'Length;
         Raw.Description := Description'Address;
         Raw.Description_Capacity := Description'Length;
         Check
           (C.RF_Data_Get_Provider_Version
              (Object.Handle, Raw'Access, D'Address, D'Length, R'Access),
            D);
         return
           (API_Value         => AMS.MEL.Provider_Version_Number (Raw.API_Version),
            Library_Value     => AMS.MEL.Provider_Version_Number (Raw.Library_Version),
            Vendor_Value      => US.To_Unbounded_String (Interfaces.C.To_Ada (Vendor)),
            Description_Value => US.To_Unbounded_String (Interfaces.C.To_Ada (Description)));
      end;
   end Query_Provider_Version;

   function Checked_Count (Span : C.Span_V1) return Natural is
   begin
      if Interfaces.Unsigned_64 (Span.Size) > Interfaces.Unsigned_64 (Natural'Last)
        or else Interfaces.Unsigned_64 (Span.Size)
                > Interfaces.Unsigned_64 (Ada.Containers.Count_Type'Last)
        or else (Span.Size > 0 and then Span.Data = System.Null_Address)
      then
         raise Provider_Error with "invalid native RF span";
      end if;
      return Natural (Span.Size);
   end Checked_Count;

   function Binary (Value : Interfaces.Unsigned_32) return Boolean is
   begin
      if Value > 1 then
         raise Provider_Error with "invalid native RF boolean";
      end if;
      return Value = 1;
   end Binary;

   type MFA_Access is access all C.RF_MFA_Info_V1;
   function To_MFA is new Ada.Unchecked_Conversion (System.Address, MFA_Access);

   procedure Copy_Ranges (Span : C.Span_V1; Into : in out Range_Vectors.Vector) is
      Count : constant Natural := Checked_Count (Span);
      type Raw_Array is array (Natural range <>) of aliased C.RF_Frequency_Range_V1
      with Convention => C;
   begin
      if Count > 0 then
         declare
            Raw : Raw_Array (0 .. Count - 1)
            with Import, Address => Span.Data;
         begin
            for Item of Raw loop
               Into.Append (Frequency_Range'(Long_Float (Item.Min_Hz), Long_Float (Item.Max_Hz)));
            end loop;
         end;
      end if;
   end Copy_Ranges;

   function Copy_Face (Raw : C.RF_Face_Info_V1) return Face_Info is
      Result : Face_Info;
   begin
      Result.ID := Raw.Face_ID;
      Result.Receive_OK := Binary (Raw.Supports_Receive);
      Result.Transmit_OK := Binary (Raw.Supports_Transmit);
      Result.Association := Binary (Raw.Requires_Endpoint_Association);
      Result.AGC := Raw.AGC_Processing_Time_FS;
      Result.Min_Request := Raw.Min_Job_Request_Lead_Time_FS;
      Result.Max_Request := Raw.Max_Job_Request_Lead_Time_FS;
      Result.Min_Detail := Raw.Min_Job_Detail_Lead_Time_FS;
      Result.Tx_Rx := Raw.Tx_Rx_Switching_Time_FS;
      Result.Rx_Tx := Raw.Rx_Tx_Switching_Time_FS;
      Result.Tx_Tx := Raw.Tx_Tx_Switching_Time_FS;
      Result.Rx_Rx := Raw.Rx_Rx_Switching_Time_FS;
      Copy_Ranges (Raw.Rx_Frequency_Ranges, Result.Rx_Ranges);
      Copy_Ranges (Raw.Tx_Frequency_Ranges, Result.Tx_Ranges);
      Copy_Ranges (Raw.Sample_Frequency_Ranges, Result.Sample_Ranges);
      return Result;
   end Copy_Face;

   function Copy_MFA (Raw : C.RF_MFA_Info_V1) return MFA_Info is
      Result       : MFA_Info;
      Format_Count : constant Natural := Checked_Count (Raw.Supported_Data_Formats);
      Face_Total   : constant Natural := Checked_Count (Raw.Faces);
      type Format_Array is array (Natural range <>) of aliased Interfaces.Unsigned_32
      with Convention => C;
      type Face_Array is array (Natural range <>) of aliased C.RF_Face_Info_V1 with Convention => C;
   begin
      Result.Reported := Raw.Reported_Num_Faces;
      Result.Open_Additions := Binary (Raw.Contains_Open_Additions);
      Result.Resolution := Raw.Scheduler_Resolution_FS;
      Result.Context_Bytes := Raw.Max_User_Defined_Context_Bytes;
      if Format_Count > 0 then
         declare
            Formats : Format_Array (0 .. Format_Count - 1)
            with Import, Address => Raw.Supported_Data_Formats.Data;
         begin
            for Item of Formats loop
               Result.Formats.Append (Job_Data_Format (Item));
            end loop;
         end;
      end if;
      if Face_Total > 0 then
         declare
            Faces : Face_Array (0 .. Face_Total - 1)
            with Import, Address => Raw.Faces.Data;
         begin
            for Item of Faces loop
               Result.Faces.Append (Copy_Face (Item));
            end loop;
         end;
      end if;
      return Result;
   end Copy_MFA;

   function Snapshot_MFA_Info (Object : Data_MEL) return MFA_Info is
      Owner   : aliased C.RF_MFA_Info_Handle := C.Null_RF_MFA_Info;
      Address : aliased System.Address := System.Null_Address;
      D       : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      R       : aliased C.Size_T := 0;
      procedure Release is
         Ignored : Interfaces.Integer_32;
      begin
         Ignored := C.RF_MFA_Info_Close (Owner'Access, System.Null_Address, 0, null);
      end Release;
   begin
      Check
        (C.RF_Data_Get_MFA_Info (Object.Handle, Owner'Access, D'Address, D'Length, R'Access), D);
      begin
         Check (C.RF_MFA_Info_View (Owner, Address'Access, D'Address, D'Length, R'Access), D);
         if Address = System.Null_Address then
            raise Provider_Error with "null native RF MFA view";
         end if;
         declare
            Result : constant MFA_Info := Copy_MFA (To_MFA (Address).all);
         begin
            Release;
            return Result;
         end;
      exception
         when others =>
            Release;
            raise;
      end;
   end Snapshot_MFA_Info;

   function Face_ID (Value : Face_Info) return Interfaces.Unsigned_32
   is (Value.ID);
   function Supports_Receive (Value : Face_Info) return Boolean
   is (Value.Receive_OK);
   function Supports_Transmit (Value : Face_Info) return Boolean
   is (Value.Transmit_OK);
   function Requires_Endpoint_Association (Value : Face_Info) return Boolean
   is (Value.Association);
   function AGC_Processing_Time_FS (Value : Face_Info) return Interfaces.Integer_64
   is (Value.AGC);
   function Min_Job_Request_Lead_Time_FS (Value : Face_Info) return Interfaces.Integer_64
   is (Value.Min_Request);
   function Max_Job_Request_Lead_Time_FS (Value : Face_Info) return Interfaces.Integer_64
   is (Value.Max_Request);
   function Min_Job_Detail_Lead_Time_FS (Value : Face_Info) return Interfaces.Integer_64
   is (Value.Min_Detail);
   function Tx_Rx_Switching_Time_FS (Value : Face_Info) return Interfaces.Integer_64
   is (Value.Tx_Rx);
   function Rx_Tx_Switching_Time_FS (Value : Face_Info) return Interfaces.Integer_64
   is (Value.Rx_Tx);
   function Tx_Tx_Switching_Time_FS (Value : Face_Info) return Interfaces.Integer_64
   is (Value.Tx_Tx);
   function Rx_Rx_Switching_Time_FS (Value : Face_Info) return Interfaces.Integer_64
   is (Value.Rx_Rx);
   function Rx_Frequency_Range_Count (Value : Face_Info) return Natural
   is (Natural (Value.Rx_Ranges.Length));
   function Rx_Frequency_Range_At (Value : Face_Info; Index : Positive) return Frequency_Range
   is (Value.Rx_Ranges (Index));
   function Tx_Frequency_Range_Count (Value : Face_Info) return Natural
   is (Natural (Value.Tx_Ranges.Length));
   function Tx_Frequency_Range_At (Value : Face_Info; Index : Positive) return Frequency_Range
   is (Value.Tx_Ranges (Index));
   function Sample_Frequency_Range_Count (Value : Face_Info) return Natural
   is (Natural (Value.Sample_Ranges.Length));
   function Sample_Frequency_Range_At (Value : Face_Info; Index : Positive) return Frequency_Range
   is (Value.Sample_Ranges (Index));
   function Reported_Num_Faces (Value : MFA_Info) return Interfaces.Unsigned_64
   is (Value.Reported);
   function Contains_Open_Additions (Value : MFA_Info) return Boolean
   is (Value.Open_Additions);
   function Scheduler_Resolution_FS (Value : MFA_Info) return Interfaces.Integer_64
   is (Value.Resolution);
   function Max_User_Defined_Context_Bytes (Value : MFA_Info) return Interfaces.Unsigned_64
   is (Value.Context_Bytes);
   function Supported_Data_Format_Count (Value : MFA_Info) return Natural
   is (Natural (Value.Formats.Length));
   function Supported_Data_Format_At (Value : MFA_Info; Index : Positive) return Job_Data_Format
   is (Value.Formats (Index));
   function Face_Count (Value : MFA_Info) return Natural
   is (Natural (Value.Faces.Length));
   function Face_At (Value : MFA_Info; Index : Positive) return Face_Info
   is (Value.Faces (Index));
end AMS.MEL.RF;
