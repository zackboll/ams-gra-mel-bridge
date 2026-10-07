with Interfaces;
with Interfaces.C;
with Interfaces.C.Strings;
with System;

private package AMS.MEL_C_API is
   pragma SPARK_Mode (Off);

   type Version_V1 is record
      Major : Interfaces.Unsigned_32;
      Minor : Interfaces.Unsigned_32;
   end record
   with Convention => C;

   Success            : constant Interfaces.Integer_32 := 0;
   Buffer_Too_Small   : constant Interfaces.Integer_32 := 7;
   Timeout            : constant Interfaces.Integer_32 := 9;
   Stream_Stopped     : constant Interfaces.Integer_32 := 10;
   Provider_Failed    : constant Interfaces.Integer_32 := 11;
   Command_Rejected   : constant Interfaces.Integer_32 := 12;
   Resource_Exhausted : constant Interfaces.Integer_32 := 13;

   type Session_Options_V1 is record
      Max_Async_Requests : Interfaces.Unsigned_32;
   end record
   with Convention => C;

   subtype Size_T is Interfaces.C.size_t;
   type Session_Handle is new System.Address;
   Null_Session                       : constant Session_Handle :=
     Session_Handle (System.Null_Address);
   type Stream_Handle is new System.Address;
   Null_Stream                        : constant Stream_Handle :=
     Stream_Handle (System.Null_Address);
   type Frame_Snapshot_Handle is new System.Address;
   Null_Frame_Snapshot                : constant Frame_Snapshot_Handle :=
     Frame_Snapshot_Handle (System.Null_Address);
   type C2_Handle is new System.Address;
   Null_C2                            : constant C2_Handle := C2_Handle (System.Null_Address);
   type Mode_Request_Handle is new System.Address;
   Null_Mode_Request                  : constant Mode_Request_Handle :=
     Mode_Request_Handle (System.Null_Address);
   type Return_Request_Handle is new System.Address;
   Null_Return_Request                : constant Return_Request_Handle :=
     Return_Request_Handle (System.Null_Address);
   type Comms_Request_Handle is new System.Address;
   Null_Comms_Request                 : constant Comms_Request_Handle :=
     Comms_Request_Handle (System.Null_Address);
   type Capability_Handle is new System.Address;
   Null_Capability                    : constant Capability_Handle :=
     Capability_Handle (System.Null_Address);
   --  Task 032B1: raw weak common Channel view. Private FFI only; no safe
   --  Ada Channel operations use it yet (Task 032B2).
   type Channel_Handle is new System.Address;
   Null_Channel                       : constant Channel_Handle :=
     Channel_Handle (System.Null_Address);
   type Metadata_Handle is new System.Address;
   Null_Metadata                      : constant Metadata_Handle :=
     Metadata_Handle (System.Null_Address);
   type Metadata_Event_Handle is new System.Address;
   Null_Metadata_Event                : constant Metadata_Event_Handle :=
     Metadata_Event_Handle (System.Null_Address);
   type Health_Handle is new System.Address;
   Null_Health                        : constant Health_Handle :=
     Health_Handle (System.Null_Address);
   type Health_Metadata_Handle is new System.Address;
   Null_Health_Metadata               : constant Health_Metadata_Handle :=
     Health_Metadata_Handle (System.Null_Address);
   type Health_Event_Handle is new System.Address;
   Null_Health_Event                  : constant Health_Event_Handle :=
     Health_Event_Handle (System.Null_Address);
   type Image_Metadata_Handle is new System.Address;
   Null_Image_Metadata                : constant Image_Metadata_Handle :=
     Image_Metadata_Handle (System.Null_Address);
   type Image_Metadata_Event_Handle is new System.Address;
   Null_Image_Metadata_Event          : constant Image_Metadata_Event_Handle :=
     Image_Metadata_Event_Handle (System.Null_Address);
   type Navigation_Request_Handle is new System.Address;
   Null_Navigation_Request            : constant Navigation_Request_Handle :=
     Navigation_Request_Handle (System.Null_Address);
   type Instrumentation_Handle is new System.Address;
   Null_Instrumentation               : constant Instrumentation_Handle :=
     Instrumentation_Handle (System.Null_Address);
   type Instrumentation_Request_Handle is new System.Address;
   Null_Instrumentation_Request       : constant Instrumentation_Request_Handle :=
     Instrumentation_Request_Handle (System.Null_Address);
   type Instrumentation_Metadata_Handle is new System.Address;
   Null_Instrumentation_Metadata      : constant Instrumentation_Metadata_Handle :=
     Instrumentation_Metadata_Handle (System.Null_Address);
   type Instrumentation_Event_Handle is new System.Address;
   Null_Instrumentation_Event         : constant Instrumentation_Event_Handle :=
     Instrumentation_Event_Handle (System.Null_Address);
   type Track_Handle is new System.Address;
   Null_Track                         : constant Track_Handle := Track_Handle (System.Null_Address);
   type Track_Metadata_Handle is new System.Address;
   Null_Track_Metadata                : constant Track_Metadata_Handle :=
     Track_Metadata_Handle (System.Null_Address);
   type Track_Event_Handle is new System.Address;
   Null_Track_Event                   : constant Track_Event_Handle :=
     Track_Event_Handle (System.Null_Address);
   type Track_Update_Request_Handle is new System.Address;
   Null_Track_Update_Request          : constant Track_Update_Request_Handle :=
     Track_Update_Request_Handle (System.Null_Address);
   --  A deliberately distinct handle type for the @Optional
   --  SystemTrackDataResponse request family.
   type Track_System_Response_Request_Handle is new System.Address;
   Null_Track_System_Response_Request : constant Track_System_Response_Request_Handle :=
     Track_System_Response_Request_Handle (System.Null_Address);
   --  Private raw RF handles; safe Ada ownership lives in AMS.MEL.RF children.
   type RF_Data_Handle is new System.Address;
   type RF_Admin_Handle is new System.Address;
   type RF_C2_Handle is new System.Address;
   Null_RF_C2                         : constant RF_C2_Handle := RF_C2_Handle (System.Null_Address);
   type RF_VA_Request_Handle is new System.Address;
   Null_RF_VA_Request                 : constant RF_VA_Request_Handle :=
     RF_VA_Request_Handle (System.Null_Address);
   type RF_VA_Handle is new System.Address;
   Null_RF_VA                         : constant RF_VA_Handle := RF_VA_Handle (System.Null_Address);
   type RF_Job_Request_Handle is new System.Address;
   Null_RF_Job_Request                : constant RF_Job_Request_Handle :=
     RF_Job_Request_Handle (System.Null_Address);
   type RF_Job_Handle is new System.Address;
   Null_RF_Job                        : constant RF_Job_Handle :=
     RF_Job_Handle (System.Null_Address);
   Null_RF_Admin                      : constant RF_Admin_Handle :=
     RF_Admin_Handle (System.Null_Address);
   Null_RF_Data                       : constant RF_Data_Handle :=
     RF_Data_Handle (System.Null_Address);
   type RF_Physical_Data_Handle is new System.Address;
   Null_RF_Physical_Data              : constant RF_Physical_Data_Handle :=
     RF_Physical_Data_Handle (System.Null_Address);
   type RF_MFA_Info_Handle is new System.Address;
   Null_RF_MFA_Info                   : constant RF_MFA_Info_Handle :=
     RF_MFA_Info_Handle (System.Null_Address);
   --  Task 033D: raw RF ProductRxEndpoint owners. Private FFI only.
   type RF_Product_Rx_Request_Handle is new System.Address;
   Null_RF_Product_Rx_Request         : constant RF_Product_Rx_Request_Handle :=
     RF_Product_Rx_Request_Handle (System.Null_Address);
   type RF_Product_Rx_Handle is new System.Address;
   Null_RF_Product_Rx                 : constant RF_Product_Rx_Handle :=
     RF_Product_Rx_Handle (System.Null_Address);
   type RF_Product_Rx_Event_Handle is new System.Address;
   Null_RF_Product_Rx_Event           : constant RF_Product_Rx_Event_Handle :=
     RF_Product_Rx_Event_Handle (System.Null_Address);

   type Byte_Array_16 is array (0 .. 15) of Interfaces.Unsigned_8 with Convention => C;
   type String_View_V1 is record
      Data : System.Address;
      Size : Size_T;
   end record
   with Convention => C;
   type UCI_ID_V1 is record
      UUID              : Byte_Array_16;
      Descriptive_Label : String_View_V1;
   end record
   with Convention => C;
   type Span_V1 is record
      Data : System.Address;
      Size : Size_T;
   end record
   with Convention => C;
   type Component_Location_V1 is record
      Offset_X_M  : Interfaces.C.double;
      Offset_Y_M  : Interfaces.C.double;
      Offset_Z_M  : Interfaces.C.double;
      Key         : String_View_V1;
      System_Name : String_View_V1;
   end record
   with Convention => C;
   type IR_Health_Config_V1 is record
      Channel_ID      : UCI_ID_V1;
      Channel_Type    : Interfaces.Unsigned_32;
      Platform_ID     : UCI_ID_V1;
      Sensor_Location : Component_Location_V1;
   end record
   with Convention => C;
   type IR_Instrumentation_Config_V1 is record
      Channel_ID      : UCI_ID_V1;
      Channel_Type    : Interfaces.Unsigned_32;
      Platform_ID     : UCI_ID_V1;
      Sensor_Location : Component_Location_V1;
   end record
   with Convention => C;
   type IR_Track_Config_V1 is record
      Channel_ID      : UCI_ID_V1;
      Channel_Type    : Interfaces.Unsigned_32;
      Platform_ID     : UCI_ID_V1;
      Sensor_Location : Component_Location_V1;
   end record
   with Convention => C;
   type IR_Instrumentation_Report_V1 is record
      Command_ID   : Interfaces.Unsigned_32;
      Size         : Interfaces.Unsigned_32;
      Timestamp_NS : Interfaces.Integer_64;
      Priority     : Interfaces.Unsigned_32;
   end record
   with Convention => C;
   type IR_Instrumentation_Level_Command_V1 is record
      Command_ID : Interfaces.Unsigned_32;
      Priority   : Interfaces.Unsigned_32;
   end record
   with Convention => C;
   type IR_Instrumentation_Result_V1 is record
      Report     : IR_Instrumentation_Report_V1;
      Error_Code : Interfaces.Unsigned_32;
   end record
   with Convention => C;
   type IR_Instrumentation_Event_V1 is record
      Kind   : Interfaces.Unsigned_32;
      Report : IR_Instrumentation_Report_V1;
   end record
   with Convention => C;
   type Euler_V1 is record
      Roll, Pitch, Yaw : Interfaces.C.double;
   end record
   with Convention => C;
   type Foreign_Key_V1 is record
      Key, System_Name : String_View_V1;
   end record
   with Convention => C;
   type Installation_Details_V1 is record
      Location               : Component_Location_V1;
      Orientation, Boresight : Euler_V1;
   end record
   with Convention => C;
   type Temperature_Status_V1 is record
      Temperature_C : Interfaces.C.double;
      State         : Interfaces.Unsigned_32;
   end record
   with Convention => C;
   type MFA_Component_V1 is record
      Component_ID             : UCI_ID_V1;
      State                    : Interfaces.Unsigned_32;
      Temperature              : Temperature_Status_V1;
      Installation_Location_ID : Foreign_Key_V1;
      Installation_Details     : Installation_Details_V1;
   end record
   with Convention => C;
   type About_V1 is record
      Model, Serial_Number, Software_Version        : String_View_V1;
      Bootloader_Software_Version, Hardware_Version : String_View_V1;
   end record
   with Convention => C;
   type MFA_Status_V1 is record
      State                               : Interfaces.Unsigned_32;
      State_Description, Mode_Description : String_View_V1;
      Transition_Status                   : Interfaces.Unsigned_32;
      About_Data                          : About_V1;
      Components                          : Span_V1;
   end record
   with Convention => C;
   type IR_Subsystem_Dep_Info_V1 is record
      Subsystem_ID, Criticality, Failure : Interfaces.Unsigned_32;
   end record
   with Convention => C;
   type IR_Version_V1 is record
      Source, Major_Revision, Minor_Revision, Engineering_Revision : Interfaces.Unsigned_32;
   end record
   with Convention => C;
   type IR_Subsystem_CSCI_Info_V1 is record
      CSCI                                                     : String_View_V1;
      Mode                                                     : Interfaces.Unsigned_32;
      Version                                                  : IR_Version_V1;
      Criticality, Failure, BIT_Report, Connection_Established : Interfaces.Unsigned_32;
   end record
   with Convention => C;
   type IR_Subsystem_Status_V1 is record
      Subsystem_ID, Criticality, Status_Sequence_Number, Failure : Interfaces.Unsigned_32;
      Subsystem_Count                                            : Interfaces.Unsigned_32;
      Subsystems                                                 : Span_V1;
      CSCI_Count                                                 : Interfaces.Unsigned_32;
      CSCI                                                       : Span_V1;
   end record
   with Convention => C;
   type Name_Value_Pair_V1 is record
      Name, Value : String_View_V1;
   end record
   with Convention => C;
   type Security_Artifact_V1 is record
      Component_ID, Associated_ID : UCI_ID_V1;
   end record
   with Convention => C;
   type Security_Event_V1 is record
      Kind, Category                   : Interfaces.Unsigned_32;
      Details                          : String_View_V1;
      Subsystem_ID, Service_ID, MDF_ID : UCI_ID_V1;
   end record
   with Convention => C;
   type Security_Audit_Record_V1 is record
      Security_Event_ID  : UCI_ID_V1;
      Event_Timestamp_NS : Interfaces.Integer_64;
      Subsystem_ID       : UCI_ID_V1;
      Artifacts          : Span_V1;
      Event              : Security_Event_V1;
      Outcome, Severity  : Interfaces.Unsigned_32;
   end record
   with Convention => C;
   type IR_Command_Status_V1 is record
      Command_ID         : Interfaces.Unsigned_32;
      State, Reason_ID   : Interfaces.Unsigned_32;
      Reason_Description : String_View_V1;
   end record
   with Convention => C;
   type BIT_Type_V1 is record
      BIT_ID                                  : UCI_ID_V1;
      Accepted_Interface                      : Interfaces.Unsigned_32;
      BIT_Item_Names, Subsystem_Component_IDs : Span_V1;
      Expected_Duration_NS                    : Interfaces.Integer_64;
   end record
   with Convention => C;
   type BIT_Configuration_V1 is record
      BIT_Types : Span_V1;
   end record
   with Convention => C;
   type Active_BIT_V1 is record
      BIT_ID                       : UCI_ID_V1;
      Estimated_Completion_Time_NS : Interfaces.Integer_64;
      Estimated_Percent_Complete   : Interfaces.C.double;
   end record
   with Convention => C;
   type Completed_BIT_Item_V1 is record
      BIT_Item_Name : String_View_V1;
      Result        : Interfaces.Unsigned_32;
      Fail_Reason   : String_View_V1;
   end record
   with Convention => C;
   type Completed_BIT_V1 is record
      BIT_ID      : UCI_ID_V1;
      Time_Tag_NS : Interfaces.Integer_64;
      Result      : Interfaces.Unsigned_32;
      Fail_Reason : String_View_V1;
      BIT_Items   : Span_V1;
   end record
   with Convention => C;
   type Fault_Data_V1 is record
      Key, Value, Format, Units : String_View_V1;
   end record
   with Convention => C;
   type Fault_Ambiguity_Group_V1 is record
      Diagnostic_Test_IDs, Component_IDs : Span_V1;
   end record
   with Convention => C;
   type Fault_V1 is record
      Fault_ID                        : UCI_ID_V1;
      Severity, State                 : Interfaces.Unsigned_32;
      Fault_Data                      : Span_V1;
      Detection_Time_NS               : Interfaces.Integer_64;
      Fault_Code, Fault_Description   : String_View_V1;
      Component_IDs, Ambiguity_Groups : Span_V1;
   end record
   with Convention => C;
   type BIT_Status_V1 is record
      Active_BITS, Completed_BITS, Faults : Span_V1;
   end record
   with Convention => C;
   type IR_Health_Event_V1 is record
      Kind                : Interfaces.Unsigned_32;
      MFA_Status          : MFA_Status_V1;
      BIT_Status          : BIT_Status_V1;
      Subsystem_Status    : IR_Subsystem_Status_V1;
      Discrete_Status     : Span_V1;
      Security_Audit      : Security_Audit_Record_V1;
      MFA_Status_Detailed : Span_V1;
   end record
   with Convention => C;
   type IR_Channel_Comms_Test_Report_V1 is record
      Command_ID, Request_ID : Interfaces.Unsigned_32;
   end record
   with Convention => C;
   type Metadata_Event_V1 is record
      Kind               : Interfaces.Unsigned_32;
      Command_Status     : IR_Command_Status_V1;
      BIT_Configuration  : BIT_Configuration_V1;
      BIT_Status         : BIT_Status_V1;
      Channel_Comms_Test : IR_Channel_Comms_Test_Report_V1;
   end record
   with Convention => C;
   type Metadata_Counters_V1 is record
      Events_Received, Events_Dropped_Queue_Full, Malformed_Or_Unsupported : Interfaces.Unsigned_64;
   end record
   with Convention => C;
   type IR_Bad_Pixel_V1 is record
      Row, Column, Reason : Interfaces.Unsigned_32;
   end record
   with Convention => C;
   type IR_Bad_Pixel_Span_V1 is record
      Data : System.Address;
      Size : Size_T;
   end record
   with Convention => C;
   type IR_Bad_Pixel_List_V1 is record
      Reported_Size, Reported_Count : Interfaces.Unsigned_32;
      Pixels                        : IR_Bad_Pixel_Span_V1;
   end record
   with Convention => C;
   type Az_El_V1 is record
      Azimuth_Rad, Elevation_Rad : Interfaces.C.double;
   end record
   with Convention => C;
   type IR_Line_Of_Sight_Report_V1 is record
      System_Time_NS                       : Interfaces.Integer_64;
      Pointing_Angle, Pointing_Angle_Rates : Az_El_V1;
      At_Speed, In_Tolerance               : Interfaces.Unsigned_8;
      Platform_Attitude                    : Euler_V1;
      Validity_Flag_Bitfield               : Interfaces.Unsigned_32;
      Image_Rotation_Rad                   : Interfaces.C.double;
   end record
   with Convention => C;
   type IR_Line_Of_Sight_Euler_V1 is record
      System_Time_NS           : Interfaces.Integer_64;
      Attitude, Attitude_Rates : Euler_V1;
   end record
   with Convention => C;
   type IR_Navigation_Response_V1 is record
      System_Time_NS         : Interfaces.Integer_64;
      Command_ID, Request_ID : Interfaces.Unsigned_32;
   end record
   with Convention => C;
   type North_East_Down_V1 is record
      North, East, Down : Interfaces.C.double;
   end record
   with Convention => C;
   --  Complete IRSTTrackReport. Reuses the one canonical North_East_Down_V1
   --  import; every C double stays Interfaces.C.double here and is converted
   --  only in the safe layer.
   type IR_Track_Report_V1 is record
      System_Time_NS     : Interfaces.Integer_64;
      Activity_ID        : Interfaces.Unsigned_32;
      Measured_NED       : North_East_Down_V1;
      Measured_Intensity : Interfaces.C.double;
      Measured_SNR       : Interfaces.C.double;
      Filtered_NED       : North_East_Down_V1;
      Filtered_Intensity : Interfaces.C.double;
      Filtered_SNR       : Interfaces.C.double;
      Range_M            : Interfaces.C.double;
      Range_Error_M      : Interfaces.C.double;
      Spatial_Extent_Rad : Interfaces.C.double;
      Track_Quality      : Interfaces.C.double;
      Clutter            : Interfaces.C.double;
      Age_NS             : Interfaces.Integer_64;
      State              : Interfaces.Unsigned_32;
      Mode               : Interfaces.Unsigned_32;
   end record
   with Convention => C;
   --  Upstream RequestSystemTrackData is an inbound @Optional request carried
   --  by the Track metadata callback; TrackChannel declares no matching send.
   --  systemTime is std::chrono::nanoseconds, whose representation is signed.
   type IR_Request_System_Track_Data_V1 is record
      System_Time_NS : Interfaces.Integer_64;
      Command_ID     : Interfaces.Unsigned_32;
      Request_ID     : Interfaces.Unsigned_32;
      Track_ID       : Interfaces.Unsigned_32;
   end record
   with Convention => C;
   --  The canonical IR XYZ, quaternion, uncertainty, and SensorInertialState
   --  imports. They are declared here, ahead of the Track metadata event, so
   --  the CandidateObjectMessage import can reuse them exactly as the C header
   --  does. No layout changed and no duplicate exists.
   type IR_Directional_V1 is record
      X, Y, Z : Interfaces.C.double;
   end record
   with Convention => C;
   type IR_Quaternion_V1 is record
      X, Y, Z, W : Interfaces.C.double;
   end record
   with Convention => C;
   type IR_Uncertainty_V1 is record
      Sensor_Uncertainties, Platform_Uncertainties : Interfaces.Unsigned_32;
   end record
   with Convention => C;
   type IR_Sensor_Inertial_State_V1 is record
      System_Time_NS                   : Interfaces.Integer_64;
      Q_XYZW, Q_ECEF_XYZW              : IR_Quaternion_V1;
      Sensor_Position, Sensor_Velocity : IR_Directional_V1;
      Uncertainties                    : IR_Uncertainty_V1;
   end record
   with Convention => C;

   --  Complete HotRegion. The enum representation and the uint16_t geometry
   --  are imported at their exact C widths.
   type IR_Hot_Region_V1 is record
      Kind                           : Interfaces.Unsigned_32;
      Size, Top, Left, Right, Bottom : Interfaces.Unsigned_16;
   end record
   with Convention => C;
   type IR_Hot_Region_Span_V1 is record
      Data : System.Address;
      Size : Size_T;
   end record
   with Convention => C;

   --  Complete CandidateObjectHeader. CFAR is upstream float and is imported
   --  as Interfaces.C.C_float; it is deliberately NOT widened to double.
   type IR_Candidate_Object_Header_V1 is record
      Number_Of_COs          : Interfaces.Unsigned_16;
      Stack_Frame_Index      : Interfaces.Unsigned_16;
      CFAR                   : Interfaces.C.C_float;
      Validity_Flag_Bitfield : Interfaces.Unsigned_16;
      TOV_UTC_NS             : Interfaces.Integer_64;
   end record
   with Convention => C;

   --  The one canonical row/column import, matching upstream RowCol.
   type IR_Row_Col_V1 is record
      Row, Column : Interfaces.C.double;
   end record
   with Convention => C;

   --  Complete CandidateObject; reuses the canonical row/column and XYZ
   --  imports.
   type IR_Candidate_Object_V1 is record
      System_Time_NS               : Interfaces.Integer_64;
      Detection_Category           : Interfaces.Unsigned_32;
      Sensor_Index                 : Interfaces.Unsigned_32;
      Subpixel                     : IR_Row_Col_V1;
      Intensity                    : Interfaces.C.double;
      Sensor_Relative_Unit         : IR_Directional_V1;
      Signal_To_Interference_Ratio : Interfaces.C.double;
      Signal_To_Noise_Ratio        : Interfaces.C.double;
   end record
   with Convention => C;
   type IR_Candidate_Object_Span_V1 is record
      Data : System.Address;
      Size : Size_T;
   end record
   with Convention => C;

   --  Complete CandidateObjectMessage. Both spans borrow storage owned by the
   --  native event owner; the safe layer copies everything before close.
   type IR_Candidate_Object_Message_V1 is record
      Header            : IR_Candidate_Object_Header_V1;
      Inertial_State    : IR_Sensor_Inertial_State_V1;
      Hot_Regions       : IR_Hot_Region_Span_V1;
      Candidate_Objects : IR_Candidate_Object_Span_V1;
   end record
   with Convention => C;

   --  FROZEN. This imports the permanently frozen C
   --  ams_mel_ir_track_metadata_event_v1 record, which has exactly these three
   --  members. Nothing may be appended to it again; later Track metadata
   --  payloads get a new version record.
   type IR_Track_Event_V1 is record
      Kind                      : Interfaces.Unsigned_32;
      Track_Report              : IR_Track_Report_V1;
      Request_System_Track_Data : IR_Request_System_Track_Data_V1;
   end record
   with Convention => C;

   --  FROZEN. Track metadata event v2: the complete frozen v1 record first,
   --  then the additive CandidateObjectMessage payload. Base.Kind stays the
   --  one discriminator for every kind. Exactly these two members; the
   --  CandidateObjectPreProcMessage payload went into v3 instead.
   type IR_Track_Event_V2 is record
      Base                     : IR_Track_Event_V1;
      Candidate_Object_Message : IR_Candidate_Object_Message_V1;
   end record
   with Convention => C;

   --  The one explicit fixed import of the upstream 3 by 3 background patch.
   --  It is a flat nine-element C-compatible array in ROW-MAJOR order:
   --  Samples (Row * 3 + Column) is upstream [Row][Column]. The safe layer
   --  maps it explicitly to an owned 3x3 Ada value.
   subtype IR_Candidate_Background_Index is Size_T range 0 .. 8;
   type IR_Candidate_Background_Samples is
     array (IR_Candidate_Background_Index) of aliased Interfaces.Integer_16
   with Convention => C;
   type IR_Candidate_Background_V1 is record
      Samples : IR_Candidate_Background_Samples;
   end record
   with Convention => C;

   --  Complete CandidateObjectPreProc; reuses the canonical row/column, XYZ,
   --  and SensorInertialState imports. Each entry carries its OWN nested
   --  inertial state. Edge is the C uint8_t, always exactly 0 or 1.
   type IR_Candidate_Object_PreProc_V1 is record
      System_Time_NS                   : Interfaces.Integer_64;
      Detection_Category               : Interfaces.Unsigned_32;
      Sensor_Index                     : Interfaces.Unsigned_32;
      Subpixel                         : IR_Row_Col_V1;
      Intensity                        : Interfaces.C.double;
      Sensor_Relative_Unit             : IR_Directional_V1;
      Signal_To_Interference_Ratio     : Interfaces.C.double;
      Signal_To_Noise_Ratio            : Interfaces.C.double;
      Candidate_Object_With_Background : IR_Candidate_Background_V1;
      Clutter                          : Interfaces.C.double;
      Candidate_Object_Quality         : Interfaces.C.double;
      Sir_Delta                        : Interfaces.C.double;
      Inertial_State                   : IR_Sensor_Inertial_State_V1;
      Edge                             : Interfaces.Unsigned_8;
      Az_Sigma                         : Interfaces.C.double;
      El_Sigma                         : Interfaces.C.double;
      Background_Normalizer            : Interfaces.C.double;
   end record
   with Convention => C;
   type IR_Candidate_Object_PreProc_Span_V1 is record
      Data : System.Address;
      Size : Size_T;
   end record
   with Convention => C;

   --  Complete CandidateObjectPreProcMessage. Both spans borrow storage owned
   --  by the native event owner; the safe layer copies everything before
   --  close. The PreProc span size is the upstream vector's own size and is
   --  deliberately NOT truncated to Header.Number_Of_COs.
   type IR_Candidate_Object_PreProc_Message_V1 is record
      Header                    : IR_Candidate_Object_Header_V1;
      Inertial_State            : IR_Sensor_Inertial_State_V1;
      Hot_Regions               : IR_Hot_Region_Span_V1;
      Candidate_Object_PreProcs : IR_Candidate_Object_PreProc_Span_V1;
   end record
   with Convention => C;

   --  Track metadata event v3: the complete frozen v2 record first, then the
   --  additive CandidateObjectPreProcMessage payload. Base.Base.Kind stays the
   --  one discriminator for every kind.
   type IR_Track_Event_V3 is record
      Base                             : IR_Track_Event_V2;
      Candidate_Object_PreProc_Message : IR_Candidate_Object_PreProc_Message_V1;
   end record
   with Convention => C;
   type Attitude_Rate_V1 is record
      Attitude_Rate         : Euler_V1;
      Attitude_Rate_Time_NS : Interfaces.Integer_64;
   end record
   with Convention => C;
   type Position_Velocity_Covariance_V1 is record
      Position_Position_Pn_Pn : Interfaces.C.double;
      Position_Position_Pn_Pe : Interfaces.C.double;
      Position_Position_Pn_Pd : Interfaces.C.double;
      Position_Position_Pe_Pe : Interfaces.C.double;
      Position_Position_Pe_Pd : Interfaces.C.double;
      Position_Position_Pd_Pd : Interfaces.C.double;
      Position_Velocity_Pn_Vn : Interfaces.C.double;
      Position_Velocity_Pn_Ve : Interfaces.C.double;
      Position_Velocity_Pn_Vd : Interfaces.C.double;
      Position_Velocity_Pe_Ve : Interfaces.C.double;
      Position_Velocity_Pe_Vd : Interfaces.C.double;
      Position_Velocity_Pd_Vd : Interfaces.C.double;
      Velocity_Velocity_Vn_Vn : Interfaces.C.double;
      Velocity_Velocity_Vn_Ve : Interfaces.C.double;
      Velocity_Velocity_Vn_Vd : Interfaces.C.double;
      Velocity_Velocity_Ve_Ve : Interfaces.C.double;
      Velocity_Velocity_Ve_Vd : Interfaces.C.double;
      Velocity_Velocity_Vd_Vd : Interfaces.C.double;
   end record
   with Convention => C;
   type Navigation_Report_V1 is record
      System_Time_NS                           : Interfaces.Integer_64;
      State                                    : Interfaces.Unsigned_32;
      Latitude_Rad                             : Interfaces.C.double;
      Longitude_Rad                            : Interfaces.C.double;
      Altitude_M                               : Interfaces.C.double;
      Attitude                                 : Euler_V1;
      Attitude_Rate                            : Attitude_Rate_V1;
      Speed                                    : North_East_Down_V1;
      Acceleration                             : North_East_Down_V1;
      Wander_Angle_Rad                         : Interfaces.C.double;
      Magnetic_Heading                         : Interfaces.C.double;
      Altitude_MSL                             : Interfaces.C.double;
      Position_Velocity_Covariance_Uncertainty : Position_Velocity_Covariance_V1;
   end record
   with Convention => C;
   type Navigation_Result_V1 is record
      Response   : IR_Navigation_Response_V1;
      Error_Code : Interfaces.Unsigned_32;
   end record
   with Convention => C;
   type IR_Image_Metadata_Event_V1 is record
      Kind                 : Interfaces.Unsigned_32;
      Bad_Pixel_List       : IR_Bad_Pixel_List_V1;
      Line_Of_Sight_Report : IR_Line_Of_Sight_Report_V1;
      Line_Of_Sight_Euler  : IR_Line_Of_Sight_Euler_V1;
      Navigation_Response  : IR_Navigation_Response_V1;
   end record
   with Convention => C;
   type IR_Stream_Config_V1 is record
      Channel_Type    : Interfaces.Unsigned_32;
      Channel_ID      : UCI_ID_V1;
      Platform_ID     : UCI_ID_V1;
      Sensor_Location : Component_Location_V1;
      Buffer_Count    : Size_T;
      Buffer_Size     : Size_T;
      Queue_Capacity  : Size_T;
   end record
   with Convention => C;
   type IR_C2_Config_V1 is record
      Channel_Type    : Interfaces.Unsigned_32;
      Channel_ID      : UCI_ID_V1;
      Platform_ID     : UCI_ID_V1;
      Sensor_Location : Component_Location_V1;
   end record
   with Convention => C;
   type IR_Channel_Comms_Test_Request_V1 is record
      Command_ID, Channel_ID, Request_ID : Interfaces.Unsigned_32;
   end record
   with Convention => C;
   type IR_Channel_Comms_Test_Result_V1 is record
      Command_ID, Request_ID, Error_Code : Interfaces.Unsigned_32;
   end record
   with Convention => C;
   type IR_Band_Info_V1 is record
      Kind                               : Interfaces.Unsigned_32;
      Min_Wavelength_M, Max_Wavelength_M : Interfaces.C.double;
   end record
   with Convention => C;
   type IR_Image_Band_V1 is record
      Band_Index : Interfaces.Unsigned_32;
      Bands      : Span_V1;
   end record
   with Convention => C;
   type IR_Channel_Capability_V1 is record
      Channel_ID                                                                                  :
        UCI_ID_V1;
      Height, Width, Bit_Depth, Row_Pitch, Buffer_Size, Image_Size, Number_Of_Bands, Pixel_Format :
        Interfaces.Unsigned_32;
      Sensor_Types                                                                                :
        Span_V1;
      Platform_ID                                                                                 :
        UCI_ID_V1;
      Sensor_Location                                                                             :
        Component_Location_V1;
      Channel_Types                                                                               :
        Span_V1;
      Task_Schedule_Depth, ODC_Available, NUC_Available                                           :
        Interfaces.Unsigned_32;
      Metadata_Capabilities, Image_Bands, Nav_Frames                                              :
        Span_V1;
   end record
   with Convention => C;
   type IR_Mode_Result_V1 is record
      Mode       : Interfaces.Unsigned_32;
      Error_Code : Interfaces.Unsigned_32;
   end record
   with Convention => C;
   type IR_Return_Result_V1 is record
      Value      : Interfaces.Unsigned_32;
      Error_Code : Interfaces.Unsigned_32;
   end record
   with Convention => C;
   type U32_Span_V1 is record
      Data : System.Address;
      Size : Size_T;
   end record
   with Convention => C;
   type U64_Span_V1 is record
      Data : System.Address;
      Size : Size_T;
   end record
   with Convention => C;
   type String_View_Array is array (Positive range <>) of aliased String_View_V1
   with Convention => C;
   type String_View_Span_V1 is record
      Data : System.Address;
      Size : Size_T;
   end record
   with Convention => C;
   type RF_VA_Config_V1 is record
      VA_Definition_ID        : Interfaces.Unsigned_32;
      Priority                : Interfaces.Unsigned_32;
      Local_Function_Info     : String_View_Span_V1;
      VA_Definition_File_Info : String_View_V1;
      Capability_IDs          : Span_V1;
   end record
   with Convention => C;
   type RF_VA_Result_V1 is record
      Error_Code : Interfaces.Unsigned_32;
   end record
   with Convention => C;
   type RF_VA_Info_V1 is record
      VA_Instance_IDs      : Span_V1;
      Element_Group_Labels : String_View_Span_V1;
      Is_Single_Group      : Interfaces.Unsigned_32;
   end record
   with Convention => C;
   type IR_Scan_Type_V1 is record
      Continuous_Scan : Interfaces.Unsigned_32;
      Returning       : Interfaces.Unsigned_32;
      Agile_Scan      : Interfaces.Unsigned_32;
   end record
   with Convention => C;
   type IR_Scan_Param_V1 is record
      Elevation_Defined_With_Range_And_Altitude : Interfaces.Unsigned_32;
      Center_AZ_Rad                             : Interfaces.C.double;
      Center_EL_Rad                             : Interfaces.C.double;
      Center_Frame_Ref_EL                       : Interfaces.Unsigned_32;
      Center_Frame_Ref_AZ                       : Interfaces.Unsigned_32;
      Scan_Width_Rad                            : Interfaces.C.double;
      Scan_Height_Rad                           : Interfaces.C.double;
      Scan_Type                                 : IR_Scan_Type_V1;
      Scan_ID                                   : Interfaces.Unsigned_32;
      Scan_Rate_Rad_Per_Second                  : Interfaces.C.double;
      Preferred_Revisit_Interval_Seconds        : Interfaces.C.double;
      Required_Revisit_Interval_Seconds         : Interfaces.C.double;
      Max_Range_Of_Interest_M                   : Interfaces.Unsigned_32;
      Min_Range_Of_Interest_M                   : Interfaces.Unsigned_32;
      Elevation_Scan_Center_Altitude_M          : Interfaces.Unsigned_32;
      Elevation_Scan_Center_Range_M             : Interfaces.Unsigned_32;
      Degradation_Method                        : Interfaces.Unsigned_32;
   end record
   with Convention => C;
   type IR_Mode_Command_V1 is record
      Command_ID      : Interfaces.Unsigned_32;
      State           : Interfaces.Unsigned_32;
      Mode            : Interfaces.Unsigned_32;
      Scan_Parameters : IR_Scan_Param_V1;
   end record
   with Convention => C;
   type IR_BIT_Command_V1 is record
      Command_ID        : Interfaces.Unsigned_32;
      Initiate_BIT_IDs  : U32_Span_V1;
      Cancel_BIT_IDs    : U32_Span_V1;
      Clear_Fault_Codes : String_View_Span_V1;
   end record
   with Convention => C;
   type IR_Config_Set_Command_V1 is record
      Command_ID     : Interfaces.Unsigned_32;
      System_Time_NS : Interfaces.Integer_64;
      Config         : String_View_V1;
   end record
   with Convention => C;

   type Reserved_Byte_Array is array (0 .. 6) of Interfaces.Unsigned_8 with Convention => C;

   type IR_Frame_V1 is record
      System_Time_NS      : Interfaces.Integer_64;
      Integration_Time_NS : Interfaces.Integer_64;
      Width               : Interfaces.Unsigned_32;
      Height              : Interfaces.Unsigned_32;
      Bits_Per_Pixel      : Interfaces.Unsigned_32;
      Number_Of_Bands     : Interfaces.Unsigned_32;
      Horizontal_FOV_Rad  : Interfaces.C.double;
      Vertical_FOV_Rad    : Interfaces.C.double;
      Pixel_Format        : Interfaces.Unsigned_32;
      Frame_ID            : Interfaces.Unsigned_32;
      Subframe_ID         : Interfaces.Unsigned_32;
      Subframe_Total      : Interfaces.Unsigned_32;
      Image_Type          : Interfaces.Unsigned_32;
      Image_Flip          : Interfaces.Unsigned_32;
      Image_Flags         : Interfaces.Unsigned_32;
      Dither_Row          : Interfaces.C.double;
      Dither_Column       : Interfaces.C.double;
      Row_Offset          : Interfaces.Unsigned_32;
      Column_Offset       : Interfaces.Unsigned_32;
      Band_Index          : Interfaces.Unsigned_8;
      Reserved            : Reserved_Byte_Array;
      Pixels              : System.Address;
      Pixel_Capacity      : Size_T;
      Pixel_Required      : Size_T;
   end record
   with Convention => C;
   type U8_Span_V1 is record
      Data : System.Address;
      Size : Size_T;
   end record
   with Convention => C;
   type IR_Contributing_Sensor_V1 is record
      Location  : Component_Location_V1;
      Sensor_ID : Interfaces.Unsigned_32;
   end record
   with Convention => C;
   --  Every published TrackDataUpdate covariance term, exactly 21 doubles.
   type IR_Track_Covariance_V1 is record
      XX, XY, XZ, X_VX, X_VY, X_VZ : Interfaces.C.double;
      YY, YZ, Y_VX, Y_VY, Y_VZ     : Interfaces.C.double;
      ZZ, Z_VX, Z_VY, Z_VZ         : Interfaces.C.double;
      VX_VX, VX_VY, VX_VZ          : Interfaces.C.double;
      VY_VY, VY_VZ                 : Interfaces.C.double;
      VZ_VZ                        : Interfaces.C.double;
   end record
   with Convention => C;
   --  Complete TrackDataUpdate input. The two times stay in upstream epoch
   --  seconds and the canonical IR_Directional_V1 is reused for both ECEF
   --  vectors.
   type IR_Track_Data_Update_V1 is record
      Platform_ID                 : Interfaces.Unsigned_32;
      Capability_UUID             : UCI_ID_V1;
      Activity_UUID               : UCI_ID_V1;
      Track_ID                    : Interfaces.Unsigned_32;
      Entity_UUID                 : UCI_ID_V1;
      Track_Status                : Interfaces.Unsigned_32;
      Time_Of_Validity_Seconds    : Interfaces.C.double;
      Time_Of_Last_Update_Seconds : Interfaces.C.double;
      Track_Position_ECEF         : IR_Directional_V1;
      Track_Velocity_ECEF         : IR_Directional_V1;
      Covariance                  : IR_Track_Covariance_V1;
      Maneuver_Probability        : Interfaces.C.double;
      Track_Quality               : Interfaces.C.double;
   end record
   with Convention => C;
   --  Reuses the one generic IR_Command_Status_V1 layout.
   type IR_Track_Update_Result_V1 is record
      Status     : IR_Command_Status_V1;
      Error_Code : Interfaces.Unsigned_32;
   end record
   with Convention => C;
   --  Complete SystemTrackDataResponse input (@Optional). The system time is a
   --  signed 64-bit nanosecond count, the published bool values use the
   --  established Unsigned_8 representation, and the canonical Az_El_V1 is
   --  reused for both angle pairs. Every C double is imported as
   --  Interfaces.C.double rather than modeled directly as Long_Float.
   type IR_System_Track_Data_Response_V1 is record
      System_Time_NS       : Interfaces.Integer_64;
      Command_ID           : Interfaces.Unsigned_32;
      Request_ID           : Interfaces.Unsigned_32;
      Track_ID             : Interfaces.Unsigned_32;
      Range_M              : Interfaces.C.double;
      Range_Rate_MPS       : Interfaces.C.double;
      Range_Error_M        : Interfaces.C.double;
      Range_Rate_Error_MPS : Interfaces.C.double;
      Az_El_Valid          : Interfaces.Unsigned_8;
      Range_Valid          : Interfaces.Unsigned_8;
      Inertial_Az_El       : Az_El_V1;
      Az_El_Error          : Az_El_V1;
   end record
   with Convention => C;
   --  Semantically distinct from IR_Track_Update_Result_V1 while reusing the
   --  one generic IR_Command_Status_V1 layout.
   type IR_Track_System_Response_Result_V1 is record
      Status     : IR_Command_Status_V1;
      Error_Code : Interfaces.Unsigned_32;
   end record
   with Convention => C;
   type IR_Nav_Error_V1 is record
      X, Y, Z, W : Interfaces.C.double;
   end record
   with Convention => C;
   type IR_Orientation_V1 is record
      Kind       : Interfaces.Unsigned_32;
      Euler      : Euler_V1;
      Quaternion : IR_Quaternion_V1;
   end record
   with Convention => C;
   type IR_Sensor_Nav_State_V1 is record
      Position                       : IR_Directional_V1;
      Position_Error                 : IR_Nav_Error_V1;
      Velocity                       : IR_Directional_V1;
      Velocity_Error                 : IR_Nav_Error_V1;
      Acceleration                   : IR_Directional_V1;
      Acceleration_Error             : IR_Nav_Error_V1;
      Orientation                    : IR_Orientation_V1;
      Orientation_Error              : IR_Nav_Error_V1;
      Orientation_Velocity           : IR_Orientation_V1;
      Orientation_Velocity_Error     : IR_Nav_Error_V1;
      Orientation_Acceleration       : IR_Orientation_V1;
      Orientation_Acceleration_Error : IR_Nav_Error_V1;
      Coordinate_System              : Interfaces.Unsigned_32;
   end record
   with Convention => C;
   type IR_Sensor_Inertial_State_Span_V1 is record
      Data : System.Address;
      Size : Size_T;
   end record
   with Convention => C;
   type IR_Sensor_Nav_State_Span_V1 is record
      Data : System.Address;
      Size : Size_T;
   end record
   with Convention => C;
   type IR_Frame_Snapshot_V1 is record
      System_Time_NS, Integration_Time_NS                                         :
        Interfaces.Integer_64;
      Width, Height, Bits_Per_Pixel, Number_Of_Bands                              :
        Interfaces.Unsigned_32;
      Horizontal_FOV_Rad, Vertical_FOV_Rad                                        :
        Interfaces.C.double;
      Contributing_Sensor                                                         :
        IR_Contributing_Sensor_V1;
      Pixel_Format, Frame_ID, Subframe_ID, Subframe_Total, Image_Type, Image_Flip :
        Interfaces.Unsigned_32;
      Image_Flags                                                                 : U32_Span_V1;
      Dither_Row, Dither_Column                                                   :
        Interfaces.C.double;
      Row_Offset, Column_Offset                                                   :
        Interfaces.Unsigned_32;
      Sensor_Inertial_States                                                      :
        IR_Sensor_Inertial_State_Span_V1;
      Sensor_Nav_States                                                           :
        IR_Sensor_Nav_State_Span_V1;
      Band_Index                                                                  :
        Interfaces.Unsigned_8;
      Pixels                                                                      : U8_Span_V1;
   end record
   with Convention => C;

   type IR_Counters_V1 is record
      Frames_Received           : Interfaces.Unsigned_64;
      Frames_Dropped_Queue_Full : Interfaces.Unsigned_64;
      Malformed_Or_Unsupported  : Interfaces.Unsigned_64;
   end record
   with Convention => C;

   type Provider_Version_V1 is record
      API_Version          : Interfaces.Unsigned_32;
      Library_Version      : Interfaces.Unsigned_32;
      Vendor               : System.Address;
      Vendor_Capacity      : Size_T;
      Vendor_Required      : Size_T;
      Description          : System.Address;
      Description_Capacity : Size_T;
      Description_Required : Size_T;
   end record
   with Convention => C;

   function Get_ABI_Version (Output : access Version_V1) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_get_abi_version";

   function Session_Open
     (Library_Path        : Interfaces.C.Strings.chars_ptr;
      Instance            : Interfaces.C.Strings.chars_ptr;
      Aperture_Config_ID  : Interfaces.C.Strings.chars_ptr;
      Output              : access Session_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_session_open";

   function Session_Open_With_Options
     (Library_Path        : Interfaces.C.Strings.chars_ptr;
      Instance            : Interfaces.C.Strings.chars_ptr;
      Aperture_Config_ID  : Interfaces.C.Strings.chars_ptr;
      Options             : access constant Session_Options_V1;
      Output              : access Session_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_session_open_with_options";

   function Session_Get_Provider_Version
     (Handle              : Session_Handle;
      Output              : access Provider_Version_V1;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_session_get_provider_version";

   function Session_Close
     (Handle              : access Session_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_session_close";

   function IR_Stream_Open
     (Session             : Session_Handle;
      Config              : access IR_Stream_Config_V1;
      Output              : access Stream_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_stream_open";
   function IR_Stream_Start
     (Stream              : Stream_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_stream_start";
   function IR_Stream_Receive
     (Stream              : Stream_Handle;
      Timeout_MS          : Interfaces.Unsigned_32;
      Output              : access IR_Frame_V1;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_stream_receive";
   function IR_Stream_Get_Capabilities
     (Stream              : Stream_Handle;
      Output              : access Capability_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_stream_get_capabilities";
   function IR_Image_Metadata_Open
     (Stream              : Stream_Handle;
      Queue_Capacity      : Size_T;
      Output              : access Image_Metadata_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_image_metadata_open";
   function IR_Image_Metadata_Receive
     (Handle              : Image_Metadata_Handle;
      Timeout_MS          : Interfaces.Unsigned_32;
      Output              : access Image_Metadata_Event_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_image_metadata_receive";
   function IR_Image_Metadata_Get_Counters
     (Handle              : Image_Metadata_Handle;
      Output              : access Metadata_Counters_V1;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_image_metadata_get_counters";
   function IR_Image_Metadata_Close
     (Handle              : access Image_Metadata_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_image_metadata_close";
   function IR_Image_Metadata_Event_View
     (Handle              : Image_Metadata_Event_Handle;
      Output              : access System.Address;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_image_metadata_event_view";
   function IR_Image_Metadata_Event_Close
     (Handle              : access Image_Metadata_Event_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_image_metadata_event_close";
   function IR_Stream_Submit_Navigation_Report
     (Stream              : Stream_Handle;
      Report              : access Navigation_Report_V1;
      Output              : access Navigation_Request_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_stream_submit_navigation_report";
   function IR_Navigation_Request_Wait
     (Handle              : Navigation_Request_Handle;
      Timeout_MS          : Interfaces.Unsigned_32;
      Output              : access Navigation_Result_V1;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_navigation_request_wait";
   function IR_Navigation_Request_Close
     (Handle              : access Navigation_Request_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_navigation_request_close";
   function IR_Stream_Get_Counters
     (Stream              : Stream_Handle;
      Output              : access IR_Counters_V1;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_stream_get_counters";
   function IR_Stream_Receive_Snapshot
     (Handle              : Stream_Handle;
      Timeout_MS          : Interfaces.Unsigned_32;
      Output              : access Frame_Snapshot_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_stream_receive_snapshot";
   function IR_Frame_Snapshot_View
     (Handle              : Frame_Snapshot_Handle;
      Output              : access System.Address;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_frame_snapshot_view";
   function IR_Frame_Snapshot_Close
     (Handle              : access Frame_Snapshot_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_frame_snapshot_close";
   function IR_Stream_Stop
     (Stream              : Stream_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_stream_stop";
   function IR_Stream_Close
     (Stream              : access Stream_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_stream_close";
   function IR_C2_Open
     (Session             : Session_Handle;
      Config              : access IR_C2_Config_V1;
      Output              : access C2_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_c2_open";
   function IR_C2_Enable
     (Handle              : C2_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_c2_enable";
   function IR_C2_Submit_Operate
     (Handle              : C2_Handle;
      Command_ID          : Interfaces.Unsigned_32;
      Output              : access Mode_Request_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_c2_submit_operate";
   function IR_C2_Submit_Mode
     (Handle              : C2_Handle;
      Command             : access IR_Mode_Command_V1;
      Output              : access Mode_Request_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_c2_submit_mode";
   function IR_Mode_Request_Wait
     (Handle              : Mode_Request_Handle;
      Timeout_MS          : Interfaces.Unsigned_32;
      Output              : access IR_Mode_Result_V1;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_mode_request_wait";
   function IR_Mode_Request_Close
     (Handle              : access Mode_Request_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_mode_request_close";
   function IR_C2_Submit_BIT_No_Op
     (Handle              : C2_Handle;
      Command_ID          : Interfaces.Unsigned_32;
      Output              : access Return_Request_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_c2_submit_bit_noop";
   function IR_C2_Submit_BIT
     (Handle              : C2_Handle;
      Command             : access IR_BIT_Command_V1;
      Output              : access Return_Request_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_c2_submit_bit";
   function IR_C2_Submit_Config_Set
     (Handle              : C2_Handle;
      Command             : access IR_Config_Set_Command_V1;
      Output              : access Return_Request_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_c2_submit_config_set";
   function IR_C2_Send_Keepalive
     (Handle              : C2_Handle;
      Output              : access Return_Request_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_c2_send_keepalive";
   function IR_C2_Submit_Comms_Test
     (Handle              : C2_Handle;
      Request             : access IR_Channel_Comms_Test_Request_V1;
      Output              : access Comms_Request_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_c2_submit_comms_test";
   function IR_Comms_Request_Wait
     (Handle              : Comms_Request_Handle;
      Timeout_MS          : Interfaces.Unsigned_32;
      Output              : access IR_Channel_Comms_Test_Result_V1;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_channel_comms_request_wait";
   function IR_Comms_Request_Close
     (Handle              : access Comms_Request_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_channel_comms_request_close";
   function IR_C2_Get_Capabilities
     (Handle              : C2_Handle;
      Output              : access Capability_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_c2_get_capabilities";
   function IR_Capability_View
     (Handle              : Capability_Handle;
      Output              : access System.Address;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_channel_capability_view";
   function IR_Capability_Close
     (Handle              : access Capability_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_channel_capability_close";
   function IR_Return_Request_Wait
     (Handle              : Return_Request_Handle;
      Timeout_MS          : Interfaces.Unsigned_32;
      Output              : access IR_Return_Result_V1;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_return_request_wait";
   function IR_Return_Request_Close
     (Handle              : access Return_Request_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_return_request_close";
   function IR_C2_Close
     (Handle              : access C2_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_c2_close";
   function IR_C2_Metadata_Open
     (Handle              : C2_Handle;
      Queue_Capacity      : Size_T;
      Output              : access Metadata_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_c2_metadata_open";
   function IR_C2_Metadata_Register_Comms_Test
     (Handle              : Metadata_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_c2_metadata_register_comms_test";
   function IR_C2_Metadata_Receive
     (Handle              : Metadata_Handle;
      Timeout_MS          : Interfaces.Unsigned_32;
      Output              : access Metadata_Event_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_c2_metadata_receive";
   function IR_C2_Metadata_Get_Counters
     (Handle              : Metadata_Handle;
      Output              : access Metadata_Counters_V1;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_c2_metadata_get_counters";
   function IR_C2_Metadata_Close
     (Handle              : access Metadata_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_c2_metadata_close";
   function IR_C2_Metadata_Event_View
     (Handle              : Metadata_Event_Handle;
      Output              : access System.Address;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_c2_metadata_event_view";
   function IR_C2_Metadata_Event_Close
     (Handle              : access Metadata_Event_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_c2_metadata_event_close";
   function IR_Health_Open
     (Parent              : Session_Handle;
      Config              : access constant IR_Health_Config_V1;
      Output              : access Health_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_health_open";
   function IR_Health_Enable
     (Handle              : Health_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_health_enable";
   function IR_Health_Get_Capabilities
     (Handle              : Health_Handle;
      Output              : access Capability_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_health_get_capabilities";
   function IR_Health_Close
     (Handle              : access Health_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_health_close";
   function IR_Health_Metadata_Open
     (Handle              : Health_Handle;
      Queue_Capacity      : Size_T;
      Output              : access Health_Metadata_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_health_metadata_open";
   function IR_Health_Metadata_Receive
     (Handle              : Health_Metadata_Handle;
      Timeout_MS          : Interfaces.Unsigned_32;
      Output              : access Health_Event_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_health_metadata_receive";
   function IR_Health_Metadata_Get_Counters
     (Handle              : Health_Metadata_Handle;
      Output              : access Metadata_Counters_V1;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_health_metadata_get_counters";
   function IR_Health_Metadata_Close
     (Handle              : access Health_Metadata_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_health_metadata_close";
   function IR_Health_Event_View
     (Handle              : Health_Event_Handle;
      Output              : access System.Address;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_health_metadata_event_view";
   function IR_Health_Event_Close
     (Handle              : access Health_Event_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_health_metadata_event_close";
   function IR_Instrumentation_Open
     (Parent              : Session_Handle;
      Config              : access constant IR_Instrumentation_Config_V1;
      Output              : access Instrumentation_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_instrumentation_open";
   function IR_Instrumentation_Enable
     (Handle              : Instrumentation_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_instrumentation_enable";
   function IR_Instrumentation_Get_Capabilities
     (Handle              : Instrumentation_Handle;
      Output              : access Capability_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_instrumentation_get_capabilities";
   function IR_Instrumentation_Submit_Level
     (Handle              : Instrumentation_Handle;
      Command             : access constant IR_Instrumentation_Level_Command_V1;
      Output              : access Instrumentation_Request_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_instrumentation_submit_level";
   function IR_Instrumentation_Request_Wait
     (Handle              : Instrumentation_Request_Handle;
      Timeout_MS          : Interfaces.Unsigned_32;
      Output              : access IR_Instrumentation_Result_V1;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_instrumentation_request_wait";
   function IR_Instrumentation_Request_Close
     (Handle              : access Instrumentation_Request_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_instrumentation_request_close";
   function IR_Instrumentation_Metadata_Open
     (Handle              : Instrumentation_Handle;
      Queue_Capacity      : Size_T;
      Output              : access Instrumentation_Metadata_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_instrumentation_metadata_open";
   function IR_Instrumentation_Metadata_Receive
     (Handle              : Instrumentation_Metadata_Handle;
      Timeout_MS          : Interfaces.Unsigned_32;
      Output              : access Instrumentation_Event_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_instrumentation_metadata_receive";
   function IR_Instrumentation_Metadata_Get_Counters
     (Handle              : Instrumentation_Metadata_Handle;
      Output              : access Metadata_Counters_V1;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with
     Import,
     Convention    => C,
     External_Name => "ams_mel_ir_instrumentation_metadata_get_counters";
   function IR_Instrumentation_Metadata_Close
     (Handle              : access Instrumentation_Metadata_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_instrumentation_metadata_close";
   function IR_Instrumentation_Event_View
     (Handle              : Instrumentation_Event_Handle;
      Output              : access System.Address;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_instrumentation_metadata_event_view";
   function IR_Instrumentation_Event_Close
     (Handle              : access Instrumentation_Event_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_instrumentation_metadata_event_close";
   function IR_Instrumentation_Close
     (Handle              : access Instrumentation_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_instrumentation_close";
   function IR_Track_Open
     (Session             : Session_Handle;
      Config              : access constant IR_Track_Config_V1;
      Output              : access Track_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_track_open";
   function IR_Track_Enable
     (Handle              : Track_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_track_enable";
   function IR_Track_Get_Capabilities
     (Handle              : Track_Handle;
      Output              : access Capability_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_track_get_capabilities";
   function IR_Track_Close
     (Handle              : access Track_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_track_close";
   function IR_Track_Metadata_Open
     (Handle              : Track_Handle;
      Queue_Capacity      : Size_T;
      Output              : access Track_Metadata_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_track_metadata_open";
   function IR_Track_Metadata_Receive
     (Handle              : Track_Metadata_Handle;
      Timeout_MS          : Interfaces.Unsigned_32;
      Output              : access Track_Event_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_track_metadata_receive";
   function IR_Track_Metadata_Get_Counters
     (Handle              : Track_Metadata_Handle;
      Output              : access Metadata_Counters_V1;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_track_metadata_get_counters";
   function IR_Track_Metadata_Close
     (Handle              : access Track_Metadata_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_track_metadata_close";
   function IR_Track_Event_View
     (Handle              : Track_Event_Handle;
      Output              : access System.Address;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_track_metadata_event_view";
   function IR_Track_Event_View_V2
     (Handle              : Track_Event_Handle;
      Output              : access System.Address;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_track_metadata_event_view_v2";
   function IR_Track_Event_View_V3
     (Handle              : Track_Event_Handle;
      Output              : access System.Address;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_track_metadata_event_view_v3";
   function IR_Track_Event_Close
     (Handle              : access Track_Event_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_track_metadata_event_close";
   function IR_Track_Submit_Update
     (Handle              : Track_Handle;
      Update              : access constant IR_Track_Data_Update_V1;
      Output              : access Track_Update_Request_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_track_submit_update";
   function IR_Track_Update_Request_Wait
     (Handle              : Track_Update_Request_Handle;
      Timeout_MS          : Interfaces.Unsigned_32;
      Output              : access IR_Track_Update_Result_V1;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_track_update_request_wait";
   function IR_Track_Update_Request_Close
     (Handle              : access Track_Update_Request_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_track_update_request_close";
   function IR_Track_Submit_System_Track_Data_Response
     (Handle              : Track_Handle;
      Response            : access constant IR_System_Track_Data_Response_V1;
      Output              : access Track_System_Response_Request_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with
     Import,
     Convention    => C,
     External_Name => "ams_mel_ir_track_submit_system_track_data_response";
   function IR_Track_System_Response_Request_Wait
     (Handle              : Track_System_Response_Request_Handle;
      Timeout_MS          : Interfaces.Unsigned_32;
      Output              : access IR_Track_System_Response_Result_V1;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_track_system_response_request_wait";
   function IR_Track_System_Response_Request_Close
     (Handle              : access Track_System_Response_Request_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_track_system_response_request_close";

   --  Task 032B1 public common Channel C ABI (private raw imports only).
   function IR_Channel_From_C2
     (Source              : C2_Handle;
      Output              : access Channel_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_channel_from_c2";
   function IR_Channel_From_Stream
     (Source              : Stream_Handle;
      Output              : access Channel_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_channel_from_stream";
   function IR_Channel_From_Health
     (Source              : Health_Handle;
      Output              : access Channel_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_channel_from_health";
   function IR_Channel_From_Instrumentation
     (Source              : Instrumentation_Handle;
      Output              : access Channel_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_channel_from_instrumentation";
   function IR_Channel_From_Track
     (Source              : Track_Handle;
      Output              : access Channel_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_channel_from_track";
   function IR_Channel_Send_Keepalive
     (Handle              : Channel_Handle;
      Output              : access Return_Request_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_channel_send_keepalive";
   function IR_Channel_Submit_Comms_Test
     (Handle              : Channel_Handle;
      Request             : access IR_Channel_Comms_Test_Request_V1;
      Output              : access Comms_Request_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_channel_submit_comms_test";
   function IR_Channel_Get_Capabilities
     (Handle              : Channel_Handle;
      Output              : access Capability_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_channel_get_capabilities";
   function IR_Channel_Close
     (Handle              : access Channel_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_ir_channel_close";

   --  Task 033B RF DataMEL raw records. Spans are raw addresses; every
   --  femtosecond value is the upstream int64 count.
   RF_Job_Data_Format_Direct_INT8          : constant Interfaces.Unsigned_32 := 0;
   RF_Job_Data_Format_Direct_INT16         : constant Interfaces.Unsigned_32 := 1;
   RF_Job_Data_Format_Complex_INT8         : constant Interfaces.Unsigned_32 := 2;
   RF_Job_Data_Format_Complex_INT16        : constant Interfaces.Unsigned_32 := 3;
   RF_Job_Data_Format_AMS_Vita_Small       : constant Interfaces.Unsigned_32 := 4;
   RF_Job_Data_Format_AMS_Vita_Medium      : constant Interfaces.Unsigned_32 := 5;
   RF_Job_Data_Format_AMS_Vita_Large       : constant Interfaces.Unsigned_32 := 6;
   RF_Job_Data_Format_AMS_Vita_Extra_Large : constant Interfaces.Unsigned_32 := 7;
   RF_Job_Data_Format_PDW_Type1            : constant Interfaces.Unsigned_32 := 8;
   RF_Job_Data_Format_PDW_Type2            : constant Interfaces.Unsigned_32 := 9;
   RF_Job_Data_Format_PDW_Type3            : constant Interfaces.Unsigned_32 := 10;
   RF_Job_Data_Format_LF_Type1             : constant Interfaces.Unsigned_32 := 11;
   RF_Job_Data_Format_LF_Type2             : constant Interfaces.Unsigned_32 := 12;
   RF_Job_Data_Format_LF_Type3             : constant Interfaces.Unsigned_32 := 13;

   type RF_Frequency_Range_V1 is record
      Min_Hz : Interfaces.C.double;
      Max_Hz : Interfaces.C.double;
   end record
   with Convention => C;
   type RF_Frequency_Range_Span_V1 is record
      Data : System.Address;
      Size : Size_T;
   end record
   with Convention => C;
   type RF_RX_Element_Group_Config_V1 is record
      Label                       : String_View_V1;
      Desired_Duty_Factor         : Interfaces.C.double;
      Expected_Center_Frequencies : RF_Frequency_Range_Span_V1;
      Endpoint_IDs                : U64_Span_V1;
      Data_Pipe_Label             : String_View_V1;
   end record
   with Convention => C;
   type RF_Job_Request_Config_V1 is record
      Request_ID                 : Interfaces.Unsigned_32;
      Priority                   : Interfaces.Unsigned_32;
      Precedence_Within_Priority : Interfaces.Unsigned_32;
      Is_Interruptable           : Interfaces.Unsigned_32;
      Instance_Selection         : U32_Span_V1;
      RX_Group                   : RF_RX_Element_Group_Config_V1;
   end record
   with Convention => C;
   type RF_UTC_Time_V1 is record
      Seconds, Fractional_Femtoseconds : Interfaces.Integer_64;
   end record
   with Convention => C;
   type RF_RX_Data_Pipe_Endpoint_Config_V1 is record
      Data_Pipe_Label : String_View_V1;
      Endpoint_IDs    : U64_Span_V1;
   end record
   with Convention => C;
   type RF_RX_Data_Pipe_Endpoint_Config_Span_V1 is record
      Data : System.Address;
      Size : Size_T;
   end record
   with Convention => C;
   type RF_RX_Element_Group_Config_V2 is record
      Label                       : String_View_V1;
      Desired_Duty_Factor         : Interfaces.C.double;
      Expected_Center_Frequencies : RF_Frequency_Range_Span_V1;
      Data_Pipe_Endpoint_Configs  : RF_RX_Data_Pipe_Endpoint_Config_Span_V1;
   end record
   with Convention => C;
   type RF_RX_Element_Group_Config_Span_V2 is record
      Data : System.Address;
      Size : Size_T;
   end record
   with Convention => C;
   type RF_Job_Request_Config_V2 is record
      Request_ID, Priority, Precedence_Within_Priority, Is_Interruptable : Interfaces.Unsigned_32;
      Instance_Selection                                                 : U32_Span_V1;
      RX_Groups                                                          :
        RF_RX_Element_Group_Config_Span_V2;
      Min_Start_Time, Max_Complete_Time                                  : RF_UTC_Time_V1;
      Duration_Femtoseconds                                              : Interfaces.Integer_64;
      Capability_ID, Activity_ID                                         : U8_Span_V1;
      TX_Power_Mode_IDs                                                  : U32_Span_V1;
      Lookahead_Femtoseconds                                             : Interfaces.Integer_64;
   end record
   with Convention => C;
   subtype RF_Pointing_Kind is Interfaces.Unsigned_32;
   RF_Pointing_ECEF                                                         :
     constant RF_Pointing_Kind := 0;
   RF_Pointing_LLA                                                          :
     constant RF_Pointing_Kind := 1;
   RF_Pointing_Platform_Relative                                            :
     constant RF_Pointing_Kind := 2;
   RF_Pointing_Face_Relative                                                :
     constant RF_Pointing_Kind := 3;
   RF_Pointing_Baseline_Relative                                            :
     constant RF_Pointing_Kind := 4;
   type RF_Vector3_V1 is record
      X, Y, Z : Interfaces.C.double;
   end record
   with Convention => C;
   type RF_Az_El_V1 is record
      Azimuth_Rad, Elevation_Rad : Interfaces.C.double;
   end record
   with Convention => C;
   type RF_ECEF_Pointing_V1 is record
      Location_M, Velocity_MPS : RF_Vector3_V1;
      Time_Of_Validity         : RF_UTC_Time_V1;
   end record
   with Convention => C;
   type RF_LLA_Pointing_V1 is record
      Latitude_Rad, Longitude_Rad, Altitude_M                  : Interfaces.C.double;
      Velocity_North_MPS, Velocity_East_MPS, Velocity_Down_MPS : Interfaces.C.double;
      Time_Of_Validity                                         : RF_UTC_Time_V1;
   end record
   with Convention => C;
   type RF_Pointing_V1 is record
      Kind                             : RF_Pointing_Kind;
      ECEF                             : RF_ECEF_Pointing_V1;
      LLA                              : RF_LLA_Pointing_V1;
      Platform_Relative, Face_Relative : RF_Az_El_V1;
      Baseline_Relative_Conic_Rad      : Interfaces.C.double;
   end record
   with Convention => C;
   type RF_Pointing_Span_V1 is record
      Data : System.Address;
      Size : Size_T;
   end record
   with Convention => C;
   type RF_RX_Element_Group_Config_V3 is record
      Group                    : RF_RX_Element_Group_Config_V2;
      Expected_Pointing_Angles : RF_Pointing_Span_V1;
   end record
   with Convention => C;
   type RF_RX_Element_Group_Config_Span_V3 is record
      Data : System.Address;
      Size : Size_T;
   end record
   with Convention => C;
   type RF_Job_Request_Config_V3 is record
      Request_ID, Priority, Precedence_Within_Priority, Is_Interruptable : Interfaces.Unsigned_32;
      Instance_Selection                                                 : U32_Span_V1;
      RX_Groups                                                          :
        RF_RX_Element_Group_Config_Span_V3;
      Min_Start_Time, Max_Complete_Time                                  : RF_UTC_Time_V1;
      Duration_Femtoseconds                                              : Interfaces.Integer_64;
      Capability_ID, Activity_ID                                         : U8_Span_V1;
      TX_Power_Mode_IDs                                                  : U32_Span_V1;
      Lookahead_Femtoseconds                                             : Interfaces.Integer_64;
      Has_Estimated_Stab_Point                                           : Interfaces.Unsigned_32;
      Estimated_Stab_Point                                               : RF_Pointing_V1;
   end record
   with Convention => C;
   subtype RF_Element_Group_Mode is Interfaces.Unsigned_32;
   RF_Element_Group_Mode_RX                                                 :
     constant RF_Element_Group_Mode := 0;
   RF_Element_Group_Mode_TX                                                 :
     constant RF_Element_Group_Mode := 1;
   type RF_TX_Element_Group_Config_V1 is record
      Label                       : String_View_V1;
      TX_Power_Level              : Interfaces.Unsigned_32;
      Desired_Duty_Factor         : Interfaces.C.double;
      Expected_Center_Frequencies : RF_Frequency_Range_Span_V1;
   end record
   with Convention => C;
   type RF_Job_Element_Group_Config_V4 is record
      Mode : RF_Element_Group_Mode;
      RX   : RF_RX_Element_Group_Config_V3;
      TX   : RF_TX_Element_Group_Config_V1;
   end record
   with Convention => C;
   type RF_Job_Element_Group_Config_Span_V4 is record
      Data : System.Address;
      Size : Size_T;
   end record
   with Convention => C;
   type RF_Job_Request_Config_V4 is record
      Request_ID, Priority, Precedence_Within_Priority, Is_Interruptable : Interfaces.Unsigned_32;
      Instance_Selection                                                 : U32_Span_V1;
      Element_Groups                                                     :
        RF_Job_Element_Group_Config_Span_V4;
      Min_Start_Time, Max_Complete_Time                                  : RF_UTC_Time_V1;
      Duration_Femtoseconds                                              : Interfaces.Integer_64;
      Capability_ID, Activity_ID                                         : U8_Span_V1;
      TX_Power_Mode_IDs                                                  : U32_Span_V1;
      Lookahead_Femtoseconds                                             : Interfaces.Integer_64;
      Has_Estimated_Stab_Point                                           : Interfaces.Unsigned_32;
      Estimated_Stab_Point                                               : RF_Pointing_V1;
   end record
   with Convention => C;
   type RF_Receive_Event_Config_V1 is record
      Event_ID                    : Interfaces.Unsigned_32;
      Element_Group_Label         : String_View_V1;
      Start_Femtoseconds          : Interfaces.Integer_64;
      Duration_Femtoseconds       : Interfaces.Integer_64;
      Center_Frequency_Hz         : Interfaces.C.double;
      Sample_Frequency_Hz         : Interfaces.C.double;
      AGC_Processing_Iterations   : Interfaces.Unsigned_64;
      Ignored_Post_AGC_Iterations : Interfaces.Unsigned_64;
      Max_Extension_Femtoseconds  : Interfaces.Integer_64;
   end record
   with Convention => C;
   type RF_Receive_Event_Config_Span_V1 is record
      Data : System.Address;
      Size : Size_T;
   end record
   with Convention => C;
   type RF_Job_Interval_Config_V1 is record
      Interval_Start_Femtoseconds        : Interfaces.Integer_64;
      Interval_ID                        : Interfaces.Unsigned_32;
      Interval_Starting_Gap_Femtoseconds : Interfaces.Integer_64;
      Sequence_Duration_Femtoseconds     : Interfaces.Integer_64;
      Sequence_Repeat_Count              : Interfaces.Unsigned_64;
      Calibration_Duration_Femtoseconds  : Interfaces.Integer_64;
      Interval_Ending_Gap_Femtoseconds   : Interfaces.Integer_64;
      Phase_Coherence_With_Prior         : Interfaces.Unsigned_32;
      Iterations_Per_Signal              : Interfaces.Unsigned_64;
      Max_Data_Rate_BPS                  : Interfaces.C.double;
      Max_Sample_Rate_Hz                 : Interfaces.C.double;
      Job_Details_ID                     : Interfaces.Unsigned_32;
      Receive_Events                     : RF_Receive_Event_Config_Span_V1;
   end record
   with Convention => C;
   type RF_Job_Interval_Config_Span_V1 is record
      Data : System.Address;
      Size : Size_T;
   end record
   with Convention => C_Pass_By_Copy;
   type RF_Job_Result_V1 is record
      Error_Code : Interfaces.Unsigned_32;
   end record
   with Convention => C;
   type RF_Job_Interval_Config_V2 is record
      Interval      : RF_Job_Interval_Config_V1;
      Status_Enable : Interfaces.Unsigned_32;
   end record
   with Convention => C;
   type RF_Job_Interval_Config_Span_V2 is record
      Data : System.Address;
      Size : Size_T;
   end record
   with Convention => C_Pass_By_Copy;
   type RF_Receive_Event_Config_V2 is record
      Event                        : RF_Receive_Event_Config_V1;
      Stab_Point_Index             : Interfaces.Unsigned_64;
      Applicable_RX_Element_Groups : U64_Span_V1;
   end record
   with Convention => C;
   type RF_Receive_Event_Config_Span_V2 is record
      Data : System.Address;
      Size : Size_T;
   end record
   with Convention => C;
   type RF_Job_Interval_Config_V3 is record
      Interval_Start_Femtoseconds        : Interfaces.Integer_64;
      Interval_ID                        : Interfaces.Unsigned_32;
      Interval_Starting_Gap_Femtoseconds : Interfaces.Integer_64;
      Sequence_Duration_Femtoseconds     : Interfaces.Integer_64;
      Sequence_Repeat_Count              : Interfaces.Unsigned_64;
      Calibration_Duration_Femtoseconds  : Interfaces.Integer_64;
      Interval_Ending_Gap_Femtoseconds   : Interfaces.Integer_64;
      Phase_Coherence_With_Prior         : Interfaces.Unsigned_32;
      Iterations_Per_Signal              : Interfaces.Unsigned_64;
      Max_Data_Rate_BPS                  : Interfaces.C.double;
      Max_Sample_Rate_Hz                 : Interfaces.C.double;
      Job_Details_ID                     : Interfaces.Unsigned_32;
      Status_Enable                      : Interfaces.Unsigned_32;
      Stab_Points                        : RF_Pointing_Span_V1;
      Receive_Events                     : RF_Receive_Event_Config_Span_V2;
   end record
   with Convention => C;
   type RF_Job_Interval_Config_Span_V3 is record
      Data : System.Address;
      Size : Size_T;
   end record
   with Convention => C_Pass_By_Copy;
   Rf_Interval_Status_Never                                                 :
     constant Interfaces.Unsigned_32 := 0;
   Rf_Interval_Status_Always                                                :
     constant Interfaces.Unsigned_32 := 1;
   Rf_Interval_Status_On_Exception                                          :
     constant Interfaces.Unsigned_32 := 2;
   Rf_Interval_Completion_None                                              :
     constant Interfaces.Unsigned_32 := 0;
   Rf_Interval_Completion_Ready_For_Next_Job_Interval                       :
     constant Interfaces.Unsigned_32 := 1;
   Rf_Interval_Completion_Failed_Interrupted                                :
     constant Interfaces.Unsigned_32 := 2;
   Rf_Interval_Completion_Failed_Invalid_Tx_Event_Spatial_Data              :
     constant Interfaces.Unsigned_32 := 3;
   Rf_Interval_Completion_Failed_Invalid_Tx_Event_Signal_Data               :
     constant Interfaces.Unsigned_32 := 4;
   Rf_Interval_Completion_Failed_Invalid_Tx_Event_Temporal_Data             :
     constant Interfaces.Unsigned_32 := 5;
   Rf_Interval_Completion_Failed_Invalid_Tx_Event_Identifier_Data           :
     constant Interfaces.Unsigned_32 := 6;
   Rf_Interval_Completion_Failed_Invalid_Rx_Event_Spatial_Data              :
     constant Interfaces.Unsigned_32 := 7;
   Rf_Interval_Completion_Failed_Invalid_Rx_Event_Signal_Data               :
     constant Interfaces.Unsigned_32 := 8;
   Rf_Interval_Completion_Failed_Invalid_Rx_Event_Temporal_Data             :
     constant Interfaces.Unsigned_32 := 9;
   Rf_Interval_Completion_Failed_Invalid_Rx_Event_Identifier_Data           :
     constant Interfaces.Unsigned_32 := 10;
   Rf_Interval_Completion_Failed_Invalid_Sequence_Temporal_Data             :
     constant Interfaces.Unsigned_32 := 11;
   Rf_Interval_Completion_Failed_Invalid_Job_Interval_Spatial_Data          :
     constant Interfaces.Unsigned_32 := 12;
   Rf_Interval_Completion_Failed_Invalid_Job_Interval_Event_Signal_Data     :
     constant Interfaces.Unsigned_32 := 13;
   Rf_Interval_Completion_Failed_Invalid_Job_Interval_Event_Temporal_Data   :
     constant Interfaces.Unsigned_32 := 14;
   Rf_Interval_Completion_Failed_Invalid_Job_Interval_Event_Identifier_Data :
     constant Interfaces.Unsigned_32 := 15;
   Rf_Interval_Completion_Failed_Invalid_Job_Temporal_Data                  :
     constant Interfaces.Unsigned_32 := 16;
   Rf_Interval_Completion_Failed_Invalid_Job_Identifier_Data                :
     constant Interfaces.Unsigned_32 := 17;
   Rf_Interval_Completion_Completed                                         :
     constant Interfaces.Unsigned_32 := 18;
   Rf_Interval_Completion_Cancelled                                         :
     constant Interfaces.Unsigned_32 := 19;
   Rf_Interval_Completion_Late_Controls                                     :
     constant Interfaces.Unsigned_32 := 20;
   Rf_Interval_Completion_Invalid_Controls                                  :
     constant Interfaces.Unsigned_32 := 21;
   Rf_Interval_Completion_Antenna_Fov_Error                                 :
     constant Interfaces.Unsigned_32 := 22;
   Rf_Interval_Completion_Transmit_Rf_Inhibited                             :
     constant Interfaces.Unsigned_32 := 23;
   Rf_Interval_Completion_Started                                           :
     constant Interfaces.Unsigned_32 := 24;
   Rf_Log_Trigger_None                                                      :
     constant Interfaces.Unsigned_32 := 0;
   Rf_Log_Trigger_Event_Extended                                            :
     constant Interfaces.Unsigned_32 := 1;
   Rf_Log_Trigger_Event_Triggered                                           :
     constant Interfaces.Unsigned_32 := 2;
   Rf_Log_Trigger_Event_Resumed                                             :
     constant Interfaces.Unsigned_32 := 3;
   Rf_Log_Trigger_Event_Cancelled                                           :
     constant Interfaces.Unsigned_32 := 4;
   Rf_Log_Trigger_Event_Inhibited                                           :
     constant Interfaces.Unsigned_32 := 5;
   Rf_Log_Trigger_Event_Delayed_Start                                       :
     constant Interfaces.Unsigned_32 := 6;
   Rf_Log_Trigger_Event_Type_Not_Supported                                  :
     constant Interfaces.Unsigned_32 := 7;
   type RF_Interval_Status_Handle is new System.Address;
   Null_RF_Interval_Status                                                  :
     constant RF_Interval_Status_Handle := RF_Interval_Status_Handle (System.Null_Address);
   type RF_Interval_Status_Event_Handle is new System.Address;
   Null_RF_Interval_Status_Event                                            :
     constant RF_Interval_Status_Event_Handle :=
       RF_Interval_Status_Event_Handle (System.Null_Address);
   type RF_Interval_Status_Options_V1 is record
      Queue_Capacity, Max_Event_Log_Entries, Max_Activity_ID_Bytes : Size_T;
   end record
   with Convention => C;
   type RF_Interval_Status_Counters_V1 is record
      Callback_Entries,
      Events_Queued,
      Events_Delivered,
      Queue_Full_Drops,
      Malformed_Drops,
      Oversize_Drops,
      Allocation_Failures,
      Callbacks_After_Close : Interfaces.Unsigned_64;
   end record
   with Convention => C;
   type RF_Job_Event_Log_Entry_V1 is record
      Event_ID, Trigger                          : Interfaces.Unsigned_32;
      Time_Seconds, Time_Fractional_Femtoseconds : Interfaces.Integer_64;
   end record
   with Convention => C;
   type RF_Interval_Status_V1 is record
      Interval_ID, Completion_Status : Interfaces.Unsigned_32;
      Event_Log                      : Span_V1;
      Activity_ID                    : U8_Span_V1;
   end record
   with Convention => C;
   function RF_Interval_Status_Open
     (Handle              : RF_Job_Handle;
      Options             : access constant RF_Interval_Status_Options_V1;
      Output              : access RF_Interval_Status_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_job_interval_status_open";
   function RF_Interval_Status_Receive
     (Handle              : RF_Interval_Status_Handle;
      Timeout_MS          : Interfaces.Unsigned_32;
      Output              : access RF_Interval_Status_Event_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_job_interval_status_receive";
   function RF_Interval_Status_Get_Counters
     (Handle              : RF_Interval_Status_Handle;
      Output              : access RF_Interval_Status_Counters_V1;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_job_interval_status_get_counters";
   function RF_Interval_Status_Close
     (Handle              : access RF_Interval_Status_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_job_interval_status_close";
   function RF_Interval_Status_Event_View
     (Handle              : RF_Interval_Status_Event_Handle;
      Output              : access System.Address;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_job_interval_status_event_view";
   function RF_Interval_Status_Event_Close
     (Handle              : access RF_Interval_Status_Event_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_job_interval_status_event_close";
   function RF_Job_Add_RX_Intervals_V2
     (Handle              : RF_Job_Handle;
      Intervals           : RF_Job_Interval_Config_Span_V2;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_job_add_rx_intervals_v2";
   function RF_Job_Add_RX_Intervals_V3
     (Handle              : RF_Job_Handle;
      Intervals           : RF_Job_Interval_Config_Span_V3;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_job_add_rx_intervals_v3";
   type RF_Job_Info_V1 is record
      Actual_Start_Seconds            : Interfaces.Integer_64;
      Actual_Start_Femtoseconds       : Interfaces.Integer_64;
      Total_Job_Duration_Femtoseconds : Interfaces.Integer_64;
      VA_Instance_ID                  : Interfaces.Unsigned_32;
      VA_Definition_ID                : Interfaces.Unsigned_32;
      Job_Details_ID                  : Interfaces.Unsigned_32;
      Job_Request_ID                  : Interfaces.Unsigned_32;
      Lookahead_Femtoseconds          : Interfaces.Integer_64;
      RX_Stream_IDs                   : U32_Span_V1;
   end record
   with Convention => C;
   RF_Job_Status_None                                                       :
     constant Interfaces.Unsigned_32 := 0;
   RF_Job_Status_In_Progress                                                :
     constant Interfaces.Unsigned_32 := 1;
   RF_Job_Status_Complete                                                   :
     constant Interfaces.Unsigned_32 := 2;
   RF_Job_Status_Failed_Invalid_ID                                          :
     constant Interfaces.Unsigned_32 := 3;
   RF_Job_Status_Failed_Interrupted                                         :
     constant Interfaces.Unsigned_32 := 4;
   RF_Job_Status_Failed_Invalid_State                                       :
     constant Interfaces.Unsigned_32 := 5;
   RF_Cancel_Error_None                                                     :
     constant Interfaces.Unsigned_32 := 0;
   type RF_Job_Cancel_Result_V1 is record
      Cancelled  : Interfaces.Unsigned_32;
      Error_Code : Interfaces.Unsigned_32;
   end record
   with Convention => C;
   type RF_Face_Info_V1 is record
      Face_ID                       : Interfaces.Unsigned_32;
      Supports_Receive              : Interfaces.Unsigned_32;
      Supports_Transmit             : Interfaces.Unsigned_32;
      Requires_Endpoint_Association : Interfaces.Unsigned_32;
      AGC_Processing_Time_FS        : Interfaces.Integer_64;
      Min_Job_Request_Lead_Time_FS  : Interfaces.Integer_64;
      Max_Job_Request_Lead_Time_FS  : Interfaces.Integer_64;
      Min_Job_Detail_Lead_Time_FS   : Interfaces.Integer_64;
      Tx_Rx_Switching_Time_FS       : Interfaces.Integer_64;
      Rx_Tx_Switching_Time_FS       : Interfaces.Integer_64;
      Tx_Tx_Switching_Time_FS       : Interfaces.Integer_64;
      Rx_Rx_Switching_Time_FS       : Interfaces.Integer_64;
      Rx_Frequency_Ranges           : Span_V1;
      Tx_Frequency_Ranges           : Span_V1;
      Sample_Frequency_Ranges       : Span_V1;
   end record
   with Convention => C;
   type RF_Tx_Power_Mode_Snapshot_Handle is new System.Address;
   Null_RF_Tx_Power_Mode_Snapshot                                           :
     constant RF_Tx_Power_Mode_Snapshot_Handle :=
       RF_Tx_Power_Mode_Snapshot_Handle (System.Null_Address);
   type RF_Tx_Power_Mode_V1 is record
      Tx_Power_Mode_ID, Is_Linear_Operation, Tx_Power_Level : Interfaces.Unsigned_32;
      Tx_Frequency_Ranges                                   : Span_V1;
      Max_Tx_Duty_Factor                                    : Interfaces.C.double;
      Max_Tx_Pulse_Width_NS                                 : Interfaces.Integer_64;
      Max_Tx_Atten, Tx_Atten_Step_Size                      : Interfaces.C.double;
   end record
   with Convention => C;
   function RF_Data_Get_Tx_Power_Modes
     (Handle              : RF_Data_Handle;
      Face_ID             : Interfaces.Unsigned_32;
      Output              : access RF_Tx_Power_Mode_Snapshot_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_data_get_tx_power_modes";
   function RF_Data_Get_Tx_Power_Mode
     (Handle                 : RF_Data_Handle;
      Face_ID, Power_Mode_ID : Interfaces.Unsigned_32;
      Output                 : access RF_Tx_Power_Mode_Snapshot_Handle;
      Diagnostic             : System.Address;
      Diagnostic_Capacity    : Size_T;
      Diagnostic_Required    : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_data_get_tx_power_mode";
   function RF_Tx_Power_Mode_Snapshot_View
     (Handle              : RF_Tx_Power_Mode_Snapshot_Handle;
      Output              : access Span_V1;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_tx_power_mode_snapshot_view";
   function RF_Tx_Power_Mode_Snapshot_Close
     (Handle              : access RF_Tx_Power_Mode_Snapshot_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_tx_power_mode_snapshot_close";
   type RF_Euler_V1 is record
      Roll_Rad, Pitch_Rad, Yaw_Rad : Interfaces.C.double;
   end record
   with Convention => C;
   type RF_Component_Location_V1 is record
      Offset_X_M, Offset_Y_M, Offset_Z_M : Interfaces.C.double;
      Key, System_Name                   : String_View_V1;
   end record
   with Convention => C;
   type RF_Physical_Data_V1 is record
      Antenna_Height_M, Antenna_Width_M, Lattice_Angle_Rad : Interfaces.C.double;
      Location                                             : RF_Component_Location_V1;
      Orientation, Boresight                               : RF_Euler_V1;
   end record
   with Convention => C;
   type RF_MFA_Info_V1 is record
      Reported_Num_Faces             : Interfaces.Unsigned_64;
      Contains_Open_Additions        : Interfaces.Unsigned_32;
      Scheduler_Resolution_FS        : Interfaces.Integer_64;
      Max_User_Defined_Context_Bytes : Interfaces.Unsigned_64;
      Supported_Data_Formats         : Span_V1;
      Faces                          : Span_V1;
   end record
   with Convention => C;

   --  RF C2 lifecycle (no request or VA operations in this checkpoint).
   function RF_C2_Open
     (Library_Path        : Interfaces.C.Strings.chars_ptr;
      Configuration       : Interfaces.C.Strings.chars_ptr;
      Output              : access RF_C2_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_c2_open";
   function RF_C2_Close
     (Handle              : access RF_C2_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_c2_close";
   function RF_C2_Submit_VA
     (Handle              : RF_C2_Handle;
      Config              : access constant RF_VA_Config_V1;
      Output              : access RF_VA_Request_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_c2_submit_virtual_aperture";
   function RF_VA_Request_Wait
     (Handle              : RF_VA_Request_Handle;
      Timeout_MS          : Interfaces.Unsigned_32;
      Result              : access RF_VA_Result_V1;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_virtual_aperture_request_wait";
   function RF_VA_Request_Claim
     (Handle              : RF_VA_Request_Handle;
      Output              : access RF_VA_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_virtual_aperture_request_claim";
   function RF_VA_Request_Close
     (Handle              : access RF_VA_Request_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_virtual_aperture_request_close";
   function RF_VA_View
     (Handle              : RF_VA_Handle;
      Output              : access System.Address;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_virtual_aperture_view";
   function RF_VA_Close
     (Handle              : access RF_VA_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_virtual_aperture_close";
   function RF_VA_Submit_Job
     (Handle              : RF_VA_Handle;
      Config              : access constant RF_Job_Request_Config_V1;
      Output              : access RF_Job_Request_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_virtual_aperture_submit_job";
   function RF_VA_Submit_Job_V2
     (Handle              : RF_VA_Handle;
      Config              : access constant RF_Job_Request_Config_V2;
      Output              : access RF_Job_Request_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_virtual_aperture_submit_job_v2";
   function RF_VA_Submit_Job_V3
     (Handle              : RF_VA_Handle;
      Config              : access constant RF_Job_Request_Config_V3;
      Output              : access RF_Job_Request_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_virtual_aperture_submit_job_v3";
   function RF_VA_Submit_Job_V4
     (Handle              : RF_VA_Handle;
      Config              : access constant RF_Job_Request_Config_V4;
      Output              : access RF_Job_Request_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_virtual_aperture_submit_job_v4";
   function RF_Job_Request_Wait
     (Handle              : RF_Job_Request_Handle;
      Timeout_MS          : Interfaces.Unsigned_32;
      Result              : access RF_Job_Result_V1;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_job_request_wait";
   function RF_Job_Request_Claim
     (Handle              : RF_Job_Request_Handle;
      Output              : access RF_Job_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_job_request_claim";
   function RF_Job_Request_Close
     (Handle              : access RF_Job_Request_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_job_request_close";
   function RF_Job_View
     (Handle              : RF_Job_Handle;
      Output              : access System.Address;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_job_view";
   function RF_Job_Add_RX_Intervals
     (Handle              : RF_Job_Handle;
      Intervals           : RF_Job_Interval_Config_Span_V1;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_job_add_rx_intervals";
   function RF_Job_Flush
     (Handle              : RF_Job_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_job_flush";
   function RF_Job_Cancel_Remaining_Intervals
     (Handle              : RF_Job_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_job_cancel_remaining_intervals";
   function RF_Job_Extend_Event
     (Handle                      : RF_Job_Handle;
      Interval_ID, Event_ID       : Interfaces.Unsigned_32;
      Added_Duration_Femtoseconds : Interfaces.Integer_64;
      Diagnostic                  : System.Address;
      Diagnostic_Capacity         : Size_T;
      Diagnostic_Required         : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_job_extend_event";
   function RF_Job_Finalize
     (Handle              : RF_Job_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_job_finalize";
   function RF_Job_Wait_Status
     (Handle              : RF_Job_Handle;
      Timeout_MS          : Interfaces.Unsigned_32;
      Status              : access Interfaces.Unsigned_32;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_job_wait_status";
   function RF_Job_Cancel
     (Handle              : RF_Job_Handle;
      Result              : access RF_Job_Cancel_Result_V1;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_job_cancel";
   function RF_Job_Close
     (Handle              : access RF_Job_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_job_close";

   function RF_Admin_Open
     (Library_Path        : Interfaces.C.Strings.chars_ptr;
      Configuration       : Interfaces.C.Strings.chars_ptr;
      Output              : access RF_Admin_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_admin_open";
   function RF_Admin_Command_State
     (Handle              : RF_Admin_Handle;
      State               : Interfaces.Unsigned_32;
      Accepted            : access Interfaces.Unsigned_32;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_admin_command_state";
   function RF_Admin_Close
     (Handle              : access RF_Admin_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_admin_close";

   function RF_Data_Open
     (Library_Path        : Interfaces.C.Strings.chars_ptr;
      Configuration       : Interfaces.C.Strings.chars_ptr;
      Output              : access RF_Data_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_data_open";
   function RF_Data_Get_Provider_Version
     (Handle              : RF_Data_Handle;
      Output              : access Provider_Version_V1;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_data_get_provider_version";
   function RF_Data_Quantize_Duration
     (Handle              : RF_Data_Handle;
      Femtoseconds        : Interfaces.Integer_64;
      Output              : access Interfaces.Integer_64;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_data_quantize_duration";
   function RF_Data_Get_Physical_Data
     (Handle              : RF_Data_Handle;
      Face_ID             : Interfaces.Unsigned_32;
      Output              : access RF_Physical_Data_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_data_get_physical_data";
   function RF_Physical_Data_View
     (Handle              : RF_Physical_Data_Handle;
      Output              : access System.Address;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_physical_data_view";
   function RF_Physical_Data_Close
     (Handle              : access RF_Physical_Data_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_physical_data_close";
   function RF_Data_Get_MFA_Info
     (Handle              : RF_Data_Handle;
      Output              : access RF_MFA_Info_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_data_get_mfa_info";
   function RF_MFA_Info_View
     (Handle              : RF_MFA_Info_Handle;
      Output              : access System.Address;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_mfa_info_view";
   function RF_MFA_Info_Close
     (Handle              : access RF_MFA_Info_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_mfa_info_close";
   function RF_Data_Close
     (Handle              : access RF_Data_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_data_close";

   --  Task 033D RF ProductRxEndpoint ComplexINT16 raw records. Spans are raw
   --  addresses; sample counts are element counts; UTCTime components are the
   --  verbatim upstream int64 seconds and femtoseconds.
   type RF_Product_Rx_Config_V1 is record
      Data_Format           : Interfaces.Unsigned_32;
      Region_Size_Bytes     : Interfaces.Unsigned_64;
      Queue_Capacity        : Size_T;
      Max_Samples_Per_Event : Size_T;
   end record
   with Convention => C;
   type RF_Complex_I16_V1 is record
      Real : Interfaces.Integer_16;
      Imag : Interfaces.Integer_16;
   end record
   with Convention => C;
   type RF_Product_Rx_Metadata_V1 is record
      MEL_Protocol_Version_ID    : Interfaces.Unsigned_32;
      VA_Definition_ID           : Interfaces.Unsigned_32;
      VA_Instance_ID             : Interfaces.Unsigned_32;
      Job_Details_ID             : Interfaces.Unsigned_32;
      Job_Interval_ID            : Interfaces.Unsigned_32;
      LF_Type_ID                 : Interfaces.Unsigned_32;
      LF_Instance_ID             : Interfaces.Unsigned_32;
      Phase_Coherence_With_Prior : Interfaces.Unsigned_32;
      First_Rx_Event_Start_S     : Interfaces.Integer_64;
      First_Rx_Event_Start_FS    : Interfaces.Integer_64;
      Rx_Stream_IDs              : U32_Span_V1;
   end record
   with Convention => C;
   type RF_Product_Rx_Event_V1 is record
      Endpoint_ID : Interfaces.Unsigned_64;
      Data_Format : Interfaces.Unsigned_32;
      Samples     : Span_V1;
      Metadata    : RF_Product_Rx_Metadata_V1;
   end record
   with Convention => C;
   type RF_Product_Rx_Info_V1 is record
      Endpoint_ID          : Interfaces.Unsigned_64;
      Assigned_Data_Format : Interfaces.Unsigned_32;
   end record
   with Convention => C;
   type RF_Product_Rx_Request_Result_V1 is record
      Error_Code : Interfaces.Unsigned_32;
   end record
   with Convention => C;
   type RF_Product_Rx_Counters_V1 is record
      Callbacks_Received          : Interfaces.Unsigned_64;
      Products_Queued             : Interfaces.Unsigned_64;
      Products_Dropped_Queue_Full : Interfaces.Unsigned_64;
      Malformed_Or_Unsupported    : Interfaces.Unsigned_64;
      Allocation_Failures         : Interfaces.Unsigned_64;
      Callbacks_After_Close       : Interfaces.Unsigned_64;
   end record
   with Convention => C;
   function RF_Data_Submit_Product_Rx
     (Handle              : RF_Data_Handle;
      Config              : access constant RF_Product_Rx_Config_V1;
      Output              : access RF_Product_Rx_Request_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_data_submit_product_rx";
   function RF_Product_Rx_Request_Wait
     (Handle              : RF_Product_Rx_Request_Handle;
      Timeout_MS          : Interfaces.Unsigned_32;
      Output              : access RF_Product_Rx_Request_Result_V1;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_product_rx_request_wait";
   function RF_Product_Rx_Request_Claim
     (Handle              : RF_Product_Rx_Request_Handle;
      Output              : access RF_Product_Rx_Handle;
      Info                : access RF_Product_Rx_Info_V1;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_product_rx_request_claim";
   function RF_Product_Rx_Request_Close
     (Handle              : access RF_Product_Rx_Request_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_product_rx_request_close";
   function RF_Product_Rx_Receive
     (Handle              : RF_Product_Rx_Handle;
      Timeout_MS          : Interfaces.Unsigned_32;
      Output              : access RF_Product_Rx_Event_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_product_rx_receive";
   function RF_Product_Rx_Get_Counters
     (Handle              : RF_Product_Rx_Handle;
      Output              : access RF_Product_Rx_Counters_V1;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_product_rx_get_counters";
   function RF_Product_Rx_Close
     (Handle              : access RF_Product_Rx_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_product_rx_close";
   function RF_Product_Rx_Event_View
     (Handle              : RF_Product_Rx_Event_Handle;
      Output              : access System.Address;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_product_rx_event_view";
   function RF_Product_Rx_Event_Close
     (Handle              : access RF_Product_Rx_Event_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_product_rx_event_close";
   --  Task 034E1 private raw live-query ABI. Public values copy out all storage.
   RF_VA_Status_None                         : constant Interfaces.Unsigned_32 := 0;
   RF_VA_Status_Operational                  : constant Interfaces.Unsigned_32 := 1;
   RF_VA_Status_Degraded                     : constant Interfaces.Unsigned_32 := 2;
   RF_VA_Status_Failed                       : constant Interfaces.Unsigned_32 := 3;
   type RF_VA_Instance_List_Handle is new System.Address;
   Null_RF_VA_Instance_List                  : constant RF_VA_Instance_List_Handle :=
     RF_VA_Instance_List_Handle (System.Null_Address);
   type RF_VA_Instance_Status_Report_Handle is new System.Address;
   Null_RF_VA_Instance_Status_Report         : constant RF_VA_Instance_Status_Report_Handle :=
     RF_VA_Instance_Status_Report_Handle (System.Null_Address);
   type RF_VA_Local_Function_Status_V1 is record
      Local_Function_Type_ID : Interfaces.Unsigned_32;
      Statuses               : Span_V1;
   end record
   with Convention => C;
   type RF_VA_Instance_Status_Report_V1 is record
      VA_Instance_ID, Status : Interfaces.Unsigned_32;
      Local_Functions        : Span_V1;
   end record
   with Convention => C;
   function RF_VA_Get_TX_Radiated_Power
     (Handle              : RF_VA_Handle;
      Element_Group       : Interfaces.Unsigned_64;
      Power_Mode          : Interfaces.Unsigned_32;
      Attenuation         : Interfaces.C.double;
      Weight              : Interfaces.Unsigned_64;
      Frequency, U, V     : Interfaces.C.double;
      Instance            : Interfaces.Unsigned_32;
      Output              : access Interfaces.C.double;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with
     Import,
     Convention    => C,
     External_Name => "ams_mel_rf_virtual_aperture_get_tx_radiated_power";
   function RF_VA_Get_TX_Peak_Radiated_Power
     (Handle                 : RF_VA_Handle;
      Element_Group          : Interfaces.Unsigned_64;
      Power_Mode             : Interfaces.Unsigned_32;
      Attenuation, Frequency : Interfaces.C.double;
      Instance               : Interfaces.Unsigned_32;
      Output                 : access Interfaces.C.double;
      Diagnostic             : System.Address;
      Diagnostic_Capacity    : Size_T;
      Diagnostic_Required    : access Size_T) return Interfaces.Integer_32
   with
     Import,
     Convention    => C,
     External_Name => "ams_mel_rf_virtual_aperture_get_tx_peak_radiated_power";
   function RF_VA_Get_TX_Aperture_Gain
     (Handle              : RF_VA_Handle;
      Element_Group       : Interfaces.Unsigned_64;
      Power_Mode          : Interfaces.Unsigned_32;
      Weight              : Interfaces.Unsigned_64;
      Frequency, U, V     : Interfaces.C.double;
      Instance            : Interfaces.Unsigned_32;
      Output              : access Interfaces.C.double;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with
     Import,
     Convention    => C,
     External_Name => "ams_mel_rf_virtual_aperture_get_tx_aperture_gain";
   function RF_VA_Get_Max_TX_Attenuation
     (Handle               : RF_VA_Handle;
      Element_Group        : Interfaces.Unsigned_64;
      Power_Mode, Instance : Interfaces.Unsigned_32;
      Output               : access Interfaces.C.double;
      Diagnostic           : System.Address;
      Diagnostic_Capacity  : Size_T;
      Diagnostic_Required  : access Size_T) return Interfaces.Integer_32
   with
     Import,
     Convention    => C,
     External_Name => "ams_mel_rf_virtual_aperture_get_max_tx_attenuation";
   function RF_VA_Get_ID
     (Handle              : RF_VA_Handle;
      Output              : access Interfaces.Unsigned_32;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_virtual_aperture_get_id";
   function RF_VA_Cached_Waveform_Supported
     (Handle              : RF_VA_Handle;
      Output              : access Interfaces.Unsigned_32;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with
     Import,
     Convention    => C,
     External_Name => "ams_mel_rf_virtual_aperture_is_cached_waveform_supported";
   function RF_VA_Dynamic_Weights_Supported
     (Handle              : RF_VA_Handle;
      Output              : access Interfaces.Unsigned_32;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with
     Import,
     Convention    => C,
     External_Name => "ams_mel_rf_virtual_aperture_dynamic_weights_supported";
   type RF_VA_LF_List_Handle is new System.Address;
   Null_RF_VA_LF_List                        : constant RF_VA_LF_List_Handle :=
     RF_VA_LF_List_Handle (System.Null_Address);
   type RF_VA_LF_Status_Handle is new System.Address;
   Null_RF_VA_LF_Status                      : constant RF_VA_LF_Status_Handle :=
     RF_VA_LF_Status_Handle (System.Null_Address);
   type RF_VA_Local_Function_Info_V1 is record
      Local_Function_Type_ID : Interfaces.Unsigned_32;
      Instance_Count         : Interfaces.Unsigned_64;
   end record
   with Convention => C;
   subtype RF_VA_Local_Function_Info_Span_V1 is Span_V1;
   function RF_VA_Get_Local_Functions
     (Handle              : RF_VA_Handle;
      Output              : access RF_VA_LF_List_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_virtual_aperture_get_local_functions";
   function RF_VA_LF_List_View
     (Handle              : RF_VA_LF_List_Handle;
      Output              : access RF_VA_Local_Function_Info_Span_V1;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_va_local_function_list_view";
   function RF_VA_LF_List_Close
     (Handle              : access RF_VA_LF_List_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_va_local_function_list_close";
   function RF_VA_Get_Local_Function_Status
     (Handle                                 : RF_VA_Handle;
      VA_Instance_ID, Local_Function_Type_ID : Interfaces.Unsigned_32;
      Output                                 : access RF_VA_LF_Status_Handle;
      Diagnostic                             : System.Address;
      Diagnostic_Capacity                    : Size_T;
      Diagnostic_Required                    : access Size_T) return Interfaces.Integer_32
   with
     Import,
     Convention    => C,
     External_Name => "ams_mel_rf_virtual_aperture_get_local_function_status";
   function RF_VA_LF_Status_View
     (Handle              : RF_VA_LF_Status_Handle;
      Output              : access Span_V1;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_va_local_function_status_view";
   function RF_VA_LF_Status_Close
     (Handle              : access RF_VA_LF_Status_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_va_local_function_status_close";
   function RF_VA_Get_Status
     (Handle              : RF_VA_Handle;
      Output              : access Interfaces.Unsigned_32;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_virtual_aperture_get_status";
   function RF_VA_Get_Instance_Status
     (Handle              : RF_VA_Handle;
      Instance_ID         : Interfaces.Unsigned_32;
      Output              : access Interfaces.Unsigned_32;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_virtual_aperture_get_instance_status";
   function RF_VA_Get_All_Instances
     (Handle              : RF_VA_Handle;
      Output              : access RF_VA_Instance_List_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_virtual_aperture_get_all_instances";
   function RF_VA_Get_Instances
     (Handle              : RF_VA_Handle;
      Face_ID             : Interfaces.Unsigned_32;
      Output              : access RF_VA_Instance_List_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_virtual_aperture_get_instances";
   function RF_VA_Instance_List_View
     (Handle              : RF_VA_Instance_List_Handle;
      Output              : access Span_V1;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_va_instance_list_view";
   function RF_VA_Instance_List_Close
     (Handle              : access RF_VA_Instance_List_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_va_instance_list_close";
   function RF_VA_Get_Instance_Status_Report
     (Handle              : RF_VA_Handle;
      Instance_ID         : Interfaces.Unsigned_32;
      Output              : access RF_VA_Instance_Status_Report_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with
     Import,
     Convention    => C,
     External_Name => "ams_mel_rf_virtual_aperture_get_instance_status_report";
   function RF_VA_Instance_Status_Report_View
     (Handle              : RF_VA_Instance_Status_Report_Handle;
      Output              : access System.Address;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_va_instance_status_report_view";
   function RF_VA_Instance_Status_Report_Close
     (Handle              : access RF_VA_Instance_Status_Report_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_va_instance_status_report_close";
   type RF_VA_Subscription_Handle is new System.Address;
   Null_RF_VA_Subscription                   : constant RF_VA_Subscription_Handle :=
     RF_VA_Subscription_Handle (System.Null_Address);
   type RF_VA_Subscription_Statistics_V1 is record
      Callback_Entries, Callbacks_Coalesced, Notifications_Delivered, Callbacks_After_Stop :
        Interfaces.Unsigned_64;
      Pending, Stopped                                                                     :
        Interfaces.Unsigned_32;
   end record
   with Convention => C;
   function RF_VA_Subscription_Open
     (VA                  : RF_VA_Handle;
      Output              : access RF_VA_Subscription_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_va_status_subscription_open";
   function RF_VA_Subscription_Wait
     (Object               : RF_VA_Subscription_Handle;
      Timeout_Milliseconds : Interfaces.Unsigned_32;
      Diagnostic           : System.Address;
      Diagnostic_Capacity  : Size_T;
      Diagnostic_Required  : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_va_status_subscription_wait";
   function RF_VA_Subscription_Get_Statistics
     (Object              : RF_VA_Subscription_Handle;
      Output              : access RF_VA_Subscription_Statistics_V1;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with
     Import,
     Convention    => C,
     External_Name => "ams_mel_rf_va_status_subscription_get_statistics";
   function RF_VA_Subscription_Unsubscribe
     (VA                  : RF_VA_Handle;
      Object              : RF_VA_Subscription_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_va_status_subscription_unsubscribe";
   function RF_VA_Subscription_Close
     (Object              : access RF_VA_Subscription_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_va_status_subscription_close";
   type RF_Element_Group_Snapshot_Handle is new System.Address;
   Null_RF_Element_Group_Snapshot            : constant RF_Element_Group_Snapshot_Handle :=
     RF_Element_Group_Snapshot_Handle (System.Null_Address);
   type RF_Element_Group_Snapshot_Options_V1 is record
      Include_Data_Pipes : Interfaces.Unsigned_32;
   end record
   with Convention => C;
   type RF_Data_Pipe_Info_V1 is record
      Lookup_Label, Label     : String_View_V1;
      Associated_Endpoint_IDs : U64_Span_V1;
   end record
   with Convention => C;
   subtype RF_Data_Pipe_Info_Span_V1 is Span_V1;
   type RF_Element_Group_Descriptor_V1 is record
      Lookup_Label, Label : String_View_V1;
      Mode                : RF_Element_Group_Mode;
      Max_RF_Bandwidth_Hz,
      Max_Sample_Rate_Samples_Per_Second,
      Max_Data_Rate_Bits_Per_Second,
      Max_Duty_Factor     : Interfaces.C.double;
      Data_Pipes          : RF_Data_Pipe_Info_Span_V1;
   end record
   with Convention => C;
   subtype RF_Element_Group_Descriptor_Span_V1 is Span_V1;
   type RF_Element_Group_Snapshot_V1 is record
      Data_Pipes_Included : Interfaces.Unsigned_32;
      Descriptors         : RF_Element_Group_Descriptor_Span_V1;
   end record
   with Convention => C;
   function RF_VA_Get_Element_Groups
     (VA                  : RF_VA_Handle;
      Options             : access constant RF_Element_Group_Snapshot_Options_V1;
      Output              : access RF_Element_Group_Snapshot_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_virtual_aperture_get_element_groups";
   function RF_Element_Group_Snapshot_View
     (Object              : RF_Element_Group_Snapshot_Handle;
      Output              : access System.Address;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_element_group_snapshot_view";
   function RF_Element_Group_Snapshot_Close
     (Object              : access RF_Element_Group_Snapshot_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_element_group_snapshot_close";
   type RF_VA_Data_Pipe_Connections_Snapshot_Handle is new System.Address;
   Null_RF_VA_Data_Pipe_Connections_Snapshot :
     constant RF_VA_Data_Pipe_Connections_Snapshot_Handle :=
       RF_VA_Data_Pipe_Connections_Snapshot_Handle (System.Null_Address);
   type RF_VA_Data_Pipe_Group_V1 is record
      Element_Group_Lookup_Label : String_View_V1;
      Data_Pipes                 : RF_Data_Pipe_Info_Span_V1;
   end record
   with Convention => C;
   subtype RF_VA_Data_Pipe_Group_Span_V1 is Span_V1;
   type RF_VA_Data_Pipe_Connections_Snapshot_V1 is record
      Groups : RF_VA_Data_Pipe_Group_Span_V1;
   end record
   with Convention => C;
   function RF_VA_Get_Data_Pipes
     (VA                  : RF_VA_Handle;
      Output              : access RF_VA_Data_Pipe_Connections_Snapshot_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with Import, Convention => C, External_Name => "ams_mel_rf_virtual_aperture_get_data_pipes";
   function RF_VA_Data_Pipe_Connections_Snapshot_View
     (Object              : RF_VA_Data_Pipe_Connections_Snapshot_Handle;
      Output              : access System.Address;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with
     Import,
     Convention    => C,
     External_Name => "ams_mel_rf_va_data_pipe_connections_snapshot_view";
   function RF_VA_Data_Pipe_Connections_Snapshot_Close
     (Object              : access RF_VA_Data_Pipe_Connections_Snapshot_Handle;
      Diagnostic          : System.Address;
      Diagnostic_Capacity : Size_T;
      Diagnostic_Required : access Size_T) return Interfaces.Integer_32
   with
     Import,
     Convention    => C,
     External_Name => "ams_mel_rf_va_data_pipe_connections_snapshot_close";
   --  Existing view/span records are used through pointers elsewhere. These
   --  private derived representations supply C by-value parameter convention
   --  without changing their frozen records or introducing public Ada C types.
   type String_View_By_Copy_V1 is new String_View_V1 with Convention => C_Pass_By_Copy;
   type U64_Span_By_Copy_V1 is new U64_Span_V1 with Convention => C_Pass_By_Copy;
   function RF_VA_Associate_Data_Pipe_Endpoint
     (VA                                                 : RF_VA_Handle;
      Element_Group_Lookup_Label, Data_Pipe_Lookup_Label : String_View_By_Copy_V1;
      Endpoint_ID                                        : Interfaces.Unsigned_64;
      Accepted                                           : access Interfaces.Unsigned_32;
      Diagnostic                                         : System.Address;
      Diagnostic_Capacity                                : Size_T;
      Diagnostic_Required                                : access Size_T)
      return Interfaces.Integer_32
   with
     Import,
     Convention    => C,
     External_Name => "ams_mel_rf_virtual_aperture_associate_data_pipe_endpoint";
   function RF_VA_Associate_Data_Pipe_Endpoints
     (VA                                                 : RF_VA_Handle;
      Element_Group_Lookup_Label, Data_Pipe_Lookup_Label : String_View_By_Copy_V1;
      Endpoint_IDs                                       : U64_Span_By_Copy_V1;
      Accepted                                           : access Interfaces.Unsigned_32;
      Diagnostic                                         : System.Address;
      Diagnostic_Capacity                                : Size_T;
      Diagnostic_Required                                : access Size_T)
      return Interfaces.Integer_32
   with
     Import,
     Convention    => C,
     External_Name => "ams_mel_rf_virtual_aperture_associate_data_pipe_endpoints";
end AMS.MEL_C_API;
