with Ada.Unchecked_Conversion;
with AMS.MEL.IR.Capability_Conversion;
with Interfaces.C;
with System;

package body AMS.MEL.IR.Channel is
   package C renames AMS.MEL_C_API;
   use type Interfaces.Integer_32;
   use type Interfaces.Unsigned_32;
   use type Interfaces.C.char;
   use type Interfaces.C.size_t;
   use type C.Channel_Handle;
   use type C.Return_Request_Handle;
   use type C.Comms_Request_Handle;

   type Diagnostic_Array is array (C.Size_T range <>) of aliased Interfaces.C.char
   with Convention => C;
   subtype Fixed_Diagnostic is Diagnostic_Array (0 .. 511);

   function Message (Buffer : Diagnostic_Array) return String is
      Length : Natural := 0;
   begin
      while Length < Buffer'Length
        and then Buffer (Buffer'First + C.Size_T (Length)) /= Interfaces.C.nul
      loop
         Length := Length + 1;
      end loop;
      declare
         Result : String (1 .. Length);
      begin
         for Index in Result'Range loop
            Result (Index) :=
              Character'Val (Interfaces.C.char'Pos (Buffer (Buffer'First + C.Size_T (Index - 1))));
         end loop;
         return Result;
      end;
   end Message;

   function Failure_Message (Buffer : Diagnostic_Array) return String is
      Value : constant String := Message (Buffer);
   begin
      return (if Value'Length = 0 then "native common Channel operation failed" else Value);
   end Failure_Message;

   type Cap_Access is access all C.IR_Channel_Capability_V1;
   function To_Cap is new Ada.Unchecked_Conversion (System.Address, Cap_Access);

   function Is_Open (Channel : View) return Boolean
   is (Channel.Handle /= C.Null_Channel);

   procedure Close (Channel : in out View) is
      D    : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      R    : aliased C.Size_T := 0;
      Code : constant Interfaces.Integer_32 :=
        C.IR_Channel_Close (Channel.Handle'Access, D'Address, D'Length, R'Access);
   begin
      if Code /= C.Success then
         raise Provider_Error with Failure_Message (D);
      end if;
   end Close;

   procedure Require_Open (Channel : View) is
   begin
      if Channel.Handle = C.Null_Channel then
         raise Provider_Error with "common Channel view is closed";
      end if;
   end Require_Open;

   function Send_Keep_Alive (Channel : View) return Return_Request is
      D : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      R : aliased C.Size_T := 0;
   begin
      Require_Open (Channel);
      return Result : Return_Request do
         declare
            Code : constant Interfaces.Integer_32 :=
              C.IR_Channel_Send_Keepalive
                (Channel.Handle, Result.Owner.Handle'Access, D'Address, D'Length, R'Access);
         begin
            Check_Submission (Code, Failure_Message (D));
         end;
      end return;
   end Send_Keep_Alive;

   function Is_Open (Request : Return_Request) return Boolean
   is (Request.Owner.Handle /= C.Null_Return_Request);

   function Status (Result : Return_Result) return Outcome
   is (Result.Result_Status);
   function Value (Result : Return_Result) return Command_Return
   is (Result.Result_Value);
   function Rejection_Code (Result : Return_Result) return Error_Code
   is (Result.Result_Code);
   function Description (Result : Return_Result) return String
   is (US.To_String (Result.Result_Text));

   procedure Close (Request : in out Return_Request) is
      D    : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      R    : aliased C.Size_T := 0;
      Code : constant Interfaces.Integer_32 :=
        C.IR_Return_Request_Close (Request.Owner.Handle'Access, D'Address, D'Length, R'Access);
   begin
      if Code /= C.Success then
         raise Provider_Error with Failure_Message (D);
      end if;
   end Close;

   overriding
   procedure Finalize (Request : in out Return_Owner) is
      Ignored : Interfaces.Integer_32;
   begin
      Ignored := C.IR_Return_Request_Close (Request.Handle'Access, System.Null_Address, 0, null);
   exception
      when others =>
         Request.Handle := C.Null_Return_Request;
   end Finalize;

   function Wait (Request : Return_Request; Timeout_Milliseconds : Natural) return Return_Result is
      Raw  : aliased C.IR_Return_Result_V1 := (Value => 0, Error_Code => 0);
      D    : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      R    : aliased C.Size_T := 0;
      Code : constant Interfaces.Integer_32 :=
        C.IR_Return_Request_Wait
          (Request.Owner.Handle,
           Interfaces.Unsigned_32 (Timeout_Milliseconds),
           Raw'Access,
           D'Address,
           D'Length,
           R'Access);
   begin
      if Code = C.Timeout then
         raise Timeout_Error with "common Channel KeepAlive request timed out";
      elsif Code = C.Success then
         if Raw.Value > Command_Return'Pos (Command_Return'Last) then
            raise Provider_Error with "native common Channel returned unknown IR Return value";
         end if;
         return
           (Result_Status => Success,
            Result_Value  => Command_Return'Val (Raw.Value),
            Result_Code   => None,
            Result_Text   => US.Null_Unbounded_String);
      elsif Code = C.Command_Rejected then
         if Raw.Error_Code > Error_Code'Pos (Error_Code'Last) then
            raise Provider_Error with "native common Channel returned unknown MEL error code";
         end if;
         declare
            Text : US.Unbounded_String := US.To_Unbounded_String (Message (D));
         begin
            if R > D'Length then
               --  Allocate exactly the required diagnostic, repeat the cached
               --  Wait, and fail closed if the cached rejection changed.
               declare
                  Complete       : aliased Diagnostic_Array (0 .. R - 1) :=
                    [others => Interfaces.C.nul];
                  Retry_Raw      : aliased C.IR_Return_Result_V1 := (Value => 0, Error_Code => 0);
                  Retry_Required : aliased C.Size_T := 0;
                  Retry_Code     : constant Interfaces.Integer_32 :=
                    C.IR_Return_Request_Wait
                      (Request.Owner.Handle,
                       0,
                       Retry_Raw'Access,
                       Complete'Address,
                       Complete'Length,
                       Retry_Required'Access);
               begin
                  if Retry_Code /= C.Command_Rejected
                    or else Retry_Raw.Error_Code /= Raw.Error_Code
                    or else Retry_Required /= R
                  then
                     raise Provider_Error
                       with "native KeepAlive rejection changed during diagnostic retry";
                  end if;
                  Text := US.To_Unbounded_String (Message (Complete));
               end;
            end if;
            return
              (Result_Status => Rejected,
               Result_Value  => Return_Success,
               Result_Code   => Error_Code'Val (Raw.Error_Code),
               Result_Text   => Text);
         end;
      else
         raise Provider_Error with Failure_Message (D);
      end if;
   end Wait;

   function Submit_Comms_Test
     (Channel    : View;
      Channel_ID : Comms_Channel_ID;
      Command_ID : AMS.MEL.IR.Channel.Command_ID;
      Request_ID : Comms_Request_ID) return Comms_Request
   is
      Raw : aliased C.IR_Channel_Comms_Test_Request_V1 :=
        (Command_ID => Interfaces.Unsigned_32 (Command_ID),
         Channel_ID => Interfaces.Unsigned_32 (Channel_ID),
         Request_ID => Interfaces.Unsigned_32 (Request_ID));
      D   : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      R   : aliased C.Size_T := 0;
   begin
      Require_Open (Channel);
      return Result : Comms_Request do
         declare
            Code : constant Interfaces.Integer_32 :=
              C.IR_Channel_Submit_Comms_Test
                (Channel.Handle,
                 Raw'Access,
                 Result.Owner.Handle'Access,
                 D'Address,
                 D'Length,
                 R'Access);
         begin
            Check_Submission (Code, Failure_Message (D));
         end;
      end return;
   end Submit_Comms_Test;

   function Is_Open (Request : Comms_Request) return Boolean
   is (Request.Owner.Handle /= C.Null_Comms_Request);

   function Wait (Request : Comms_Request; Timeout_Milliseconds : Natural) return Comms_Result is
      Raw  : aliased C.IR_Channel_Comms_Test_Result_V1 := (0, 0, 0);
      D    : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      R    : aliased C.Size_T := 0;
      Code : constant Interfaces.Integer_32 :=
        C.IR_Comms_Request_Wait
          (Request.Owner.Handle,
           Interfaces.Unsigned_32 (Timeout_Milliseconds),
           Raw'Access,
           D'Address,
           D'Length,
           R'Access);
   begin
      if Code = C.Timeout then
         raise Timeout_Error with "common Channel CommsTest request timed out";
      elsif Code = C.Success then
         return
           (Result_Status => Success,
            Result_Report =>
              (Command_ID => AMS.MEL.IR.Channel.Command_ID (Raw.Command_ID),
               Request_ID => Comms_Request_ID (Raw.Request_ID)),
            Result_Code   => None,
            Result_Text   => US.Null_Unbounded_String);
      elsif Code = C.Command_Rejected then
         if Raw.Error_Code > Error_Code'Pos (Error_Code'Last) then
            raise Provider_Error with "native common Channel returned unknown MEL error code";
         end if;
         declare
            Text : US.Unbounded_String := US.To_Unbounded_String (Message (D));
         begin
            if R > D'Length then
               declare
                  Complete       : aliased Diagnostic_Array (0 .. R - 1) :=
                    [others => Interfaces.C.nul];
                  Retry_Raw      : aliased C.IR_Channel_Comms_Test_Result_V1 := (0, 0, 0);
                  Retry_Required : aliased C.Size_T := 0;
                  Retry_Code     : constant Interfaces.Integer_32 :=
                    C.IR_Comms_Request_Wait
                      (Request.Owner.Handle,
                       0,
                       Retry_Raw'Access,
                       Complete'Address,
                       Complete'Length,
                       Retry_Required'Access);
               begin
                  if Retry_Code /= C.Command_Rejected
                    or else Retry_Raw.Error_Code /= Raw.Error_Code
                    or else Retry_Required /= R
                  then
                     raise Provider_Error
                       with "native CommsTest rejection changed during diagnostic retry";
                  end if;
                  Text := US.To_Unbounded_String (Message (Complete));
               end;
            end if;
            return
              (Result_Status => Rejected,
               Result_Report => (0, 0),
               Result_Code   => Error_Code'Val (Raw.Error_Code),
               Result_Text   => Text);
         end;
      else
         raise Provider_Error with Failure_Message (D);
      end if;
   end Wait;

   function Status (Result : Comms_Result) return Outcome
   is (Result.Result_Status);
   function Report (Result : Comms_Result) return Comms_Test_Report
   is (Result.Result_Report);
   function Rejection_Code (Result : Comms_Result) return Error_Code
   is (Result.Result_Code);
   function Description (Result : Comms_Result) return String
   is (US.To_String (Result.Result_Text));

   procedure Close (Request : in out Comms_Request) is
      D    : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      R    : aliased C.Size_T := 0;
      Code : constant Interfaces.Integer_32 :=
        C.IR_Comms_Request_Close (Request.Owner.Handle'Access, D'Address, D'Length, R'Access);
   begin
      if Code /= C.Success then
         raise Provider_Error with Failure_Message (D);
      end if;
   end Close;

   overriding
   procedure Finalize (Request : in out Comms_Owner) is
      Ignored : Interfaces.Integer_32;
   begin
      Ignored := C.IR_Comms_Request_Close (Request.Handle'Access, System.Null_Address, 0, null);
   exception
      when others =>
         Request.Handle := C.Null_Comms_Request;
   end Finalize;

   function Capabilities (Channel : View) return Channel_Capability is
      Owner   : aliased C.Capability_Handle := C.Null_Capability;
      Address : aliased System.Address := System.Null_Address;
      D       : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      R       : aliased C.Size_T := 0;
      procedure Release is
         Ignored : Interfaces.Integer_32;
      begin
         Ignored := C.IR_Capability_Close (Owner'Access, System.Null_Address, 0, null);
      end Release;
   begin
      Require_Open (Channel);
      if C.IR_Channel_Get_Capabilities (Channel.Handle, Owner'Access, D'Address, D'Length, R'Access)
        /= C.Success
      then
         Release;
         raise Provider_Error with Failure_Message (D);
      end if;
      if C.IR_Capability_View (Owner, Address'Access, D'Address, D'Length, R'Access) /= C.Success
      then
         Release;
         raise Provider_Error with Failure_Message (D);
      end if;
      begin
         declare
            Result : constant Channel_Capability :=
              Capability_Conversion.To_Channel_Capability (To_Cap (Address).all);
         begin
            Release;
            return Result;
         end;
      exception
         when others =>
            Release;
            raise;
      end;
   end Capabilities;

   function Create_Capability
     (Channel_ID                                                                    : UCI_ID;
      Height, Width, Bit_Depth, Row_Pitch, Buffer_Size, Image_Size, Number_Of_Bands :
        Interfaces.Unsigned_32;
      Format                                                                        : Pixel_Format;
      Sensor_Types                                                                  :
        Sensor_Type_Vectors.Vector;
      Platform_ID                                                                   : UCI_ID;
      Sensor_Location                                                               :
        Component_Location;
      Channel_Types                                                                 :
        Channel_Type_Vectors.Vector;
      Task_Schedule_Depth                                                           :
        Interfaces.Unsigned_32;
      ODC_Available, NUC_Available                                                  : Boolean;
      Metadata                                                                      :
        Metadata_Vectors.Vector;
      Image_Bands                                                                   :
        Image_Band_Vectors.Vector;
      Nav_Frames                                                                    :
        Coordinate_Vectors.Vector) return Channel_Capability
   is ((Channel_ID,
        Platform_ID,
        Height,
        Width,
        Bit_Depth,
        Row_Pitch,
        Buffer_Size,
        Image_Size,
        Number_Of_Bands,
        Task_Schedule_Depth,
        Format,
        Sensor_Types,
        Sensor_Location,
        Channel_Types,
        ODC_Available,
        NUC_Available,
        Metadata,
        Image_Bands,
        Nav_Frames));
   function Channel_ID (Value : Channel_Capability) return UCI_ID
   is (Value.ID);
   function Height (Value : Channel_Capability) return Interfaces.Unsigned_32
   is (Value.H);
   function Width (Value : Channel_Capability) return Interfaces.Unsigned_32
   is (Value.W);
   function Bit_Depth (Value : Channel_Capability) return Interfaces.Unsigned_32
   is (Value.Depth);
   function Row_Pitch (Value : Channel_Capability) return Interfaces.Unsigned_32
   is (Value.Pitch);
   function Buffer_Size (Value : Channel_Capability) return Interfaces.Unsigned_32
   is (Value.Buffer);
   function Image_Size (Value : Channel_Capability) return Interfaces.Unsigned_32
   is (Value.Image);
   function Number_Of_Bands (Value : Channel_Capability) return Interfaces.Unsigned_32
   is (Value.Band_Count);
   function Format (Value : Channel_Capability) return Pixel_Format
   is (Value.Pixel);
   function Sensor_Type_Count (Value : Channel_Capability) return Natural
   is (Natural (Value.Sensors.Length));
   function Sensor_Type_At (Value : Channel_Capability; Index : Positive) return Sensor_Type
   is (Value.Sensors (Index));
   function Platform_ID (Value : Channel_Capability) return UCI_ID
   is (Value.Platform);
   function Sensor_Location (Value : Channel_Capability) return Component_Location
   is (Value.Location);
   function Channel_Type_Count (Value : Channel_Capability) return Natural
   is (Natural (Value.Channels.Length));
   function Channel_Type_At (Value : Channel_Capability; Index : Positive) return Channel_Type
   is (Value.Channels (Index));
   function Task_Schedule_Depth (Value : Channel_Capability) return Interfaces.Unsigned_32
   is (Value.Schedule);
   function ODC_Available (Value : Channel_Capability) return Boolean
   is (Value.ODC);
   function NUC_Available (Value : Channel_Capability) return Boolean
   is (Value.NUC);
   function Metadata_Capability_Count (Value : Channel_Capability) return Natural
   is (Natural (Value.Metadata.Length));
   function Metadata_Capability_At
     (Value : Channel_Capability; Index : Positive) return Metadata_Capability
   is (Value.Metadata (Index));
   function Has_Metadata_Capability
     (Value : Channel_Capability; Item : Metadata_Capability) return Boolean
   is (Value.Metadata.Contains (Item));
   function Image_Band_Count (Value : Channel_Capability) return Natural
   is (Natural (Value.Bands.Length));
   function Image_Band_Index_At
     (Value : Channel_Capability; Index : Positive) return Interfaces.Unsigned_32
   is (Value.Bands (Index).Band_Index);
   function Image_Band_Info_Count (Value : Channel_Capability; Index : Positive) return Natural
   is (Natural (Value.Bands (Index).Bands.Length));
   function Image_Band_Info_At
     (Value : Channel_Capability; Band_Index, Info_Index : Positive) return Band_Info
   is (Value.Bands (Band_Index).Bands (Info_Index));
   function Nav_Frame_Count (Value : Channel_Capability) return Natural
   is (Natural (Value.Frames.Length));
   function Nav_Frame_At
     (Value : Channel_Capability; Index : Positive) return Coordinate_System_Type
   is (Value.Frames (Index));
end AMS.MEL.IR.Channel;
