package body AMS.MEL.RF.Product_Rx.Testing is
   procedure Check_Representation is
      Raw    : constant AMS.MEL_C_API.RF_Product_Rx_Event_V2 :=
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
      Pair   : constant Complex_I8 := (0, 0);
      V3     : constant AMS.MEL_C_API.RF_Product_Rx_Event_V3 :=
        (Endpoint_ID         => 0,
         Data_Format         => 0,
         Direct_Samples_I8   => (System.Null_Address, 0),
         Direct_Samples_I16  => (System.Null_Address, 0),
         Complex_Samples_I8  => (System.Null_Address, 0),
         Complex_Samples_I16 => (System.Null_Address, 0),
         Metadata            => Raw.Metadata);
      V4     : constant AMS.MEL_C_API.RF_Product_Rx_Event_V4 := (V3, (System.Null_Address, 0));
      Packet : constant AMS.MEL_C_API.RF_VITA_Small_Packet_V1 :=
        (Serialized_Bytes => (System.Null_Address, 0));
   begin
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
