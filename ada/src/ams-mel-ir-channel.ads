with Ada.Containers.Vectors;
private with Ada.Finalization;
private with Ada.Strings.Unbounded;
private with AMS.MEL_C_API;
with Interfaces;

package AMS.MEL.IR.Channel is
   --  Canonical common command identifier. AMS.MEL.IR.C2.Command_ID is a
   --  source-compatible subtype of this type.
   type Command_ID is mod 2**32 with Size => 32;
   type Comms_Channel_ID is mod 2**32 with Size => 32;
   type Comms_Request_ID is mod 2**32 with Size => 32;
   type Comms_Test_Report is record
      Command_ID : AMS.MEL.IR.Channel.Command_ID;
      Request_ID : Comms_Request_ID;
   end record;

   type Channel_Type is
     (IRST_Track,
      IRST_Image,
      Command_And_Control,
      Scheduling,
      Health_And_Status,
      Instrumentation,
      Stacked_Image,
      Reserved_1,
      Reserved_2);
   type Pixel_Format is (Mono, RGB, Bayer);
   type Sensor_Type is
     (Unspecified, Gimbal_Horizontal, Gimbal_Vertical, Gimbal_Rotation, Step_Stare);
   type Metadata_Capability is
     (Bad_Pixel_List,
      Optical_Distortion_Map,
      LF_Status,
      Line_Of_Sight_Report,
      Line_Of_Sight_Quaternion,
      Line_Of_Sight_Euler,
      MFA_Status,
      MFA_Status_Detailed,
      BIT_Configuration,
      Command_Status,
      BIT_Status,
      Candidate_Object_Message,
      Task_Executing_Rep,
      Subsystem_Status_Resp,
      Execute_Task_Ack,
      Sched_Created_Rep,
      IRST_Track_Report,
      Channel_Comms_Test_Rep,
      Camera_Command_Resp,
      Camera_Protect_Cmd_Resp,
      Instrumentation_Report,
      Navigation_Report_Resp,
      Request_System_Track_Data,
      Update_Track_List_Response,
      LOS_3D_Kinematics_Type,
      Candidate_Object_Preproc_Message,
      Task_Events,
      Scan_Performance_Report,
      Reserved_3,
      Reserved_5,
      Reserved_9,
      Reserved_10);
   type Band_Type is
     (Invalid,
      Multiband,
      IR_Far,
      IR_Near,
      IR_Longwave,
      IR_Midwave,
      IR_Shortwave,
      Visible_White,
      Visible_Red,
      Visible_Green,
      Visible_Blue,
      UVA,
      UVB,
      UVC,
      UV_Vacuum);
   type Coordinate_System_Type is (LLA, ECEF, NED_Platform, NED_Sensor);
   type Band_Info is record
      Kind                               : Band_Type;
      Min_Wavelength_M, Max_Wavelength_M : Long_Float;
   end record;

   package Sensor_Type_Vectors is new Ada.Containers.Vectors (Positive, Sensor_Type);
   package Channel_Type_Vectors is new Ada.Containers.Vectors (Positive, Channel_Type);
   package Metadata_Vectors is new Ada.Containers.Vectors (Positive, Metadata_Capability);
   package Band_Info_Vectors is new Ada.Containers.Vectors (Positive, Band_Info);
   type Image_Band is record
      Band_Index : Interfaces.Unsigned_32;
      Bands      : Band_Info_Vectors.Vector;
   end record;
   package Image_Band_Vectors is new Ada.Containers.Vectors (Positive, Image_Band);
   package Coordinate_Vectors is new Ada.Containers.Vectors (Positive, Coordinate_System_Type);

   type Channel_Capability is private;
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
        Coordinate_Vectors.Vector) return Channel_Capability;

   function Channel_ID (Value : Channel_Capability) return UCI_ID;
   function Height (Value : Channel_Capability) return Interfaces.Unsigned_32;
   function Width (Value : Channel_Capability) return Interfaces.Unsigned_32;
   function Bit_Depth (Value : Channel_Capability) return Interfaces.Unsigned_32;
   function Row_Pitch (Value : Channel_Capability) return Interfaces.Unsigned_32;
   function Buffer_Size (Value : Channel_Capability) return Interfaces.Unsigned_32;
   function Image_Size (Value : Channel_Capability) return Interfaces.Unsigned_32;
   function Number_Of_Bands (Value : Channel_Capability) return Interfaces.Unsigned_32;
   function Format (Value : Channel_Capability) return Pixel_Format;
   function Sensor_Type_Count (Value : Channel_Capability) return Natural;
   function Sensor_Type_At (Value : Channel_Capability; Index : Positive) return Sensor_Type;
   function Platform_ID (Value : Channel_Capability) return UCI_ID;
   function Sensor_Location (Value : Channel_Capability) return Component_Location;
   function Channel_Type_Count (Value : Channel_Capability) return Natural;
   function Channel_Type_At (Value : Channel_Capability; Index : Positive) return Channel_Type;
   function Task_Schedule_Depth (Value : Channel_Capability) return Interfaces.Unsigned_32;
   function ODC_Available (Value : Channel_Capability) return Boolean;
   function NUC_Available (Value : Channel_Capability) return Boolean;
   function Metadata_Capability_Count (Value : Channel_Capability) return Natural;
   function Metadata_Capability_At
     (Value : Channel_Capability; Index : Positive) return Metadata_Capability;
   function Has_Metadata_Capability
     (Value : Channel_Capability; Item : Metadata_Capability) return Boolean;
   function Image_Band_Count (Value : Channel_Capability) return Natural;
   function Image_Band_Index_At
     (Value : Channel_Capability; Index : Positive) return Interfaces.Unsigned_32;
   function Image_Band_Info_Count (Value : Channel_Capability; Index : Positive) return Natural;
   function Image_Band_Info_At
     (Value : Channel_Capability; Band_Index, Info_Index : Positive) return Band_Info;
   function Nav_Frame_Count (Value : Channel_Capability) return Natural;
   function Nav_Frame_At
     (Value : Channel_Capability; Index : Positive) return Coordinate_System_Type;

   --  Safe common Channel facade (Task 032B2).
   --
   --  A View is created only by a typed family's As_Channel conversion:
   --  C2.As_Channel, Image.As_Channel, Health_Status.As_Channel,
   --  Instrumentation.As_Channel, or Track.As_Channel. It is a controlled
   --  owner of exactly one native weak common Channel view. It does NOT own
   --  the typed family state, Session, provider Channel, Control, or provider
   --  library: after the typed owner and its Session close, common operations
   --  on a still-open View raise Provider_Error, while Close still succeeds.
   --  Closing or finalizing a View never closes, enables, disables, or
   --  detaches the typed source owner and never cancels pending requests.
   --  Operations on one View and Close of that same View must be externally
   --  serialized; distinct Views need no serialization.
   subtype View is AMS.MEL.IR.Channel_View;
   function Is_Open (Channel : View) return Boolean;
   procedure Close (Channel : in out View);
   --  Idempotent. Destroys only the weak native view.

   type Outcome is (Success, Rejected);
   type Error_Code is
     (None,
      Invalid_ID,
      Invalid_State,
      Invalid_Parameters,
      Insufficient_Permissions,
      Insufficient_Resources,
      Insufficient_Local_Resources,
      Insufficient_Remote_Resources,
      Unsupported);
   type Command_Return is (Return_Success, Bad_Pointer, Fail, Not_Supported, Not_Implemented);
   --  Success means RequestFor<Return> completed with a Command_Return value;
   --  that value may be Fail. Rejected represents upstream ErrorOr(Error).

   --  Submissions are valid while the family's common lifecycle admits
   --  requests (C2/Health/Instrumentation/Track: Attached or Enabled; Image:
   --  Attached or Running). Session admission refusal raises
   --  AMS.MEL.Resource_Exhausted; any other submission failure, including an
   --  expired or closed View, raises Provider_Error. An admitted request owns
   --  the native family graph independently of the View, typed owner, and
   --  Session. Timeout_Error is inherited from AMS.MEL.IR; timeout never
   --  cancels or consumes a request, and a later Wait returns the identical
   --  cached terminal result. Close must not race Wait on the same request.
   type Return_Request is limited private;
   function Send_Keep_Alive (Channel : View) return Return_Request;
   function Is_Open (Request : Return_Request) return Boolean;
   type Return_Result is private;
   function Status (Result : Return_Result) return Outcome;
   function Value (Result : Return_Result) return Command_Return
   with Pre => Status (Result) = Success;
   function Rejection_Code (Result : Return_Result) return Error_Code
   with Pre => Status (Result) = Rejected;
   function Description (Result : Return_Result) return String
   with Pre => Status (Result) = Rejected;
   function Wait (Request : Return_Request; Timeout_Milliseconds : Natural) return Return_Result;
   procedure Close (Request : in out Return_Request);

   type Comms_Request is limited private;
   function Submit_Comms_Test
     (Channel    : View;
      Channel_ID : Comms_Channel_ID;
      Command_ID : AMS.MEL.IR.Channel.Command_ID;
      Request_ID : Comms_Request_ID) return Comms_Request;
   function Is_Open (Request : Comms_Request) return Boolean;
   type Comms_Result is private;
   function Status (Result : Comms_Result) return Outcome;
   function Report (Result : Comms_Result) return Comms_Test_Report
   with Pre => Status (Result) = Success;
   function Rejection_Code (Result : Comms_Result) return Error_Code
   with Pre => Status (Result) = Rejected;
   function Description (Result : Comms_Result) return String
   with Pre => Status (Result) = Rejected;
   function Wait (Request : Comms_Request; Timeout_Milliseconds : Natural) return Comms_Result;
   procedure Close (Request : in out Comms_Request);

   --  Returns an owned snapshot that remains valid after View Close, typed
   --  owner Close, Session Close, and provider unload.
   function Capabilities (Channel : View) return Channel_Capability;
private
   package US renames Ada.Strings.Unbounded;
   type Return_Owner is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.Return_Request_Handle := AMS.MEL_C_API.Null_Return_Request;
   end record;
   overriding
   procedure Finalize (Request : in out Return_Owner);
   type Return_Request is limited record
      Owner : Return_Owner;
   end record;
   type Return_Result is record
      Result_Status : Outcome := Success;
      Result_Value  : Command_Return := Return_Success;
      Result_Code   : Error_Code := None;
      Result_Text   : US.Unbounded_String;
   end record;
   type Comms_Owner is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.Comms_Request_Handle := AMS.MEL_C_API.Null_Comms_Request;
   end record;
   overriding
   procedure Finalize (Request : in out Comms_Owner);
   type Comms_Request is limited record
      Owner : Comms_Owner;
   end record;
   type Comms_Result is record
      Result_Status : Outcome := Success;
      Result_Report : Comms_Test_Report := (0, 0);
      Result_Code   : Error_Code := None;
      Result_Text   : US.Unbounded_String;
   end record;
   type Channel_Capability is record
      ID, Platform                                            : UCI_ID;
      H, W, Depth, Pitch, Buffer, Image, Band_Count, Schedule : Interfaces.Unsigned_32;
      Pixel                                                   : Pixel_Format := Mono;
      Sensors                                                 : Sensor_Type_Vectors.Vector;
      Location                                                : Component_Location;
      Channels                                                : Channel_Type_Vectors.Vector;
      ODC, NUC                                                : Boolean := False;
      Metadata                                                : Metadata_Vectors.Vector;
      Bands                                                   : Image_Band_Vectors.Vector;
      Frames                                                  : Coordinate_Vectors.Vector;
   end record;
end AMS.MEL.IR.Channel;
