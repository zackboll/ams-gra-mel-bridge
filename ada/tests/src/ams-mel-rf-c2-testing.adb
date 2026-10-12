with Ada.Unchecked_Conversion;
with Interfaces;

package body AMS.MEL.RF.C2.Testing is
   procedure Check_Modulation_Representation is
      package C renames AMS.MEL_C_API;
      generic
         type Value_Type is private;
      function Zero return Value_Type;
      function Zero return Value_Type is
         type Bytes is array (1 .. Value_Type'Object_Size / 8) of Interfaces.Unsigned_8;
         function Convert is new Ada.Unchecked_Conversion (Bytes, Value_Type);
      begin
         return Convert ([others => 0]);
      end Zero;
      function Zero_Modulation is new Zero (C.RF_Modulation_Config_V1);
      function Zero_Event is new Zero (C.RF_TX_Event_Config_V3);
      function Zero_Interval is new Zero (C.RF_Job_Interval_Config_V11);
      function Zero_Binary is new Zero (C.RF_Modulation_Config_V2);
      function Zero_V12 is new Zero (C.RF_Job_Interval_Config_V12);
      Binary     : constant C.RF_Modulation_Config_V2 := Zero_Binary;
      V12        : constant C.RF_Job_Interval_Config_V12 := Zero_V12;
      Modulation : constant C.RF_Modulation_Config_V1 := Zero_Modulation;
      Event      : constant C.RF_TX_Event_Config_V3 := Zero_Event;
      Interval   : constant C.RF_Job_Interval_Config_V11 := Zero_Interval;
   begin
      --  The actual compiled C layout probe qualifies these LP64 measurements.
      --  Do not count this conditional check as narrower-target runtime evidence.
      if C.Size_T'Size = 64 then
         if C.RF_Modulation_Config_V2'Object_Size /= 576
           or else C.RF_Modulation_Config_V2'Alignment /= 8
           or else Binary.Modulation'Position /= 0
           or else Binary.Binary_Data'Position /= 40
           or else Binary.Pulse_Duration_Femtoseconds'Position /= 56
           or else Binary.Frequency_Offset_Hz'Position /= 64
           or else C.RF_Modulation_Config_Span_V2'Object_Size /= 128
           or else C.RF_Modulation_Config_Span_V2'Alignment /= 8
           or else C.RF_Job_Interval_Config_V12'Object_Size /= 2304
           or else C.RF_Job_Interval_Config_V12'Alignment /= 8
           or else V12.Interval'Position /= 0
           or else V12.Has_Transmit_Events'Position /= 240
           or else V12.Transmit_Events'Position /= 248
           or else V12.Has_Modulations'Position /= 264
           or else V12.Modulations'Position /= 272
           or else C.RF_Job_Interval_Config_Span_V12'Object_Size /= 128
           or else C.RF_Job_Interval_Config_Span_V12'Alignment /= 8
         then
            raise Program_Error with "binary modulation C/Ada representation mismatch";
         end if;
         if C.Double_Span_V1'Object_Size /= 128
           or else C.Double_Span_V1'Alignment /= 8
           or else C.I32_Span_V1'Object_Size /= 128
           or else C.I32_Span_V1'Alignment /= 8
           or else C.RF_Modulation_Config_V1'Object_Size /= 320
           or else C.RF_Modulation_Config_V1'Alignment /= 8
           or else Modulation.Kind'Position /= 0
           or else Modulation.Chip_Duration_Femtoseconds'Position /= 8
           or else Modulation.Phase_Chips_Rad'Position /= 16
           or else Modulation.Frequency_Extent_Hz'Position /= 32
           or else C.RF_Modulation_Config_Span_V1'Object_Size /= 128
           or else C.RF_TX_Event_Config_V3'Object_Size /= 1536
           or else C.RF_TX_Event_Config_V3'Alignment /= 8
           or else Event.Event'Position /= 0
           or else Event.Has_Modulation_Indices'Position /= 168
           or else Event.Modulation_Indices'Position /= 176
           or else C.RF_TX_Event_Config_Span_V3'Object_Size /= 128
           or else C.RF_Job_Interval_Config_V11'Object_Size /= 2304
           or else C.RF_Job_Interval_Config_V11'Alignment /= 8
           or else Interval.Interval'Position /= 0
           or else Interval.Has_Transmit_Events'Position /= 240
           or else Interval.Transmit_Events'Position /= 248
           or else Interval.Has_Modulations'Position /= 264
           or else Interval.Modulations'Position /= 272
           or else C.RF_Job_Interval_Config_Span_V11'Object_Size /= 128
         then
            raise Program_Error with "new modulation C/Ada representation mismatch";
         end if;
      end if;
   end Check_Modulation_Representation;
end AMS.MEL.RF.C2.Testing;
