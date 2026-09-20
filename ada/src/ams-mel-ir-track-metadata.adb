with Ada.Unchecked_Conversion;
with Interfaces.C;
with System;

package body AMS.MEL.IR.Track.Metadata is
   package C renames AMS.MEL_C_API;
   use type Interfaces.Integer_32;
   use type Interfaces.Unsigned_32;
   use type Interfaces.C.char;
   use type C.Track_Handle;
   use type C.Track_Metadata_Handle;
   use type C.Track_Event_Handle;

   type Diagnostic is array (C.Size_T range <>) of aliased Interfaces.C.char with Convention => C;
   subtype Fixed_Diagnostic is Diagnostic (0 .. 511);
   type Event_Access is access all C.IR_Track_Event_V1;
   function To_Event is new Ada.Unchecked_Conversion (System.Address, Event_Access);

   --  The native Track metadata event kinds defined by this release.
   Irst_Track_Report_Kind         : constant Interfaces.Unsigned_32 := 1;
   Request_System_Track_Data_Kind : constant Interfaces.Unsigned_32 := 2;
   Candidate_Object_Message_Kind  : constant Interfaces.Unsigned_32 := 3;

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
         return (if Result'Length = 0 then "native IR Track metadata operation failed" else Result);
      end;
   end Message;

   function To_NED (Raw : C.North_East_Down_V1) return North_East_Down
   is (North => Long_Float (Raw.North),
       East  => Long_Float (Raw.East),
       Down  => Long_Float (Raw.Down));

   --  Fails closed on an unrecognized event kind and on any provider enum
   --  value outside the upstream range. Unchecked enumeration conversion is
   --  deliberately not used for either enumeration.
   --  Complete verbatim copy of the @Optional RequestSystemTrackData. Upstream
   --  declares no enum and no constrained field, so there is nothing to
   --  validate: every provider value is well formed.
   function Copy_Request (Raw : C.IR_Request_System_Track_Data_V1) return Request_System_Track_Data
   is (System_Time_NS => Long_Long_Integer (Raw.System_Time_NS),
       Command_ID     => Raw.Command_ID,
       Request_ID     => Raw.Request_ID,
       Track_ID       => Raw.Track_ID);

   function Copy_Event (Raw : C.IR_Track_Event_V1) return IRST_Track_Report is
   begin
      if Raw.Kind /= Irst_Track_Report_Kind then
         raise Provider_Error with "invalid native IR Track metadata kind";
      end if;
      if Raw.Track_Report.State > IRST_Track_State'Enum_Rep (Dropped) then
         raise Provider_Error with "native IR Track returned unknown IrstTrackState";
      end if;
      if Raw.Track_Report.Mode > IRST_Track_Mode'Enum_Rep (Stare) then
         raise Provider_Error with "native IR Track returned unknown IrstTrackMode";
      end if;
      return
        (System_Time_NS     => Long_Long_Integer (Raw.Track_Report.System_Time_NS),
         Activity_ID        => Raw.Track_Report.Activity_ID,
         Measured_NED       => To_NED (Raw.Track_Report.Measured_NED),
         Measured_Intensity => Long_Float (Raw.Track_Report.Measured_Intensity),
         Measured_SNR       => Long_Float (Raw.Track_Report.Measured_SNR),
         Filtered_NED       => To_NED (Raw.Track_Report.Filtered_NED),
         Filtered_Intensity => Long_Float (Raw.Track_Report.Filtered_Intensity),
         Filtered_SNR       => Long_Float (Raw.Track_Report.Filtered_SNR),
         Range_M            => Long_Float (Raw.Track_Report.Range_M),
         Range_Error_M      => Long_Float (Raw.Track_Report.Range_Error_M),
         Spatial_Extent_Rad => Long_Float (Raw.Track_Report.Spatial_Extent_Rad),
         Track_Quality      => Long_Float (Raw.Track_Report.Track_Quality),
         Clutter            => Long_Float (Raw.Track_Report.Clutter),
         Age_NS             => Long_Long_Integer (Raw.Track_Report.Age_NS),
         State              => IRST_Track_State'Enum_Val (Raw.Track_Report.State),
         Mode               => IRST_Track_Mode'Enum_Val (Raw.Track_Report.Mode));
   end Copy_Event;

   function To_Directional (Raw : C.IR_Directional_V1) return Directional
   is (X => Long_Float (Raw.X), Y => Long_Float (Raw.Y), Z => Long_Float (Raw.Z));

   function To_Quaternion (Raw : C.IR_Quaternion_V1) return Quaternion
   is (X => Long_Float (Raw.X),
       Y => Long_Float (Raw.Y),
       Z => Long_Float (Raw.Z),
       W => Long_Float (Raw.W));

   function To_Inertial_State (Raw : C.IR_Sensor_Inertial_State_V1) return Sensor_Inertial_State
   is (System_Time_NS  => Long_Long_Integer (Raw.System_Time_NS),
       Q_XYZW          => To_Quaternion (Raw.Q_XYZW),
       Q_ECEF_XYZW     => To_Quaternion (Raw.Q_ECEF_XYZW),
       Sensor_Position => To_Directional (Raw.Sensor_Position),
       Sensor_Velocity => To_Directional (Raw.Sensor_Velocity),
       Uncertainties   =>
         (Sensor_Uncertainties   => Raw.Uncertainties.Sensor_Uncertainties,
          Platform_Uncertainties => Raw.Uncertainties.Platform_Uncertainties));

   --  Copies the complete CandidateObjectMessage into wholly Ada-owned
   --  storage. Both native spans are read here and never retained; the caller
   --  closes the native event immediately afterwards.
   --
   --  The candidate span size is already exactly numberOfCOs: the native
   --  adapter exposes only the meaningful prefix of the upstream fixed
   --  900-entry array, so no trailing storage slot is ever visible here.
   --
   --  The hot-region enum was already validated natively against the upstream
   --  0..3 range; the check is repeated rather than using unchecked
   --  enumeration conversion.
   function Copy_Candidate_Message
     (Raw : C.IR_Candidate_Object_Message_V1) return Candidate_Object_Message
   is
      type Region_Array is array (C.Size_T range <>) of aliased C.IR_Hot_Region_V1
      with Convention => C;
      type Object_Array is array (C.Size_T range <>) of aliased C.IR_Candidate_Object_V1
      with Convention => C;
      Result : Candidate_Object_Message;
      use type C.Size_T;
   begin
      Result.Header_Value :=
        (Number_Of_COs          => Raw.Header.Number_Of_COs,
         Stack_Frame_Index      => Raw.Header.Stack_Frame_Index,
         CFAR                   => Float (Raw.Header.CFAR),
         Validity_Flag_Bitfield => Raw.Header.Validity_Flag_Bitfield,
         TOV_UTC_NS             => Long_Long_Integer (Raw.Header.TOV_UTC_NS));
      Result.Inertial := To_Inertial_State (Raw.Inertial_State);

      if Raw.Hot_Regions.Size > 0 then
         declare
            Regions : Region_Array (0 .. Raw.Hot_Regions.Size - 1)
            with Import, Address => Raw.Hot_Regions.Data;
         begin
            for Index in Regions'Range loop
               if Regions (Index).Kind > Hot_Region_Type'Enum_Rep (Mask) then
                  raise Provider_Error with "native IR Track returned unknown HotRegionType";
               end if;
               Result.Regions.Append
                 (Hot_Region'
                    (Kind   => Hot_Region_Type'Enum_Val (Regions (Index).Kind),
                     Size   => Regions (Index).Size,
                     Top    => Regions (Index).Top,
                     Left   => Regions (Index).Left,
                     Right  => Regions (Index).Right,
                     Bottom => Regions (Index).Bottom));
            end loop;
         end;
      end if;

      if Raw.Candidate_Objects.Size > 0 then
         declare
            Objects : Object_Array (0 .. Raw.Candidate_Objects.Size - 1)
            with Import, Address => Raw.Candidate_Objects.Data;
         begin
            for Index in Objects'Range loop
               Result.Candidate_Objects.Append
                 (Candidate_Object'
                    (System_Time_NS               =>
                       Long_Long_Integer (Objects (Index).System_Time_NS),
                     Detection_Category           => Objects (Index).Detection_Category,
                     Sensor_Index                 => Objects (Index).Sensor_Index,
                     Subpixel                     =>
                       (Row    => Long_Float (Objects (Index).Subpixel.Row),
                        Column => Long_Float (Objects (Index).Subpixel.Column)),
                     Intensity                    => Long_Float (Objects (Index).Intensity),
                     Sensor_Relative_Unit         =>
                       To_Directional (Objects (Index).Sensor_Relative_Unit),
                     Signal_To_Interference_Ratio =>
                       Long_Float (Objects (Index).Signal_To_Interference_Ratio),
                     Signal_To_Noise_Ratio        =>
                       Long_Float (Objects (Index).Signal_To_Noise_Ratio)));
            end loop;
         end;
      end if;
      return Result;
   end Copy_Candidate_Message;

   function Header (Message : Candidate_Object_Message) return Candidate_Object_Header
   is (Message.Header_Value);

   function Inertial_State (Message : Candidate_Object_Message) return Sensor_Inertial_State
   is (Message.Inertial);

   function Hot_Region_Count (Message : Candidate_Object_Message) return Natural
   is (Natural (Message.Regions.Length));

   function Hot_Region_At (Message : Candidate_Object_Message; Index : Positive) return Hot_Region
   is (Message.Regions (Index));

   function Candidate_Object_Count (Message : Candidate_Object_Message) return Natural
   is (Natural (Message.Candidate_Objects.Length));

   function Candidate_Object_At
     (Message : Candidate_Object_Message; Index : Positive) return Candidate_Object
   is (Message.Candidate_Objects (Index));

   --  Fails closed on any kind this release does not implement, which is now
   --  only the unimplemented CandidateObjectPreProcMessage callback.
   function Copy_Any_Event (Raw : C.IR_Track_Event_V1) return Metadata_Event is
   begin
      if Raw.Kind = Irst_Track_Report_Kind then
         return (Kind => IRST_Track_Report_Event, Report => Copy_Event (Raw));
      elsif Raw.Kind = Request_System_Track_Data_Kind then
         return
           (Kind    => Request_System_Track_Data_Event,
            Request => Copy_Request (Raw.Request_System_Track_Data));
      elsif Raw.Kind = Candidate_Object_Message_Kind then
         return
           (Kind       => Candidate_Object_Message_Event,
            Candidates => Copy_Candidate_Message (Raw.Candidate_Object_Message));
      else
         raise Provider_Error with "invalid native IR Track metadata kind";
      end if;
   end Copy_Any_Event;

   function Open (Channel : Track_Channel; Queue_Capacity : Positive := 16) return Metadata_Channel
   is
      D        : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      Required : aliased C.Size_T := 0;
   begin
      if Channel.Handle = C.Null_Track then
         raise Provider_Error with "Track channel is closed";
      end if;
      return Result : Metadata_Channel do
         if C.IR_Track_Metadata_Open
              (Channel.Handle,
               C.Size_T (Queue_Capacity),
               Result.Handle'Access,
               D'Address,
               D'Length,
               Required'Access)
           /= C.Success
         then
            raise Provider_Error with Message (D);
         end if;
      end return;
   end Open;

   function Is_Open (Stream : Metadata_Channel) return Boolean
   is (Stream.Handle /= C.Null_Track_Metadata);

   function Receive
     (Stream : Metadata_Channel; Timeout_Milliseconds : Natural := 0) return IRST_Track_Report
   is
      Event : constant Metadata_Event := Receive_Event (Stream, Timeout_Milliseconds);
   begin
      if Event.Kind /= IRST_Track_Report_Event then
         raise Provider_Error
           with "native IR Track returned a non-report metadata event; " & "use Receive_Event";
      end if;
      return Event.Report;
   end Receive;

   function Receive_Event
     (Stream : Metadata_Channel; Timeout_Milliseconds : Natural := 0) return Metadata_Event
   is
      Owner    : aliased C.Track_Event_Handle := C.Null_Track_Event;
      Address  : aliased System.Address := System.Null_Address;
      D        : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      Required : aliased C.Size_T := 0;
      Code     : Interfaces.Integer_32;
      procedure Release is
         Ignored : Interfaces.Integer_32;
      begin
         Ignored := C.IR_Track_Event_Close (Owner'Access, System.Null_Address, 0, null);
      end Release;
   begin
      Code :=
        C.IR_Track_Metadata_Receive
          (Stream.Handle,
           Interfaces.Unsigned_32 (Timeout_Milliseconds),
           Owner'Access,
           D'Address,
           D'Length,
           Required'Access);
      if Code = C.Timeout then
         raise Timeout_Error;
      elsif Code = C.Stream_Stopped then
         raise Stream_Stopped;
      elsif Code /= C.Success then
         raise Provider_Error with Message (D);
      end if;
      if C.IR_Track_Event_View (Owner, Address'Access, D'Address, D'Length, Required'Access)
        /= C.Success
      then
         Release;
         raise Provider_Error with Message (D);
      end if;
      declare
         --  Every field is copied into Ada-owned storage before the native
         --  event owner is closed; no native pointer escapes.
         Result : constant Metadata_Event := Copy_Any_Event (To_Event (Address).all);
      begin
         Release;
         return Result;
      end;
   exception
      --  Any conversion failure still closes the native event owner.
      when others =>
         if Owner /= C.Null_Track_Event then
            Release;
         end if;
         raise;
   end Receive_Event;

   function Counters (Stream : Metadata_Channel) return Metadata_Counters is
      Raw      : aliased C.Metadata_Counters_V1 := (others => 0);
      D        : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      Required : aliased C.Size_T := 0;
   begin
      if C.IR_Track_Metadata_Get_Counters
           (Stream.Handle, Raw'Access, D'Address, D'Length, Required'Access)
        /= C.Success
      then
         raise Provider_Error with Message (D);
      end if;
      return
        (Counter (Raw.Events_Received),
         Counter (Raw.Events_Dropped_Queue_Full),
         Counter (Raw.Malformed_Or_Unsupported));
   end Counters;

   procedure Close (Stream : in out Metadata_Channel) is
      D        : aliased Fixed_Diagnostic := [others => Interfaces.C.nul];
      Required : aliased C.Size_T := 0;
   begin
      if C.IR_Track_Metadata_Close (Stream.Handle'Access, D'Address, D'Length, Required'Access)
        /= C.Success
      then
         raise Provider_Error with Message (D);
      end if;
   end Close;

   overriding
   procedure Finalize (Stream : in out Metadata_Channel) is
      Ignored : Interfaces.Integer_32;
   begin
      Ignored := C.IR_Track_Metadata_Close (Stream.Handle'Access, System.Null_Address, 0, null);
   exception
      when others =>
         Stream.Handle := C.Null_Track_Metadata;
   end Finalize;
end AMS.MEL.IR.Track.Metadata;
