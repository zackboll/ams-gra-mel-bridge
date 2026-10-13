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
      function Zero_waveform_profile_params_v1 is new Zero (C.RF_waveform_profile_params_v1);
      Value_waveform_profile_params_v1             : constant C.RF_waveform_profile_params_v1 :=
        Zero_waveform_profile_params_v1;
      function Zero_waveform_profile_params_span_v1 is new
        Zero (C.RF_waveform_profile_params_span_v1);
      Value_waveform_profile_params_span_v1        :
        constant C.RF_waveform_profile_params_span_v1 := Zero_waveform_profile_params_span_v1;
      function Zero_waveform_profile_iterations_v1 is new
        Zero (C.RF_waveform_profile_iterations_v1);
      Value_waveform_profile_iterations_v1         : constant C.RF_waveform_profile_iterations_v1 :=
        Zero_waveform_profile_iterations_v1;
      function Zero_waveform_profile_iterations_span_v1 is new
        Zero (C.RF_waveform_profile_iterations_span_v1);
      Value_waveform_profile_iterations_span_v1    :
        constant C.RF_waveform_profile_iterations_span_v1 :=
          Zero_waveform_profile_iterations_span_v1;
      function Zero_event_waveform_profile_pattern_v1 is new
        Zero (C.RF_event_waveform_profile_pattern_v1);
      Value_event_waveform_profile_pattern_v1      :
        constant C.RF_event_waveform_profile_pattern_v1 := Zero_event_waveform_profile_pattern_v1;
      function Zero_event_waveform_profile_pattern_span_v1 is new
        Zero (C.RF_event_waveform_profile_pattern_span_v1);
      Value_event_waveform_profile_pattern_span_v1 :
        constant C.RF_event_waveform_profile_pattern_span_v1 :=
          Zero_event_waveform_profile_pattern_span_v1;
      function Zero_custom_waveform_config_v1 is new Zero (C.RF_custom_waveform_config_v1);
      Value_custom_waveform_config_v1              : constant C.RF_custom_waveform_config_v1 :=
        Zero_custom_waveform_config_v1;
      function Zero_modulation_config_v3 is new Zero (C.RF_modulation_config_v3);
      Value_modulation_config_v3                   : constant C.RF_modulation_config_v3 :=
        Zero_modulation_config_v3;
      function Zero_modulation_config_span_v3 is new Zero (C.RF_modulation_config_span_v3);
      Value_modulation_config_span_v3              : constant C.RF_modulation_config_span_v3 :=
        Zero_modulation_config_span_v3;
      function Zero_job_interval_config_v13 is new Zero (C.RF_job_interval_config_v13);
      Value_job_interval_config_v13                : constant C.RF_job_interval_config_v13 :=
        Zero_job_interval_config_v13;
      function Zero_job_interval_config_span_v13 is new Zero (C.RF_job_interval_config_span_v13);
      Value_job_interval_config_span_v13           : constant C.RF_job_interval_config_span_v13 :=
        Zero_job_interval_config_span_v13;
      function Zero_RF_Complex_I8_V1 is new Zero (C.RF_Complex_I8_V1);
      Value_RF_Complex_I8_V1                       : constant C.RF_Complex_I8_V1 :=
        Zero_RF_Complex_I8_V1;
      function Zero_RF_Complex_I8_Span_V1 is new Zero (C.RF_Complex_I8_Span_V1);
      Value_RF_Complex_I8_Span_V1                  : constant C.RF_Complex_I8_Span_V1 :=
        Zero_RF_Complex_I8_Span_V1;
      function Zero_RF_Complex_I16_Span_V1 is new Zero (C.RF_Complex_I16_Span_V1);
      Value_RF_Complex_I16_Span_V1                 : constant C.RF_Complex_I16_Span_V1 :=
        Zero_RF_Complex_I16_Span_V1;
      function Zero_RF_Waveform_Buffer_Config_V1 is new Zero (C.RF_Waveform_Buffer_Config_V1);
      Value_RF_Waveform_Buffer_Config_V1           : constant C.RF_Waveform_Buffer_Config_V1 :=
        Zero_RF_Waveform_Buffer_Config_V1;
      function Zero_RF_Modulation_Config_V4 is new Zero (C.RF_Modulation_Config_V4);
      Value_RF_Modulation_Config_V4                : constant C.RF_Modulation_Config_V4 :=
        Zero_RF_Modulation_Config_V4;
      function Zero_RF_Modulation_Config_Span_V4 is new Zero (C.RF_Modulation_Config_Span_V4);
      Value_RF_Modulation_Config_Span_V4           : constant C.RF_Modulation_Config_Span_V4 :=
        Zero_RF_Modulation_Config_Span_V4;
      function Zero_RF_Job_Interval_Config_V14 is new Zero (C.RF_Job_Interval_Config_V14);
      Value_RF_Job_Interval_Config_V14             : constant C.RF_Job_Interval_Config_V14 :=
        Zero_RF_Job_Interval_Config_V14;
      function Zero_RF_Job_Interval_Config_Span_V14 is new Zero (C.RF_Job_Interval_Config_Span_V14);
      Value_RF_Job_Interval_Config_Span_V14        : constant C.RF_Job_Interval_Config_Span_V14 :=
        Zero_RF_Job_Interval_Config_Span_V14;
      function Zero_Modulation is new Zero (C.RF_Modulation_Config_V1);
      function Zero_Event is new Zero (C.RF_TX_Event_Config_V3);
      function Zero_Interval is new Zero (C.RF_Job_Interval_Config_V11);
      function Zero_Binary is new Zero (C.RF_Modulation_Config_V2);
      function Zero_V12 is new Zero (C.RF_Job_Interval_Config_V12);
      Binary                                       : constant C.RF_Modulation_Config_V2 :=
        Zero_Binary;
      V12                                          : constant C.RF_Job_Interval_Config_V12 :=
        Zero_V12;
      Modulation                                   : constant C.RF_Modulation_Config_V1 :=
        Zero_Modulation;
      Event                                        : constant C.RF_TX_Event_Config_V3 := Zero_Event;
      Interval                                     : constant C.RF_Job_Interval_Config_V11 :=
        Zero_Interval;
      Value_RF_Waveform_Stream_Config_V1           : constant C.RF_Waveform_Stream_Config_V1 :=
        (0, 0, 0, 0, 0, 0);
      Value_RF_Modulation_Config_V5                : constant C.RF_Modulation_Config_V5 :=
        (Value_RF_Modulation_Config_V4, Value_RF_Waveform_Stream_Config_V1);
      function Zero_RF_Modulation_Config_Span_V5 is new Zero (C.RF_Modulation_Config_Span_V5);
      Value_RF_Modulation_Config_Span_V5           : constant C.RF_Modulation_Config_Span_V5 :=
        Zero_RF_Modulation_Config_Span_V5;
      function Zero_RF_Job_Interval_Config_V16 is new Zero (C.RF_Job_Interval_Config_V16);
      V16                                          : constant C.RF_Job_Interval_Config_V16 :=
        Zero_RF_Job_Interval_Config_V16;
      function Zero_RF_Job_Interval_Config_Span_V16 is new Zero (C.RF_Job_Interval_Config_Span_V16);
      S16                                          : constant C.RF_Job_Interval_Config_Span_V16 :=
        Zero_RF_Job_Interval_Config_Span_V16;
      function Zero_RF_Job_Interval_Config_V15 is new Zero (C.RF_Job_Interval_Config_V15);
      Value_RF_Job_Interval_Config_V15             : constant C.RF_Job_Interval_Config_V15 :=
        Zero_RF_Job_Interval_Config_V15;
      function Zero_RF_Job_Interval_Config_Span_V15 is new Zero (C.RF_Job_Interval_Config_Span_V15);
      Value_RF_Job_Interval_Config_Span_V15        : constant C.RF_Job_Interval_Config_Span_V15 :=
        Zero_RF_Job_Interval_Config_Span_V15;
   begin
      --  The actual compiled C layout probe qualifies these LP64 measurements.
      --  Do not count this conditional check as narrower-target runtime evidence.
      if C.Size_T'Size = 64 then
         if C.RF_Waveform_Stream_Config_V1'Object_Size /= 320
           or else C.RF_Waveform_Stream_Config_V1'Alignment /= 8
           or else Value_RF_Waveform_Stream_Config_V1.Endpoint_ID'Position /= 0
           or else Value_RF_Waveform_Stream_Config_V1.Start_Address'Position /= 8
           or else Value_RF_Waveform_Stream_Config_V1.Num_Bytes'Position /= 16
           or else Value_RF_Waveform_Stream_Config_V1.Sample_Format'Position /= 24
           or else Value_RF_Waveform_Stream_Config_V1.Item_Format'Position /= 28
           or else Value_RF_Waveform_Stream_Config_V1.Repeat_Mode'Position /= 32
           or else C.RF_Modulation_Config_V5'Object_Size /= 1472
           or else C.RF_Modulation_Config_V5'Alignment /= 8
           or else Value_RF_Modulation_Config_V5.Modulation'Position /= 0
           or else Value_RF_Modulation_Config_V5.Waveform_Stream'Position /= 144
           or else C.RF_Modulation_Config_Span_V5'Object_Size /= 128
           or else C.RF_Modulation_Config_Span_V5'Alignment /= 8
           or else Value_RF_Modulation_Config_Span_V5.Data'Position /= 0
           or else Value_RF_Modulation_Config_Span_V5.Size'Position /= 8
           or else C.RF_Job_Interval_Config_V16'Object_Size /= 2496
           or else C.RF_Job_Interval_Config_V16'Alignment /= 8
           or else V16.Interval'Position /= 0
           or else V16.Has_Transmit_Events'Position /= 240
           or else V16.Transmit_Events'Position /= 248
           or else V16.Has_Modulations'Position /= 264
           or else V16.Modulations'Position /= 272
           or else V16.Has_User_Context'Position /= 288
           or else V16.User_Context'Position /= 296
           or else C.RF_Job_Interval_Config_Span_V16'Object_Size /= 128
           or else C.RF_Job_Interval_Config_Span_V16'Alignment /= 8
           or else S16.Data'Position /= 0
           or else S16.Size'Position /= 8
           or else C.RF_Job_Interval_Config_V15'Object_Size /= 2304
           or else C.RF_Job_Interval_Config_V15'Alignment /= 8
           or else Value_RF_Job_Interval_Config_V15.Interval'Position /= 0
           or else Value_RF_Job_Interval_Config_V15.Has_Transmit_Events'Position /= 240
           or else Value_RF_Job_Interval_Config_V15.Transmit_Events'Position /= 248
           or else Value_RF_Job_Interval_Config_V15.Has_Modulations'Position /= 264
           or else Value_RF_Job_Interval_Config_V15.Modulations'Position /= 272
           or else C.RF_Job_Interval_Config_Span_V15'Object_Size /= 128
           or else C.RF_Job_Interval_Config_Span_V15'Alignment /= 8
           or else Value_RF_Job_Interval_Config_Span_V15.Data'Position /= 0
           or else Value_RF_Job_Interval_Config_Span_V15.Size'Position /= 8
         then
            raise Program_Error with "WaveformStream C/Ada layout mismatch";
         end if;
         if C.RF_Complex_I8_V1'Object_Size /= 16
           or else C.RF_Complex_I8_V1'Alignment /= 1
           or else Value_RF_Complex_I8_V1.Real'Position /= 0
           or else Value_RF_Complex_I8_V1.Imag'Position /= 1
           or else C.RF_Complex_I8_Span_V1'Object_Size /= 128
           or else C.RF_Complex_I8_Span_V1'Alignment /= 8
           or else Value_RF_Complex_I8_Span_V1.Data'Position /= 0
           or else Value_RF_Complex_I8_Span_V1.Size'Position /= 8
           or else C.RF_Complex_I16_Span_V1'Object_Size /= 128
           or else C.RF_Complex_I16_Span_V1'Alignment /= 8
           or else Value_RF_Complex_I16_Span_V1.Data'Position /= 0
           or else Value_RF_Complex_I16_Span_V1.Size'Position /= 8
           or else C.RF_Waveform_Buffer_Config_V1'Object_Size /= 320
           or else C.RF_Waveform_Buffer_Config_V1'Alignment /= 8
           or else Value_RF_Waveform_Buffer_Config_V1.Data_Format'Position /= 0
           or else Value_RF_Waveform_Buffer_Config_V1.Samples_I8'Position /= 8
           or else Value_RF_Waveform_Buffer_Config_V1.Samples_I16'Position /= 24
           or else C.RF_Modulation_Config_V4'Object_Size /= 1152
           or else C.RF_Modulation_Config_V4'Alignment /= 8
           or else Value_RF_Modulation_Config_V4.Modulation'Position /= 0
           or else Value_RF_Modulation_Config_V4.Waveform_Buffer'Position /= 104
           or else C.RF_Modulation_Config_Span_V4'Object_Size /= 128
           or else C.RF_Modulation_Config_Span_V4'Alignment /= 8
           or else Value_RF_Modulation_Config_Span_V4.Data'Position /= 0
           or else Value_RF_Modulation_Config_Span_V4.Size'Position /= 8
           or else C.RF_Job_Interval_Config_V14'Object_Size /= 2304
           or else C.RF_Job_Interval_Config_V14'Alignment /= 8
           or else Value_RF_Job_Interval_Config_V14.Interval'Position /= 0
           or else Value_RF_Job_Interval_Config_V14.Has_Transmit_Events'Position /= 240
           or else Value_RF_Job_Interval_Config_V14.Transmit_Events'Position /= 248
           or else Value_RF_Job_Interval_Config_V14.Has_Modulations'Position /= 264
           or else Value_RF_Job_Interval_Config_V14.Modulations'Position /= 272
           or else C.RF_Job_Interval_Config_Span_V14'Object_Size /= 128
           or else C.RF_Job_Interval_Config_Span_V14'Alignment /= 8
           or else Value_RF_Job_Interval_Config_Span_V14.Data'Position /= 0
           or else Value_RF_Job_Interval_Config_Span_V14.Size'Position /= 8
         then
            raise Program_Error with "WaveformBuffer C/Ada layout mismatch";
         end if;
         if C.RF_waveform_profile_params_v1'Object_Size /= 640
           or else C.RF_waveform_profile_params_v1'Alignment /= 8
           or else Value_waveform_profile_params_v1.chip_duration_femtoseconds'Position /= 0
           or else Value_waveform_profile_params_v1.update_phase'Position /= 8
           or else Value_waveform_profile_params_v1.update_delta_phase'Position /= 12
           or else Value_waveform_profile_params_v1.update_frequency_offset'Position /= 16
           or else Value_waveform_profile_params_v1.update_delta_frequency'Position /= 20
           or else Value_waveform_profile_params_v1.update_frequency_slope'Position /= 24
           or else Value_waveform_profile_params_v1.update_amplitude_factor'Position /= 28
           or else Value_waveform_profile_params_v1.phase_deg'Position /= 32
           or else Value_waveform_profile_params_v1.delta_phase_deg'Position /= 40
           or else Value_waveform_profile_params_v1.frequency_offset_hz'Position /= 48
           or else Value_waveform_profile_params_v1.delta_frequency_hz'Position /= 56
           or else Value_waveform_profile_params_v1.frequency_slope_hz_sec'Position /= 64
           or else Value_waveform_profile_params_v1.amplitude_factor'Position /= 72
           or else C.RF_waveform_profile_params_span_v1'Object_Size /= 128
           or else C.RF_waveform_profile_params_span_v1'Alignment /= 8
           or else Value_waveform_profile_params_span_v1.data'Position /= 0
           or else Value_waveform_profile_params_span_v1.size'Position /= 8
           or else C.RF_waveform_profile_iterations_v1'Object_Size /= 64
           or else C.RF_waveform_profile_iterations_v1'Alignment /= 4
           or else Value_waveform_profile_iterations_v1.num_chip_iterations'Position /= 0
           or else Value_waveform_profile_iterations_v1.waveform_profile_index'Position /= 4
           or else C.RF_waveform_profile_iterations_span_v1'Object_Size /= 128
           or else C.RF_waveform_profile_iterations_span_v1'Alignment /= 8
           or else Value_waveform_profile_iterations_span_v1.data'Position /= 0
           or else Value_waveform_profile_iterations_span_v1.size'Position /= 8
           or else C.RF_event_waveform_profile_pattern_v1'Object_Size /= 128
           or else C.RF_event_waveform_profile_pattern_v1'Alignment /= 8
           or else Value_event_waveform_profile_pattern_v1.steps'Position /= 0
           or else C.RF_event_waveform_profile_pattern_span_v1'Object_Size /= 128
           or else C.RF_event_waveform_profile_pattern_span_v1'Alignment /= 8
           or else Value_event_waveform_profile_pattern_span_v1.data'Position /= 0
           or else Value_event_waveform_profile_pattern_span_v1.size'Position /= 8
           or else C.RF_custom_waveform_config_v1'Object_Size /= 256
           or else C.RF_custom_waveform_config_v1'Alignment /= 8
           or else Value_custom_waveform_config_v1.profiles'Position /= 0
           or else Value_custom_waveform_config_v1.patterns'Position /= 16
           or else C.RF_modulation_config_v3'Object_Size /= 832
           or else C.RF_modulation_config_v3'Alignment /= 8
           or else Value_modulation_config_v3.modulation'Position /= 0
           or else Value_modulation_config_v3.custom_waveform'Position /= 72
           or else C.RF_modulation_config_span_v3'Object_Size /= 128
           or else C.RF_modulation_config_span_v3'Alignment /= 8
           or else Value_modulation_config_span_v3.data'Position /= 0
           or else Value_modulation_config_span_v3.size'Position /= 8
           or else C.RF_job_interval_config_v13'Object_Size /= 2304
           or else C.RF_job_interval_config_v13'Alignment /= 8
           or else Value_job_interval_config_v13.interval'Position /= 0
           or else Value_job_interval_config_v13.has_transmit_events'Position /= 240
           or else Value_job_interval_config_v13.transmit_events'Position /= 248
           or else Value_job_interval_config_v13.has_modulations'Position /= 264
           or else Value_job_interval_config_v13.modulations'Position /= 272
           or else C.RF_job_interval_config_span_v13'Object_Size /= 128
           or else C.RF_job_interval_config_span_v13'Alignment /= 8
           or else Value_job_interval_config_span_v13.data'Position /= 0
           or else Value_job_interval_config_span_v13.size'Position /= 8
         then
            raise Program_Error with "CustomWaveform C/Ada layout mismatch";
         end if;
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
