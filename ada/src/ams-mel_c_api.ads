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

   Success          : constant Interfaces.Integer_32 := 0;
   Buffer_Too_Small : constant Interfaces.Integer_32 := 7;
   Timeout          : constant Interfaces.Integer_32 := 9;
   Stream_Stopped   : constant Interfaces.Integer_32 := 10;
   Command_Rejected : constant Interfaces.Integer_32 := 12;

   subtype Size_T is Interfaces.C.size_t;
   type Session_Handle is new System.Address;
   Null_Session                  : constant Session_Handle := Session_Handle (System.Null_Address);
   type Stream_Handle is new System.Address;
   Null_Stream                   : constant Stream_Handle := Stream_Handle (System.Null_Address);
   type Frame_Snapshot_Handle is new System.Address;
   Null_Frame_Snapshot           : constant Frame_Snapshot_Handle :=
     Frame_Snapshot_Handle (System.Null_Address);
   type C2_Handle is new System.Address;
   Null_C2                       : constant C2_Handle := C2_Handle (System.Null_Address);
   type Mode_Request_Handle is new System.Address;
   Null_Mode_Request             : constant Mode_Request_Handle :=
     Mode_Request_Handle (System.Null_Address);
   type Return_Request_Handle is new System.Address;
   Null_Return_Request           : constant Return_Request_Handle :=
     Return_Request_Handle (System.Null_Address);
   type Comms_Request_Handle is new System.Address;
   Null_Comms_Request            : constant Comms_Request_Handle :=
     Comms_Request_Handle (System.Null_Address);
   type Capability_Handle is new System.Address;
   Null_Capability               : constant Capability_Handle :=
     Capability_Handle (System.Null_Address);
   type Metadata_Handle is new System.Address;
   Null_Metadata                 : constant Metadata_Handle :=
     Metadata_Handle (System.Null_Address);
   type Metadata_Event_Handle is new System.Address;
   Null_Metadata_Event           : constant Metadata_Event_Handle :=
     Metadata_Event_Handle (System.Null_Address);
   type Health_Handle is new System.Address;
   Null_Health                   : constant Health_Handle := Health_Handle (System.Null_Address);
   type Health_Metadata_Handle is new System.Address;
   Null_Health_Metadata          : constant Health_Metadata_Handle :=
     Health_Metadata_Handle (System.Null_Address);
   type Health_Event_Handle is new System.Address;
   Null_Health_Event             : constant Health_Event_Handle :=
     Health_Event_Handle (System.Null_Address);
   type Image_Metadata_Handle is new System.Address;
   Null_Image_Metadata           : constant Image_Metadata_Handle :=
     Image_Metadata_Handle (System.Null_Address);
   type Image_Metadata_Event_Handle is new System.Address;
   Null_Image_Metadata_Event     : constant Image_Metadata_Event_Handle :=
     Image_Metadata_Event_Handle (System.Null_Address);
   type Navigation_Request_Handle is new System.Address;
   Null_Navigation_Request       : constant Navigation_Request_Handle :=
     Navigation_Request_Handle (System.Null_Address);
   type Instrumentation_Handle is new System.Address;
   Null_Instrumentation          : constant Instrumentation_Handle :=
     Instrumentation_Handle (System.Null_Address);
   type Instrumentation_Request_Handle is new System.Address;
   Null_Instrumentation_Request  : constant Instrumentation_Request_Handle :=
     Instrumentation_Request_Handle (System.Null_Address);
   type Instrumentation_Metadata_Handle is new System.Address;
   Null_Instrumentation_Metadata : constant Instrumentation_Metadata_Handle :=
     Instrumentation_Metadata_Handle (System.Null_Address);
   type Instrumentation_Event_Handle is new System.Address;
   Null_Instrumentation_Event    : constant Instrumentation_Event_Handle :=
     Instrumentation_Event_Handle (System.Null_Address);
   type Track_Handle is new System.Address;
   Null_Track                    : constant Track_Handle := Track_Handle (System.Null_Address);
   type Track_Metadata_Handle is new System.Address;
   Null_Track_Metadata           : constant Track_Metadata_Handle :=
     Track_Metadata_Handle (System.Null_Address);
   type Track_Event_Handle is new System.Address;
   Null_Track_Event              : constant Track_Event_Handle :=
     Track_Event_Handle (System.Null_Address);
   type Track_Update_Request_Handle is new System.Address;
   Null_Track_Update_Request     : constant Track_Update_Request_Handle :=
     Track_Update_Request_Handle (System.Null_Address);

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
   type IR_Track_Event_V1 is record
      Kind         : Interfaces.Unsigned_32;
      Track_Report : IR_Track_Report_V1;
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
   type String_View_Array is array (Positive range <>) of aliased String_View_V1
   with Convention => C;
   type String_View_Span_V1 is record
      Data : System.Address;
      Size : Size_T;
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
   type IR_Directional_V1 is record
      X, Y, Z : Interfaces.C.double;
   end record
   with Convention => C;
   type IR_Quaternion_V1 is record
      X, Y, Z, W : Interfaces.C.double;
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
   type IR_Nav_Error_V1 is record
      X, Y, Z, W : Interfaces.C.double;
   end record
   with Convention => C;
   type IR_Uncertainty_V1 is record
      Sensor_Uncertainties, Platform_Uncertainties : Interfaces.Unsigned_32;
   end record
   with Convention => C;
   type IR_Orientation_V1 is record
      Kind       : Interfaces.Unsigned_32;
      Euler      : Euler_V1;
      Quaternion : IR_Quaternion_V1;
   end record
   with Convention => C;
   type IR_Sensor_Inertial_State_V1 is record
      System_Time_NS                   : Interfaces.Integer_64;
      Q_XYZW, Q_ECEF_XYZW              : IR_Quaternion_V1;
      Sensor_Position, Sensor_Velocity : IR_Directional_V1;
      Uncertainties                    : IR_Uncertainty_V1;
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
end AMS.MEL_C_API;
