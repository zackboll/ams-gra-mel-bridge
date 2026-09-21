with Ada.Unchecked_Conversion;
with AMS.MEL.IR.Capability_Conversion;
with Interfaces.C;
with System.Storage_Elements;

package body AMS.MEL.IR.Image is
   package C renames AMS.MEL_C_API;
   use type Interfaces.Integer_32;
   use type Interfaces.Unsigned_32;
   use type Interfaces.C.size_t;
   use type Interfaces.C.char;
   use type C.Navigation_Request_Handle;
   use type C.Frame_Snapshot_Handle;
   use type System.Address;
   use System.Storage_Elements;
   type View_Access is access all C.IR_Frame_Snapshot_V1;
   type Capability_Access is access all C.IR_Channel_Capability_V1;
   type U32_Access is access all Interfaces.Unsigned_32;
   type Inertial_Access is access all C.IR_Sensor_Inertial_State_V1;
   type Nav_Access is access all C.IR_Sensor_Nav_State_V1;
   function To_View is new Ada.Unchecked_Conversion (System.Address, View_Access);
   function To_Capability is new Ada.Unchecked_Conversion (System.Address, Capability_Access);
   function To_U32 is new Ada.Unchecked_Conversion (System.Address, U32_Access);
   function To_Inertial is new Ada.Unchecked_Conversion (System.Address, Inertial_Access);
   function To_Nav is new Ada.Unchecked_Conversion (System.Address, Nav_Access);
   function Address_At (Base : System.Address; Index, Bytes : Natural) return System.Address
   is (Base + Storage_Offset (Index * Bytes));
   type Diagnostic is array (C.Size_T range 0 .. 511) of aliased Interfaces.C.char
   with Convention => C;
   function Message (Value : Diagnostic) return String is
      Last : Natural := 0;
   begin
      while Last < Value'Length and then Value (C.Size_T (Last)) /= Interfaces.C.nul loop
         Last := Last + 1;
      end loop;
      declare
         Result : String (1 .. Last);
      begin
         for I in Result'Range loop
            Result (I) := Character'Val (Interfaces.C.char'Pos (Value (C.Size_T (I - 1))));
         end loop;
         return (if Result'Length = 0 then "native full IR frame operation failed" else Result);
      end;
   end Message;
   procedure Check (Code : Interfaces.Integer_32; Buffer : Diagnostic) is
   begin
      if Code = C.Timeout then
         raise Timeout_Error;
      elsif Code = C.Stream_Stopped then
         raise Stream_Stopped;
      elsif Code /= C.Success then
         raise Provider_Error with Message (Buffer);
      end if;
   end Check;
   function Capabilities
     (Object : AMS.MEL.IR.Image_Stream) return AMS.MEL.IR.Channel.Channel_Capability
   is
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
      Check
        (C.IR_Stream_Get_Capabilities (Object.Handle, Owner'Access, D'Address, D'Length, R'Access),
         D);
      begin
         Check (C.IR_Capability_View (Owner, Address'Access, D'Address, D'Length, R'Access), D);
         declare
            Result : constant AMS.MEL.IR.Channel.Channel_Capability :=
              Capability_Conversion.To_Channel_Capability (To_Capability (Address).all);
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
   function Direction (Value : C.IR_Directional_V1) return Directional
   is ((Long_Float (Value.X), Long_Float (Value.Y), Long_Float (Value.Z)));
   function Quaternion_Value (Value : C.IR_Quaternion_V1) return Quaternion
   is ((Long_Float (Value.X), Long_Float (Value.Y), Long_Float (Value.Z), Long_Float (Value.W)));
   function Error_Value (Value : C.IR_Nav_Error_V1) return Nav_Error
   is ((Long_Float (Value.X), Long_Float (Value.Y), Long_Float (Value.Z), Long_Float (Value.W)));
   function Orientation_Value (Value : C.IR_Orientation_V1) return Orientation is
   begin
      if Value.Kind = 0 then
         return
           (Euler_Orientation,
            (Long_Float (Value.Euler.Roll),
             Long_Float (Value.Euler.Pitch),
             Long_Float (Value.Euler.Yaw)));
      elsif Value.Kind = 1 then
         return (Quaternion_Orientation, Quaternion_Value (Value.Quaternion));
      else
         raise Provider_Error with "invalid native orientation kind";
      end if;
   end Orientation_Value;
   function Copy_String (Value : C.String_View_V1) return String is
      type Char_Access is access all Interfaces.C.char;
      function To_Char is new Ada.Unchecked_Conversion (System.Address, Char_Access);
   begin
      if Value.Size = 0 then
         return "";
      end if;
      declare
         Result : String (1 .. Natural (Value.Size));
      begin
         for I in Result'Range loop
            Result (I) :=
              Character'Val
                (Interfaces.C.char'Pos (To_Char (Address_At (Value.Data, I - 1, 1)).all));
         end loop;
         return Result;
      end;
   end Copy_String;
   procedure Close (Handle : aliased in out C.Frame_Snapshot_Handle) is
      D       : aliased Diagnostic := [others => Interfaces.C.nul];
      R       : aliased C.Size_T := 0;
      Ignored : Interfaces.Integer_32;
   begin
      Ignored := C.IR_Frame_Snapshot_Close (Handle'Access, D'Address, D'Length, R'Access);
   end Close;
   --  Converts every small metadata field of a native frame snapshot view
   --  into Ada-owned storage and records the bulk pixel payload as a borrowed
   --  address/length pair. The payload itself is deliberately NOT copied here:
   --  this one conversion serves both the owned-copy Receive path, which adds
   --  its own explicit copy afterwards, and the borrowed Acquire_Frame path,
   --  which never copies the payload at all.
   function To_Lease_Metadata (Raw : C.IR_Frame_Snapshot_V1) return Lease_Metadata is
      Result : Lease_Metadata;
   begin
      if Raw.Pixel_Format > 2 or else Raw.Image_Type > 2 or else Raw.Image_Flip > 3 then
         raise Provider_Error with "invalid native full frame enum";
      end if;
      Result.Time := Long_Long_Integer (Raw.System_Time_NS);
      Result.Integration := Long_Long_Integer (Raw.Integration_Time_NS);
      Result.W := Raw.Width;
      Result.H := Raw.Height;
      Result.BPP := Raw.Bits_Per_Pixel;
      Result.Bands := Raw.Number_Of_Bands;
      Result.HFOV := Long_Float (Raw.Horizontal_FOV_Rad);
      Result.VFOV := Long_Float (Raw.Vertical_FOV_Rad);
      Result.Sensor :=
        (AMS.MEL.IR.Create_Component_Location
           (Long_Float (Raw.Contributing_Sensor.Location.Offset_X_M),
            Long_Float (Raw.Contributing_Sensor.Location.Offset_Y_M),
            Long_Float (Raw.Contributing_Sensor.Location.Offset_Z_M),
            Copy_String (Raw.Contributing_Sensor.Location.Key),
            Copy_String (Raw.Contributing_Sensor.Location.System_Name)),
         Raw.Contributing_Sensor.Sensor_ID);
      Result.Format := AMS.MEL.IR.Channel.Pixel_Format'Val (Raw.Pixel_Format);
      Result.ID := Raw.Frame_ID;
      Result.Sub_ID := Raw.Subframe_ID;
      Result.Sub_Total := Raw.Subframe_Total;
      Result.Kind := Full_Image_Type'Val (Raw.Image_Type);
      Result.Flip := AMS.MEL.IR.Image_Flip'Val (Raw.Image_Flip);
      if Raw.Image_Flags.Size > 0 then
         for I in 0 .. Natural (Raw.Image_Flags.Size) - 1 loop
            declare
               V : constant Interfaces.Unsigned_32 :=
                 To_U32 (Address_At (Raw.Image_Flags.Data, I, 4)).all;
            begin
               if V > 3 then
                  raise Provider_Error with "invalid native image flag";
               end if;
               Result.Flags.Append (Image_Flag'Val (V));
            end;
         end loop;
      end if;
      Result.D_Row := Long_Float (Raw.Dither_Row);
      Result.D_Column := Long_Float (Raw.Dither_Column);
      Result.Row := Raw.Row_Offset;
      Result.Column := Raw.Column_Offset;
      if Raw.Sensor_Inertial_States.Size > 0 then
         for I in 0 .. Natural (Raw.Sensor_Inertial_States.Size) - 1 loop
            declare
               V : constant C.IR_Sensor_Inertial_State_V1 :=
                 To_Inertial
                   (Address_At
                      (Raw.Sensor_Inertial_States.Data,
                       I,
                       C.IR_Sensor_Inertial_State_V1'Object_Size / System.Storage_Unit)).all;
            begin
               Result.Inertial.Append
                 (Sensor_Inertial_State'
                    (Long_Long_Integer (V.System_Time_NS),
                     Quaternion_Value (V.Q_XYZW),
                     Quaternion_Value (V.Q_ECEF_XYZW),
                     Direction (V.Sensor_Position),
                     Direction (V.Sensor_Velocity),
                     (V.Uncertainties.Sensor_Uncertainties,
                      V.Uncertainties.Platform_Uncertainties)));
            end;
         end loop;
      end if;
      if Raw.Sensor_Nav_States.Size > 0 then
         for I in 0 .. Natural (Raw.Sensor_Nav_States.Size) - 1 loop
            declare
               V : constant C.IR_Sensor_Nav_State_V1 :=
                 To_Nav
                   (Address_At
                      (Raw.Sensor_Nav_States.Data,
                       I,
                       C.IR_Sensor_Nav_State_V1'Object_Size / System.Storage_Unit)).all;
            begin
               if V.Coordinate_System > 3 then
                  raise Provider_Error with "invalid native coordinate system";
               end if;
               Result.Nav.Append
                 (Sensor_Nav_State'
                    (Direction (V.Position),
                     Direction (V.Velocity),
                     Direction (V.Acceleration),
                     Error_Value (V.Position_Error),
                     Error_Value (V.Velocity_Error),
                     Error_Value (V.Acceleration_Error),
                     Orientation_Value (V.Orientation),
                     Error_Value (V.Orientation_Error),
                     Orientation_Value (V.Orientation_Velocity),
                     Error_Value (V.Orientation_Velocity_Error),
                     Orientation_Value (V.Orientation_Acceleration),
                     Error_Value (V.Orientation_Acceleration_Error),
                     Coordinate_System'Val (V.Coordinate_System)));
            end;
         end loop;
      end if;
      Result.Band := Raw.Band_Index;
      --  Bulk payload: pointer and length only. Fail closed on a span that
      --  cannot be represented as a safe Ada array, or on a null pointer with
      --  a nonzero size, instead of performing unchecked memory access.
      if Raw.Pixels.Size > C.Size_T (Natural'Last) then
         raise Provider_Error with "native pixel span exceeds Ada index range";
      end if;
      Result.Payload_Size := Natural (Raw.Pixels.Size);
      Result.Payload := Raw.Pixels.Data;
      if Result.Payload_Size > 0 and then Result.Payload = System.Null_Address then
         raise Provider_Error with "native pixel span has a null pointer with a nonzero size";
      end if;
      return Result;
   end To_Lease_Metadata;

   --  Reads the snapshot view and converts the metadata. Never closes the
   --  snapshot; the caller owns that decision.
   function View_Metadata (Handle : C.Frame_Snapshot_Handle) return Lease_Metadata is
      Address : aliased System.Address := System.Null_Address;
      D       : aliased Diagnostic := [others => Interfaces.C.nul];
      R       : aliased C.Size_T := 0;
   begin
      Check (C.IR_Frame_Snapshot_View (Handle, Address'Access, D'Address, D'Length, R'Access), D);
      if Address = System.Null_Address then
         raise Provider_Error with "native frame snapshot returned a null view";
      end if;
      return To_Lease_Metadata (To_View (Address).all);
   end View_Metadata;

   --  Dequeues one native snapshot. On any failure the output handle stays
   --  null, so no snapshot is leaked.
   procedure Acquire_Snapshot
     (Object               : AMS.MEL.IR.Image_Stream;
      Timeout_Milliseconds : Natural;
      Handle               : aliased in out C.Frame_Snapshot_Handle)
   is
      D : aliased Diagnostic := [others => Interfaces.C.nul];
      R : aliased C.Size_T := 0;
   begin
      Check
        (C.IR_Stream_Receive_Snapshot
           (Object.Handle,
            Interfaces.Unsigned_32 (Timeout_Milliseconds),
            Handle'Access,
            D'Address,
            D'Length,
            R'Access),
         D);
   end Acquire_Snapshot;

   --  A constrained borrowed view over native pixel storage. Only the address
   --  is bound; no element is read or written to build it, so construction is
   --  O(1) in the pixel count.
   procedure Borrow_Pixels
     (Payload : System.Address;
      Size    : Natural;
      Process : not null access procedure (Pixels : AMS.MEL.IR.Pixel_Array))
   is
      Empty : constant AMS.MEL.IR.Pixel_Array (1 .. 0) := [others => 0];
   begin
      if Size = 0 then
         Process (Empty);
         return;
      end if;
      declare
         View : constant AMS.MEL.IR.Pixel_Array (1 .. Size)
         with Import, Convention => C, Address => Payload;
      begin
         Process (View);
      end;
   end Borrow_Pixels;

   function Receive
     (Object : AMS.MEL.IR.Image_Stream; Timeout_Milliseconds : Natural := 0) return Full_Frame
   is
      Handle : aliased C.Frame_Snapshot_Handle := C.Null_Frame_Snapshot;
   begin
      Acquire_Snapshot (Object, Timeout_Milliseconds, Handle);
      begin
         declare
            Meta   : constant Lease_Metadata := View_Metadata (Handle);
            Result : Full_Frame;
            --  Compatibility path: this API is documented as owned/copying, so
            --  the payload is explicitly copied into Ada storage here and the
            --  native snapshot is released before returning.
            procedure Copy_Into (Pixels : AMS.MEL.IR.Pixel_Array) is
            begin
               Result.Data.Reserve_Capacity (Ada.Containers.Count_Type (Pixels'Length));
               for Value of Pixels loop
                  Result.Data.Append (Value);
               end loop;
            end Copy_Into;
         begin
            Result.Time := Meta.Time;
            Result.Integration := Meta.Integration;
            Result.W := Meta.W;
            Result.H := Meta.H;
            Result.BPP := Meta.BPP;
            Result.Bands := Meta.Bands;
            Result.HFOV := Meta.HFOV;
            Result.VFOV := Meta.VFOV;
            Result.Sensor := Meta.Sensor;
            Result.Format := Meta.Format;
            Result.ID := Meta.ID;
            Result.Sub_ID := Meta.Sub_ID;
            Result.Sub_Total := Meta.Sub_Total;
            Result.Kind := Meta.Kind;
            Result.Flip := Meta.Flip;
            Result.Flags := Meta.Flags;
            Result.D_Row := Meta.D_Row;
            Result.D_Column := Meta.D_Column;
            Result.Row := Meta.Row;
            Result.Column := Meta.Column;
            Result.Inertial := Meta.Inertial;
            Result.Nav := Meta.Nav;
            Result.Band := Meta.Band;
            Borrow_Pixels (Meta.Payload, Meta.Payload_Size, Copy_Into'Access);
            Close (Handle);
            return Result;
         end;
      exception
         when others =>
            Close (Handle);
            raise;
      end;
   end Receive;
   function System_Time_NS (Value : Full_Frame) return Long_Long_Integer
   is (Value.Time);
   function Integration_Time_NS (Value : Full_Frame) return Long_Long_Integer
   is (Value.Integration);
   function Width (Value : Full_Frame) return Interfaces.Unsigned_32
   is (Value.W);
   function Height (Value : Full_Frame) return Interfaces.Unsigned_32
   is (Value.H);
   function Bits_Per_Pixel (Value : Full_Frame) return Interfaces.Unsigned_32
   is (Value.BPP);
   function Number_Of_Bands (Value : Full_Frame) return Interfaces.Unsigned_32
   is (Value.Bands);
   function Horizontal_FOV_Rad (Value : Full_Frame) return Long_Float
   is (Value.HFOV);
   function Vertical_FOV_Rad (Value : Full_Frame) return Long_Float
   is (Value.VFOV);
   function Contributing_Sensor_Value (Value : Full_Frame) return Contributing_Sensor
   is (Value.Sensor);
   function Pixel_Format (Value : Full_Frame) return AMS.MEL.IR.Channel.Pixel_Format
   is (Value.Format);
   function Frame_ID (Value : Full_Frame) return Interfaces.Unsigned_32
   is (Value.ID);
   function Subframe_ID (Value : Full_Frame) return Interfaces.Unsigned_32
   is (Value.Sub_ID);
   function Subframe_Total (Value : Full_Frame) return Interfaces.Unsigned_32
   is (Value.Sub_Total);
   function Image_Type (Value : Full_Frame) return Full_Image_Type
   is (Value.Kind);
   function Image_Flip (Value : Full_Frame) return AMS.MEL.IR.Image_Flip
   is (Value.Flip);
   function Image_Flag_Count (Value : Full_Frame) return Natural
   is (Natural (Value.Flags.Length));
   function Image_Flag_At (Value : Full_Frame; Index : Positive) return Image_Flag
   is (Value.Flags.Element (Index));
   function Dither_Row (Value : Full_Frame) return Long_Float
   is (Value.D_Row);
   function Dither_Column (Value : Full_Frame) return Long_Float
   is (Value.D_Column);
   function Row_Offset (Value : Full_Frame) return Interfaces.Unsigned_32
   is (Value.Row);
   function Column_Offset (Value : Full_Frame) return Interfaces.Unsigned_32
   is (Value.Column);
   function Sensor_Inertial_State_Count (Value : Full_Frame) return Natural
   is (Natural (Value.Inertial.Length));
   function Sensor_Inertial_State_At
     (Value : Full_Frame; Index : Positive) return Sensor_Inertial_State
   is (Value.Inertial.Element (Index));
   function Sensor_Nav_State_Count (Value : Full_Frame) return Natural
   is (Natural (Value.Nav.Length));
   function Sensor_Nav_State_At (Value : Full_Frame; Index : Positive) return Sensor_Nav_State
   is (Value.Nav.Element (Index));
   function Band_Index (Value : Full_Frame) return AMS.MEL.IR.Byte
   is (Value.Band);
   function Pixel_Count (Value : Full_Frame) return Natural
   is (Natural (Value.Data.Length));
   function Pixels (Value : Full_Frame) return AMS.MEL.IR.Pixel_Array is
      Result : AMS.MEL.IR.Pixel_Array (1 .. Natural (Value.Data.Length));
   begin
      for I in Result'Range loop
         Result (I) := Value.Data.Element (I);
      end loop;
      return Result;
   end Pixels;

   type Diagnostic_Array is array (C.Size_T range <>) of aliased Interfaces.C.char
   with Convention => C;
   function Message (Value : Diagnostic_Array) return String is
      Last : Natural := 0;
   begin
      while Last < Value'Length and then Value (C.Size_T (Last)) /= Interfaces.C.nul loop
         Last := Last + 1;
      end loop;
      declare
         Result : String (1 .. Last);
      begin
         for I in Result'Range loop
            Result (I) := Character'Val (Interfaces.C.char'Pos (Value (C.Size_T (I - 1))));
         end loop;
         return
           (if Result'Length = 0 then "native IR Image navigation operation failed" else Result);
      end;
   end Message;

   function Raw_Euler (Value : AMS.MEL.Status.Euler) return C.Euler_V1
   is ((Roll  => Interfaces.C.double (Value.Roll),
        Pitch => Interfaces.C.double (Value.Pitch),
        Yaw   => Interfaces.C.double (Value.Yaw)));
   function Raw_NED (Value : North_East_Down) return C.North_East_Down_V1
   is ((North => Interfaces.C.double (Value.North),
        East  => Interfaces.C.double (Value.East),
        Down  => Interfaces.C.double (Value.Down)));
   function Raw_Attitude_Rate (Value : Attitude_Rate) return C.Attitude_Rate_V1
   is ((Attitude_Rate         => Raw_Euler (Value.Value),
        Attitude_Rate_Time_NS => Interfaces.Integer_64 (Value.System_Time_NS)));
   function Raw_Covariance
     (Value : Position_Velocity_Covariance) return C.Position_Velocity_Covariance_V1
   is ((Position_Position_Pn_Pn => Interfaces.C.double (Value.Position_Position_Pn_Pn),
        Position_Position_Pn_Pe => Interfaces.C.double (Value.Position_Position_Pn_Pe),
        Position_Position_Pn_Pd => Interfaces.C.double (Value.Position_Position_Pn_Pd),
        Position_Position_Pe_Pe => Interfaces.C.double (Value.Position_Position_Pe_Pe),
        Position_Position_Pe_Pd => Interfaces.C.double (Value.Position_Position_Pe_Pd),
        Position_Position_Pd_Pd => Interfaces.C.double (Value.Position_Position_Pd_Pd),
        Position_Velocity_Pn_Vn => Interfaces.C.double (Value.Position_Velocity_Pn_Vn),
        Position_Velocity_Pn_Ve => Interfaces.C.double (Value.Position_Velocity_Pn_Ve),
        Position_Velocity_Pn_Vd => Interfaces.C.double (Value.Position_Velocity_Pn_Vd),
        Position_Velocity_Pe_Ve => Interfaces.C.double (Value.Position_Velocity_Pe_Ve),
        Position_Velocity_Pe_Vd => Interfaces.C.double (Value.Position_Velocity_Pe_Vd),
        Position_Velocity_Pd_Vd => Interfaces.C.double (Value.Position_Velocity_Pd_Vd),
        Velocity_Velocity_Vn_Vn => Interfaces.C.double (Value.Velocity_Velocity_Vn_Vn),
        Velocity_Velocity_Vn_Ve => Interfaces.C.double (Value.Velocity_Velocity_Vn_Ve),
        Velocity_Velocity_Vn_Vd => Interfaces.C.double (Value.Velocity_Velocity_Vn_Vd),
        Velocity_Velocity_Ve_Ve => Interfaces.C.double (Value.Velocity_Velocity_Ve_Ve),
        Velocity_Velocity_Ve_Vd => Interfaces.C.double (Value.Velocity_Velocity_Ve_Vd),
        Velocity_Velocity_Vd_Vd => Interfaces.C.double (Value.Velocity_Velocity_Vd_Vd)));
   function Raw_Report (Value : Navigation_Report) return C.Navigation_Report_V1
   is ((System_Time_NS                           => Interfaces.Integer_64 (Value.System_Time_NS),
        State                                    => Position_Solution_State'Enum_Rep (Value.State),
        Latitude_Rad                             => Interfaces.C.double (Value.Latitude_Rad),
        Longitude_Rad                            => Interfaces.C.double (Value.Longitude_Rad),
        Altitude_M                               => Interfaces.C.double (Value.Altitude_M),
        Attitude                                 => Raw_Euler (Value.Attitude),
        Attitude_Rate                            => Raw_Attitude_Rate (Value.Attitude_Rate),
        Speed                                    => Raw_NED (Value.Speed),
        Acceleration                             => Raw_NED (Value.Acceleration),
        Wander_Angle_Rad                         => Interfaces.C.double (Value.Wander_Angle_Rad),
        Magnetic_Heading                         => Interfaces.C.double (Value.Magnetic_Heading),
        Altitude_MSL                             => Interfaces.C.double (Value.Altitude_MSL),
        Position_Velocity_Covariance_Uncertainty =>
          Raw_Covariance (Value.Position_Velocity_Covariance)));

   function Submit_Navigation_Report
     (Object : AMS.MEL.IR.Image_Stream; Report : Navigation_Report) return Navigation_Request
   is
      Raw : aliased C.Navigation_Report_V1 := Raw_Report (Report);
      D   : aliased Diagnostic := [others => Interfaces.C.nul];
      R   : aliased C.Size_T := 0;
   begin
      return Result : Navigation_Request do
         Check
           (C.IR_Stream_Submit_Navigation_Report
              (Object.Handle,
               Raw'Access,
               Result.Owner.Handle'Access,
               D'Address,
               D'Length,
               R'Access),
            D);
      end return;
   end Submit_Navigation_Report;

   function Is_Open (Request : Navigation_Request) return Boolean
   is (Request.Owner.Handle /= C.Null_Navigation_Request);

   function Status (Result : Navigation_Result) return Navigation_Outcome
   is (Result.Result_Status);
   function Response (Result : Navigation_Result) return Navigation_Response
   is (Result.Result_Response);
   function Rejection_Code (Result : Navigation_Result) return Navigation_Error_Code
   is (Result.Result_Code);
   function Description (Result : Navigation_Result) return String
   is (US.To_String (Result.Result_Text));

   function Wait
     (Request : Navigation_Request; Timeout_Milliseconds : Natural) return Navigation_Result
   is
      Raw  : aliased C.Navigation_Result_V1 := ((0, 0, 0), 0);
      D    : aliased Diagnostic_Array (0 .. 511) := [others => Interfaces.C.nul];
      R    : aliased C.Size_T := 0;
      Code : constant Interfaces.Integer_32 :=
        C.IR_Navigation_Request_Wait
          (Request.Owner.Handle,
           Interfaces.Unsigned_32 (Timeout_Milliseconds),
           Raw'Access,
           D'Address,
           D'Length,
           R'Access);
   begin
      if Code = C.Timeout then
         raise Timeout_Error with "IR Image navigation request timed out";
      elsif Code = C.Success then
         return
           (Result_Status   => Success,
            Result_Response =>
              (System_Time_NS => Long_Long_Integer (Raw.Response.System_Time_NS),
               Command_ID     => Raw.Response.Command_ID,
               Request_ID     => Raw.Response.Request_ID),
            Result_Code     => None,
            Result_Text     => US.Null_Unbounded_String);
      elsif Code = C.Command_Rejected then
         if Raw.Error_Code > 8 then
            raise Provider_Error with "native IR Image returned unknown MEL error code";
         end if;
         declare
            Text : US.Unbounded_String := US.To_Unbounded_String (Message (D));
         begin
            if R > D'Length then
               declare
                  Complete       : aliased Diagnostic_Array (0 .. R - 1) :=
                    [others => Interfaces.C.nul];
                  Retry_Raw      : aliased C.Navigation_Result_V1 := ((0, 0, 0), 0);
                  Retry_Required : aliased C.Size_T := 0;
                  Retry_Code     : constant Interfaces.Integer_32 :=
                    C.IR_Navigation_Request_Wait
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
                       with "native IR Image rejection changed during diagnostic retry";
                  end if;
                  Text := US.To_Unbounded_String (Message (Complete));
               end;
            end if;
            return
              (Result_Status   => Rejected,
               Result_Response => (0, 0, 0),
               Result_Code     => Navigation_Error_Code'Val (Raw.Error_Code),
               Result_Text     => Text);
         end;
      else
         raise Provider_Error with Message (D);
      end if;
   end Wait;

   procedure Close (Request : in out Navigation_Request) is
      Ignored : Interfaces.Integer_32;
   begin
      Ignored :=
        C.IR_Navigation_Request_Close (Request.Owner.Handle'Access, System.Null_Address, 0, null);
   end Close;

   overriding
   procedure Finalize (Request : in out Navigation_Request_Owner) is
      Ignored : Interfaces.Integer_32;
   begin
      Ignored :=
        C.IR_Navigation_Request_Close (Request.Handle'Access, System.Null_Address, 0, null);
   exception
      when others =>
         Request.Handle := C.Null_Navigation_Request;
   end Finalize;

   ---------------------------------------------------------------------------
   --  High-rate borrowed data plane (Task 030A)
   --
   --  Ownership chain:
   --
   --      QueuedFrame storage owned by ams_mel_ir_frame_snapshot   (native)
   --          -> ams_mel_ir_frame_snapshot *                       (opaque C)
   --              -> Frame_Lease_Owner.Handle                      (limited Ada)
   --                  -> Pixel_Array view bound to Pixels.data     (borrowed)
   --
   --  The native snapshot is fully independent of the stream, Session, and
   --  provider after dequeue, so a live lease keeps the payload valid through
   --  Stop, Close, Session close, and provider teardown.

   function Acquire_Frame
     (Object : AMS.MEL.IR.Image_Stream; Timeout_Milliseconds : Natural := 0) return Frame_Lease is
   begin
      return Result : Frame_Lease do
         Acquire_Snapshot (Object, Timeout_Milliseconds, Result.Owner.Handle);
         begin
            --  Only small metadata is converted here. The payload is retained
            --  as the snapshot's own address and length; nothing payload-sized
            --  is allocated or copied.
            Result.Owner.Data := View_Metadata (Result.Owner.Handle);
         exception
            when others =>
               --  A metadata conversion failure must not leak the snapshot.
               Close (Result.Owner.Handle);
               raise;
         end;
      end return;
   end Acquire_Frame;

   function Is_Open (Frame : Frame_Lease) return Boolean
   is (Frame.Owner.Handle /= C.Null_Frame_Snapshot);

   procedure Close (Frame : in out Frame_Lease) is
   begin
      --  Idempotent: the native close clears the handle, and a null handle is
      --  simply closed again with no effect.
      Close (Frame.Owner.Handle);
      Frame.Owner.Data.Payload := System.Null_Address;
      Frame.Owner.Data.Payload_Size := 0;
   end Close;

   overriding
   procedure Finalize (Frame : in out Frame_Lease_Owner) is
      Ignored : Interfaces.Integer_32;
   begin
      Ignored := C.IR_Frame_Snapshot_Close (Frame.Handle'Access, System.Null_Address, 0, null);
      Frame.Data.Payload := System.Null_Address;
      Frame.Data.Payload_Size := 0;
   exception
      when others =>
         Frame.Handle := C.Null_Frame_Snapshot;
   end Finalize;

   function Pixel_Count (Frame : Frame_Lease) return Natural
   is (Frame.Owner.Data.Payload_Size);

   procedure With_Pixels
     (Frame : Frame_Lease; Process : not null access procedure (Pixels : AMS.MEL.IR.Pixel_Array)) is
   begin
      if Frame.Owner.Handle = C.Null_Frame_Snapshot then
         raise Provider_Error with "IR frame lease is closed";
      end if;
      if Frame.Owner.Data.Payload_Size > 0 and then Frame.Owner.Data.Payload = System.Null_Address
      then
         raise Provider_Error with "IR frame lease has an invalid native pixel view";
      end if;
      Borrow_Pixels (Frame.Owner.Data.Payload, Frame.Owner.Data.Payload_Size, Process);
   end With_Pixels;

   function Copy_Pixels (Frame : Frame_Lease) return AMS.MEL.IR.Pixel_Array is
      Result : AMS.MEL.IR.Pixel_Array (1 .. Pixel_Count (Frame)) := [others => 0];
      procedure Copy_Into (Pixels : AMS.MEL.IR.Pixel_Array) is
      begin
         Result := Pixels;
      end Copy_Into;
   begin
      With_Pixels (Frame, Copy_Into'Access);
      return Result;
   end Copy_Pixels;

   function System_Time_NS (Frame : Frame_Lease) return Long_Long_Integer
   is (Frame.Owner.Data.Time);
   function Integration_Time_NS (Frame : Frame_Lease) return Long_Long_Integer
   is (Frame.Owner.Data.Integration);
   function Width (Frame : Frame_Lease) return Interfaces.Unsigned_32
   is (Frame.Owner.Data.W);
   function Height (Frame : Frame_Lease) return Interfaces.Unsigned_32
   is (Frame.Owner.Data.H);
   function Bits_Per_Pixel (Frame : Frame_Lease) return Interfaces.Unsigned_32
   is (Frame.Owner.Data.BPP);
   function Number_Of_Bands (Frame : Frame_Lease) return Interfaces.Unsigned_32
   is (Frame.Owner.Data.Bands);
   function Horizontal_FOV_Rad (Frame : Frame_Lease) return Long_Float
   is (Frame.Owner.Data.HFOV);
   function Vertical_FOV_Rad (Frame : Frame_Lease) return Long_Float
   is (Frame.Owner.Data.VFOV);
   function Contributing_Sensor_Value (Frame : Frame_Lease) return Contributing_Sensor
   is (Frame.Owner.Data.Sensor);
   function Pixel_Format (Frame : Frame_Lease) return AMS.MEL.IR.Channel.Pixel_Format
   is (Frame.Owner.Data.Format);
   function Frame_ID (Frame : Frame_Lease) return Interfaces.Unsigned_32
   is (Frame.Owner.Data.ID);
   function Subframe_ID (Frame : Frame_Lease) return Interfaces.Unsigned_32
   is (Frame.Owner.Data.Sub_ID);
   function Subframe_Total (Frame : Frame_Lease) return Interfaces.Unsigned_32
   is (Frame.Owner.Data.Sub_Total);
   function Image_Type (Frame : Frame_Lease) return Full_Image_Type
   is (Frame.Owner.Data.Kind);
   function Image_Flip (Frame : Frame_Lease) return AMS.MEL.IR.Image_Flip
   is (Frame.Owner.Data.Flip);
   function Image_Flag_Count (Frame : Frame_Lease) return Natural
   is (Natural (Frame.Owner.Data.Flags.Length));
   function Image_Flag_At (Frame : Frame_Lease; Index : Positive) return Image_Flag
   is (Frame.Owner.Data.Flags.Element (Index));
   function Dither_Row (Frame : Frame_Lease) return Long_Float
   is (Frame.Owner.Data.D_Row);
   function Dither_Column (Frame : Frame_Lease) return Long_Float
   is (Frame.Owner.Data.D_Column);
   function Row_Offset (Frame : Frame_Lease) return Interfaces.Unsigned_32
   is (Frame.Owner.Data.Row);
   function Column_Offset (Frame : Frame_Lease) return Interfaces.Unsigned_32
   is (Frame.Owner.Data.Column);
   function Sensor_Inertial_State_Count (Frame : Frame_Lease) return Natural
   is (Natural (Frame.Owner.Data.Inertial.Length));
   function Sensor_Inertial_State_At
     (Frame : Frame_Lease; Index : Positive) return Sensor_Inertial_State
   is (Frame.Owner.Data.Inertial.Element (Index));
   function Sensor_Nav_State_Count (Frame : Frame_Lease) return Natural
   is (Natural (Frame.Owner.Data.Nav.Length));
   function Sensor_Nav_State_At (Frame : Frame_Lease; Index : Positive) return Sensor_Nav_State
   is (Frame.Owner.Data.Nav.Element (Index));
   function Band_Index (Frame : Frame_Lease) return AMS.MEL.IR.Byte
   is (Frame.Owner.Data.Band);
end AMS.MEL.IR.Image;
