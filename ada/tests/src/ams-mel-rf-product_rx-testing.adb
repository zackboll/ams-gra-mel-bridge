with System.Storage_Elements;
with Interfaces;
with Interfaces.C;

package body AMS.MEL.RF.Product_Rx.Testing is
   procedure Check_Stabilization_Boundaries is
      package C renames AMS.MEL_C_API;
      use type Interfaces.Integer_64;
      use type Interfaces.C.double;
      use System.Storage_Elements;
      Raw    : aliased C.RF_Pointing_V1 :=
        (Kind                        => 3,
         ECEF                        => ((0.0, 0.0, 0.0), (0.0, 0.0, 0.0), (0, -1)),
         LLA                         => (0.0, 0.0, 0.0, 0.0, 0.0, 0.0, (0, -1)),
         Platform_Relative           => (0.0, 0.0),
         Face_Relative               => (18.5, -9.75),
         Baseline_Relative_Conic_Rad => 0.0);
      Target : Stabilization_Vectors.Vector;
      procedure Reject (Span : C.RF_Pointing_Span_V1) is
      begin
         Copy_Stabilization (Span, Target);
         raise Program_Error with "invalid stabilization boundary accepted";
      exception
         when Provider_Error =>
            null;
      end Reject;
   begin
      --  Inactive invalid UTC must not be inspected for relative pointings.
      Copy_Stabilization ((Raw'Address, 1), Target);
      Reject ((Raw'Address, 0));
      Reject ((System.Null_Address, 1));
      Reject ((Raw'Address, C.Size_T'Last));
      Reject ((To_Address (To_Integer (Raw'Address) + 1), 1));
      Reject ((To_Address (Integer_Address'Last - C.RF_Pointing_V1'Alignment + 1), 1));
      Raw.Kind := 5;
      Reject ((Raw'Address, 1));
      Raw.Kind := 0;
      Reject ((Raw'Address, 1));
      Raw.ECEF.Time_Of_Validity.Fractional_Femtoseconds := 1_000_000_000_000_000;
      Reject ((Raw'Address, 1));
      Raw.ECEF.Time_Of_Validity.Fractional_Femtoseconds := 0;
      Copy_Stabilization ((Raw'Address, 1), Target);
      declare
         M : Product_Metadata;
      begin
         M.Stabilization := Target;
         declare
            Point : constant AMS.MEL.RF.C2.Pointing := Stabilization_Point_At (M, 3);
         begin
            raise Program_Error with AMS.MEL.RF.C2.Kind (Point)'Image;
         end;
      exception
         when Constraint_Error =>
            null;
      end;
      declare
         package P renames AMS.MEL.RF.C2;
         Value : constant P.Pointing := P.Create_Face_Relative_Pointing (1.0, 2.0);
      begin
         for Field in 1 .. 6 loop
            begin
               case Field is
                  when 1      =>
                     declare
                        X : constant P.ECEF_Coordinates := P.ECEF_Position (Value);
                     begin
                        raise Program_Error with X.X'Image;
                     end;

                  when 2      =>
                     declare
                        X : constant P.ECEF_Coordinates := P.ECEF_Velocity (Value);
                     begin
                        raise Program_Error with X.X'Image;
                     end;

                  when 3      =>
                     declare
                        X : constant P.LLA_Coordinates := P.LLA_Position (Value);
                     begin
                        raise Program_Error with X.Altitude_M'Image;
                     end;

                  when 4      =>
                     declare
                        X : constant P.NED_Velocity := P.LLA_Velocity (Value);
                     begin
                        raise Program_Error with X.North_MPS'Image;
                     end;

                  when 5      =>
                     declare
                        X : constant Long_Float := P.Conic_Rad (Value);
                     begin
                        raise Program_Error with X'Image;
                     end;

                  when others =>
                     declare
                        X : constant P.UTC_Time := P.Time_Of_Validity (Value);
                     begin
                        raise Program_Error with P.Seconds (X)'Image;
                     end;
               end case;
            exception
               when Constraint_Error =>
                  null;
            end;
         end loop;
         for Field in 1 .. 2 loop
            begin
               declare
                  V : constant P.Pointing := P.Create_Baseline_Relative_Pointing (1.0);
                  X : constant Long_Float :=
                    (if Field = 1 then P.Azimuth_Rad (V) else P.Elevation_Rad (V));
               begin
                  raise Program_Error with X'Image;
               end;
            exception
               when Constraint_Error =>
                  null;
            end;
         end loop;
      end;
   end Check_Stabilization_Boundaries;
   procedure Check_Representation is
      Raw          : constant AMS.MEL_C_API.RF_Product_Rx_Event_V2 :=
        (Endpoint_ID => 0,
         Data_Format => 0,
         Samples_I8  => (System.Null_Address, 0),
         Samples_I16 => (System.Null_Address, 0),
         Metadata    =>
           (MEL_Protocol_Version_ID    => 0,
            VA_Definition_ID           => 0,
            VA_Instance_ID             => 0,
            Job_Details_ID             => 0,
            Job_Interval_ID            => 0,
            LF_Type_ID                 => 0,
            LF_Instance_ID             => 0,
            Phase_Coherence_With_Prior => 0,
            First_Rx_Event_Start_S     => 0,
            First_Rx_Event_Start_FS    => 0,
            Rx_Stream_IDs              => (System.Null_Address, 0)));
      Pair         : constant Complex_I8 := (0, 0);
      V3           : constant AMS.MEL_C_API.RF_Product_Rx_Event_V3 :=
        (Endpoint_ID         => 0,
         Data_Format         => 0,
         Direct_Samples_I8   => (System.Null_Address, 0),
         Direct_Samples_I16  => (System.Null_Address, 0),
         Complex_Samples_I8  => (System.Null_Address, 0),
         Complex_Samples_I16 => (System.Null_Address, 0),
         Metadata            => Raw.Metadata);
      V4           : constant AMS.MEL_C_API.RF_Product_Rx_Event_V4 :=
        (V3, (System.Null_Address, 0));
      Packet       : constant AMS.MEL_C_API.RF_VITA_Small_Packet_V1 :=
        (Serialized_Bytes => (System.Null_Address, 0));
      V5           : constant AMS.MEL_C_API.RF_Product_Rx_Event_V5 :=
        (V4, (System.Null_Address, 0));
      Medium       : constant AMS.MEL_C_API.RF_VITA_Medium_Packet_V1 :=
        (Serialized_Bytes => (System.Null_Address, 0));
      V6           : constant AMS.MEL_C_API.RF_Product_Rx_Event_V6 :=
        (V5, (System.Null_Address, 0));
      Large        : constant AMS.MEL_C_API.RF_VITA_Large_Packet_V1 :=
        (Serialized_Bytes => (System.Null_Address, 0));
      V7           : constant AMS.MEL_C_API.RF_Product_Rx_Event_V7 :=
        (V6, (System.Null_Address, 0));
      Extra        : constant AMS.MEL_C_API.RF_VITA_Extra_Large_Packet_V1 :=
        (Serialized_Bytes => (System.Null_Address, 0));
      Associations : constant AMS.MEL_C_API.RF_Receive_Event_Associations_V1 :=
        (0, (System.Null_Address, 0));
      V8           : constant AMS.MEL_C_API.RF_Product_Rx_Event_V8 := (V7, Associations);
      V9           : constant AMS.MEL_C_API.RF_Product_Rx_Event_V9 :=
        (V8, (System.Null_Address, 0));
   begin
      if System.Address'Size = 64 then
         if V9'Size /= 248 * 8
           or else V9'Alignment /= 8
           or else V9.Event'Position /= 0
           or else V9.Stabilization_Points'Position /= 232
           or else AMS.MEL_C_API.RF_Pointing_V1'Object_Size /= 176 * 8
         then
            raise Program_Error with "private Ada v9 differs from C11 layout";
         end if;
      end if;
      if System.Address'Size = 64 then
         if V8'Size /= 232 * 8
           or else AMS.MEL_C_API.RF_Product_Rx_Event_V8'Alignment /= 8
           or else V8.Event'Position /= 0
           or else V8.Receive_Event_Associations'Position /= 208
           or else Associations'Size /= 24 * 8
           or else AMS.MEL_C_API.RF_Receive_Event_Associations_V1'Alignment /= 8
           or else Associations.Present'Position /= 0
           or else Associations.Present'Size /= 32
           or else Associations.Event_IDs'Position /= 8
           or else Associations.Event_IDs.Data'Position /= 0
           or else Associations.Event_IDs.Size'Position /= 8
           or else AMS.MEL_C_API.U32_Span_V1'Alignment /= 8
         then
            raise Program_Error with "private Ada v8 differs from measured C11 layout";
         end if;
      end if;
      if System.Address'Size = 64 then
         if V7'Size /= 208 * 8
           or else AMS.MEL_C_API.RF_Product_Rx_Event_V7'Alignment /= 8
           or else V7.Event'Position /= 0
           or else V7.VITA_Extra_Large_Packets'Position /= 192
           or else Extra'Size /= 128
           or else Extra.Serialized_Bytes'Position /= 0
         then
            raise Program_Error with "private Ada ProductRx v7/C layout mismatch";
         end if;
         if V6'Size /= 192 * 8
           or else AMS.MEL_C_API.RF_Product_Rx_Event_V6'Alignment /= 8
           or else V6.Event'Position /= 0
           or else V6.VITA_Large_Packets'Position /= 176
           or else V6.VITA_Large_Packets'Size /= 128
           or else AMS.MEL_C_API.RF_VITA_Large_Packet_Span_V1'Alignment /= 8
           or else V6.VITA_Large_Packets.Data'Position /= 0
           or else V6.VITA_Large_Packets.Size'Position /= 8
           or else Large'Size /= 128
           or else AMS.MEL_C_API.RF_VITA_Large_Packet_V1'Alignment /= 8
           or else Large.Serialized_Bytes'Position /= 0
         then
            raise Program_Error with "RF Large event differs from measured C11 layout";
         end if;
      end if;
      if System.Address'Size = 64 then
         if V5'Size /= 176 * 8
           or else AMS.MEL_C_API.RF_Product_Rx_Event_V5'Alignment /= 8
           or else V5.Event'Position /= 0
           or else V5.VITA_Medium_Packets'Position /= 160
           or else V5.VITA_Medium_Packets'Size /= 128
           or else AMS.MEL_C_API.RF_VITA_Medium_Packet_Span_V1'Alignment /= 8
           or else V5.VITA_Medium_Packets.Data'Position /= 0
           or else V5.VITA_Medium_Packets.Size'Position /= 8
           or else Medium'Size /= 128
           or else AMS.MEL_C_API.RF_VITA_Medium_Packet_V1'Alignment /= 8
           or else Medium.Serialized_Bytes'Position /= 0
         then
            raise Program_Error with "RF Medium event differs from measured C11 layout";
         end if;
      end if;
      if System.Address'Size = 64 then
         if V4'Size /= 160 * 8
           or else AMS.MEL_C_API.RF_Product_Rx_Event_V4'Alignment /= 8
           or else V4.Event'Position /= 0
           or else V4.VITA_Small_Packets'Position /= 144
           or else V4.VITA_Small_Packets'Size /= 128
           or else AMS.MEL_C_API.RF_VITA_Small_Packet_Span_V1'Alignment /= 8
           or else V4.VITA_Small_Packets.Data'Position /= 0
           or else V4.VITA_Small_Packets.Size'Position /= 8
           or else Packet'Size /= 128
           or else AMS.MEL_C_API.RF_VITA_Small_Packet_V1'Alignment /= 8
           or else Packet.Serialized_Bytes'Position /= 0
         then
            raise Program_Error with "RF VITA event differs from measured C11 layout";
         end if;
      end if;
      --  Values measured by the compiled C11 product_rx_v2_layout_probe.
      --  This runtime qualification is for the current 64-bit platform.
      if System.Address'Size = 64 then
         if V3'Size /= 144 * 8
           or else AMS.MEL_C_API.RF_Product_Rx_Event_V3'Alignment /= 8
           or else V3.Endpoint_ID'Position /= 0
           or else V3.Data_Format'Position /= 8
           or else V3.Direct_Samples_I8'Position /= 16
           or else V3.Direct_Samples_I16'Position /= 32
           or else V3.Complex_Samples_I8'Position /= 48
           or else V3.Complex_Samples_I16'Position /= 64
           or else V3.Metadata'Position /= 80
           or else V3.Direct_Samples_I8'Size /= 128
           or else V3.Direct_Samples_I16'Size /= 128
           or else V3.Direct_Samples_I8.Data'Position /= 0
           or else V3.Direct_Samples_I8.Size'Position /= 8
           or else V3.Direct_Samples_I16.Data'Position /= 0
           or else V3.Direct_Samples_I16.Size'Position /= 8
         then
            raise Program_Error with "RF event v3 differs from measured C11 layout";
         end if;
         if Raw'Size /= 112 * 8
           or else Raw'Alignment /= 8
           or else Raw.Endpoint_ID'Position /= 0
           or else Raw.Data_Format'Position /= 8
           or else Raw.Samples_I8'Position /= 16
           or else Raw.Samples_I16'Position /= 32
           or else Raw.Metadata'Position /= 48
           or else Raw.Samples_I8'Size /= 16 * 8
           or else Raw.Samples_I16'Size /= 16 * 8
           or else Raw.Samples_I8.Data'Position /= 0
           or else Raw.Samples_I8.Size'Position /= 8
           or else Raw.Samples_I16.Data'Position /= 0
           or else Raw.Samples_I16.Size'Position /= 8
           or else Raw.Metadata'Size /= 64 * 8
           or else Raw.Metadata.MEL_Protocol_Version_ID'Position /= 0
           or else Raw.Metadata.VA_Definition_ID'Position /= 4
           or else Raw.Metadata.VA_Instance_ID'Position /= 8
           or else Raw.Metadata.Job_Details_ID'Position /= 12
           or else Raw.Metadata.Job_Interval_ID'Position /= 16
           or else Raw.Metadata.LF_Type_ID'Position /= 20
           or else Raw.Metadata.LF_Instance_ID'Position /= 24
           or else Raw.Metadata.Phase_Coherence_With_Prior'Position /= 28
           or else Raw.Metadata.First_Rx_Event_Start_S'Position /= 32
           or else Raw.Metadata.First_Rx_Event_Start_FS'Position /= 40
           or else Raw.Metadata.Rx_Stream_IDs'Position /= 48
         then
            raise Program_Error with "RF event v2 differs from measured C11 layout";
         end if;
      end if;
      if Pair'Size /= 16
        or else Complex_I8'Alignment /= 1
        or else Pair.Real'Position /= 0
        or else Pair.Imag'Position /= 1
      then
         raise Program_Error with "RF Complex_I8 differs from measured C11 layout";
      end if;
      if Complex_I16'Size /= AMS.MEL_C_API.RF_Complex_I16_V1'Size then
         raise Program_Error with "RF Complex_I16 Size mismatch";
      end if;
      if Complex_I16'Object_Size /= AMS.MEL_C_API.RF_Complex_I16_V1'Object_Size then
         raise Program_Error with "RF Complex_I16 Object_Size mismatch";
      end if;
      if Complex_I16'Alignment /= AMS.MEL_C_API.RF_Complex_I16_V1'Alignment then
         raise Program_Error with "RF Complex_I16 Alignment mismatch";
      end if;
   end Check_Representation;

   function Native_Sample_Address (Value : Event) return System.Address
   is (Value.Sample_Address);
   function Native_Endpoint_Address (Value : Endpoint) return System.Address
   is (System.Address (Value.Handle));
end AMS.MEL.RF.Product_Rx.Testing;
