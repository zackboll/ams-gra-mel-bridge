with Ada.Unchecked_Conversion;
with Interfaces.C;
with System;
with System.Storage_Elements;

package body AMS.MEL.IR.C2.Common is
   package C renames AMS.MEL_C_API;
   package V renames IR.Channel;
   use type Interfaces.Integer_32;
   use type Interfaces.C.char;
   use type Interfaces.C.size_t;
   use type Interfaces.Unsigned_32;
   use System.Storage_Elements;
   type Diagnostic_Array is array (C.Size_T range <>) of aliased Interfaces.C.char
   with Convention => C;
   subtype Diagnostic is Diagnostic_Array (0 .. 1023);
   function Message (D : Diagnostic_Array) return String is
      N : Natural := 0;
   begin
      while N < D'Length and then D (C.Size_T (N)) /= Interfaces.C.nul loop
         N := N + 1;
      end loop;
      declare
         S : String (1 .. N);
      begin
         for I in S'Range loop
            S (I) := Character'Val (Interfaces.C.char'Pos (D (C.Size_T (I - 1))));
         end loop;
         return (if N = 0 then "native common Channel operation failed" else S);
      end;
   end Message;
   function Address_At (Base : System.Address; Index, Bytes : Natural) return System.Address
   is (Base + Storage_Offset (Index * Bytes));
   type U32_Access is access all Interfaces.Unsigned_32;
   type Cap_Access is access all C.IR_Channel_Capability_V1;
   type Band_Access is access all C.IR_Image_Band_V1;
   type Info_Access is access all C.IR_Band_Info_V1;
   type Char_Access is access all Interfaces.C.char;
   function To_U32 is new Ada.Unchecked_Conversion (System.Address, U32_Access);
   function To_Cap is new Ada.Unchecked_Conversion (System.Address, Cap_Access);
   function To_Band is new Ada.Unchecked_Conversion (System.Address, Band_Access);
   function To_Info is new Ada.Unchecked_Conversion (System.Address, Info_Access);
   function To_Char is new Ada.Unchecked_Conversion (System.Address, Char_Access);
   function Copy (S : C.String_View_V1) return String is
   begin
      if S.Size = 0 then
         return "";
      end if;
      declare
         R : String (1 .. Natural (S.Size));
      begin
         for I in R'Range loop
            R (I) :=
              Character'Val (Interfaces.C.char'Pos (To_Char (Address_At (S.Data, I - 1, 1)).all));
         end loop;
         return R;
      end;
   end Copy;
   function Copy (ID : C.UCI_ID_V1) return UCI_ID is
      B : UUID;
   begin
      for I in B'Range loop
         B (I) := ID.UUID (I);
      end loop;
      return Create_UCI_ID (B, Copy (ID.Descriptive_Label));
   end Copy;

   function Send_Keep_Alive (Channel : Control_Channel) return Return_Request is
      D : aliased Diagnostic := [others => Interfaces.C.nul];
      R : aliased C.Size_T := 0;
   begin
      return Result : Return_Request do
         if C.IR_C2_Send_Keepalive
              (Channel.Handle, Result.Owner.Handle'Access, D'Address, D'Length, R'Access)
           /= C.Success
         then
            raise Provider_Error with Message (D);
         end if;
      end return;
   end Send_Keep_Alive;
   function Submit_Comms_Test
     (Channel    : Control_Channel;
      Channel_ID : V.Comms_Channel_ID;
      Command_ID : C2.Command_ID;
      Request_ID : V.Comms_Request_ID) return Comms_Request
   is
      Raw : aliased C.IR_Channel_Comms_Test_Request_V1 :=
        (Interfaces.Unsigned_32 (Command_ID),
         Interfaces.Unsigned_32 (Channel_ID),
         Interfaces.Unsigned_32 (Request_ID));
      D   : aliased Diagnostic := [others => Interfaces.C.nul];
      R   : aliased C.Size_T := 0;
   begin
      return Result : Comms_Request do
         if C.IR_C2_Submit_Comms_Test
              (Channel.Handle,
               Raw'Access,
               Result.Owner.Handle'Access,
               D'Address,
               D'Length,
               R'Access)
           /= C.Success
         then
            raise Provider_Error with Message (D);
         end if;
      end return;
   end Submit_Comms_Test;
   function Wait (Request : Comms_Request; Timeout_Milliseconds : Natural) return Comms_Result is
      Raw  : aliased C.IR_Channel_Comms_Test_Result_V1 := (0, 0, 0);
      D    : aliased Diagnostic := [others => Interfaces.C.nul];
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
      if Code = C.Success then
         return
           (Success,
            (C2.Command_ID (Raw.Command_ID), V.Comms_Request_ID (Raw.Request_ID)),
            None,
            US.Null_Unbounded_String);
      elsif Code = C.Timeout then
         raise Timeout_Error;
      elsif Code = C.Command_Rejected then
         if R > D'Length then
            declare
               Complete       : aliased Diagnostic_Array (0 .. R - 1) :=
                 [others => Interfaces.C.nul];
               Retry_Raw      : aliased C.IR_Channel_Comms_Test_Result_V1 := (0, 0, 0);
               Retry_Required : aliased C.Size_T := 0;
               Retry          : constant Interfaces.Integer_32 :=
                 C.IR_Comms_Request_Wait
                   (Request.Owner.Handle,
                    0,
                    Retry_Raw'Access,
                    Complete'Address,
                    Complete'Length,
                    Retry_Required'Access);
            begin
               if Retry /= C.Command_Rejected
                 or else Retry_Raw.Error_Code /= Raw.Error_Code
                 or else Retry_Required /= R
               then
                  raise Provider_Error
                    with "native CommsTest rejection changed during diagnostic retry";
               end if;
               return
                 (Rejected,
                  (0, 0),
                  Error_Code'Val (Raw.Error_Code),
                  US.To_Unbounded_String (Message (Complete)));
            end;
         end if;
         return
           (Rejected,
            (0, 0),
            Error_Code'Val (Raw.Error_Code),
            US.To_Unbounded_String (Message (D)));
      else
         raise Provider_Error with Message (D);
      end if;
   end Wait;
   function Status (Result : Comms_Result) return Outcome
   is (Result.Result_Status);
   function Report (Result : Comms_Result) return V.Comms_Test_Report
   is (Result.Result_Report);
   function Rejection_Code (Result : Comms_Result) return Error_Code
   is (Result.Result_Code);
   function Description (Result : Comms_Result) return String
   is (US.To_String (Result.Result_Text));
   procedure Close (Request : in out Comms_Request) is
      D : aliased Diagnostic := [others => Interfaces.C.nul];
      R : aliased C.Size_T := 0;
   begin
      if C.IR_Comms_Request_Close (Request.Owner.Handle'Access, D'Address, D'Length, R'Access)
        /= C.Success
      then
         raise Provider_Error with Message (D);
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

   function Capabilities (Channel : Control_Channel) return V.Channel_Capability is
      Owner   : aliased C.Capability_Handle := C.Null_Capability;
      Address : aliased System.Address := System.Null_Address;
      D       : aliased Diagnostic := [others => Interfaces.C.nul];
      R       : aliased C.Size_T := 0;
      procedure Release is
         Ignored : Interfaces.Integer_32;
      begin
         Ignored := C.IR_Capability_Close (Owner'Access, System.Null_Address, 0, null);
      end Release;
   begin
      if C.IR_C2_Get_Capabilities (Channel.Handle, Owner'Access, D'Address, D'Length, R'Access)
        /= C.Success
      then
         raise Provider_Error with Message (D);
      end if;
      if C.IR_Capability_View (Owner, Address'Access, D'Address, D'Length, R'Access) /= C.Success
      then
         Release;
         raise Provider_Error with Message (D);
      end if;
      declare
         Raw      : constant Cap_Access := To_Cap (Address);
         Sensors  : V.Sensor_Type_Vectors.Vector;
         Channels : V.Channel_Type_Vectors.Vector;
         Metadata : V.Metadata_Vectors.Vector;
         Bands    : V.Image_Band_Vectors.Vector;
         Frames   : V.Coordinate_Vectors.Vector;
      begin
         if Raw.Sensor_Types.Size > 0 then
            for I in 0 .. Natural (Raw.Sensor_Types.Size) - 1 loop
               Sensors.Append
                 (V.Sensor_Type'Val (To_U32 (Address_At (Raw.Sensor_Types.Data, I, 4)).all));
            end loop;
         end if;
         if Raw.Channel_Types.Size > 0 then
            for I in 0 .. Natural (Raw.Channel_Types.Size) - 1 loop
               Channels.Append
                 (V.Channel_Type'Val (To_U32 (Address_At (Raw.Channel_Types.Data, I, 4)).all));
            end loop;
         end if;
         if Raw.Metadata_Capabilities.Size > 0 then
            for I in 0 .. Natural (Raw.Metadata_Capabilities.Size) - 1 loop
               Metadata.Append
                 (V.Metadata_Capability'Val
                    (To_U32 (Address_At (Raw.Metadata_Capabilities.Data, I, 4)).all));
            end loop;
         end if;
         if Raw.Image_Bands.Size > 0 then
            for I in 0 .. Natural (Raw.Image_Bands.Size) - 1 loop
               declare
                  B     : constant Band_Access :=
                    To_Band (Address_At (Raw.Image_Bands.Data, I, C.IR_Image_Band_V1'Size / 8));
                  Infos : V.Band_Info_Vectors.Vector;
               begin
                  if B.Bands.Size > 0 then
                     for J in 0 .. Natural (B.Bands.Size) - 1 loop
                        declare
                           X : constant Info_Access :=
                             To_Info (Address_At (B.Bands.Data, J, C.IR_Band_Info_V1'Size / 8));
                        begin
                           Infos.Append
                             (V.Band_Info'
                                (V.Band_Type'Val (X.Kind),
                                 Long_Float (X.Min_Wavelength_M),
                                 Long_Float (X.Max_Wavelength_M)));
                        end;
                     end loop;
                  end if;
                  Bands.Append (V.Image_Band'(B.Band_Index, Infos));
               end;
            end loop;
         end if;
         if Raw.Nav_Frames.Size > 0 then
            for I in 0 .. Natural (Raw.Nav_Frames.Size) - 1 loop
               Frames.Append
                 (V.Coordinate_System_Type'Val
                    (To_U32 (Address_At (Raw.Nav_Frames.Data, I, 4)).all));
            end loop;
         end if;
         declare
            Result : constant V.Channel_Capability :=
              V.Create_Capability
                (Copy (Raw.Channel_ID),
                 Raw.Height,
                 Raw.Width,
                 Raw.Bit_Depth,
                 Raw.Row_Pitch,
                 Raw.Buffer_Size,
                 Raw.Image_Size,
                 Raw.Number_Of_Bands,
                 V.Pixel_Format'Val (Raw.Pixel_Format),
                 Sensors,
                 Copy (Raw.Platform_ID),
                 Create_Component_Location
                   (Long_Float (Raw.Sensor_Location.Offset_X_M),
                    Long_Float (Raw.Sensor_Location.Offset_Y_M),
                    Long_Float (Raw.Sensor_Location.Offset_Z_M),
                    Copy (Raw.Sensor_Location.Key),
                    Copy (Raw.Sensor_Location.System_Name)),
                 Channels,
                 Raw.Task_Schedule_Depth,
                 Raw.ODC_Available = 1,
                 Raw.NUC_Available = 1,
                 Metadata,
                 Bands,
                 Frames);
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
end AMS.MEL.IR.C2.Common;
