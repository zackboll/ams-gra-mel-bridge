private with Ada.Finalization;
private with Ada.Strings.Unbounded;
private with AMS.MEL_C_API;
with AMS.MEL.IR.Channel;
with Interfaces;

--  Conditionally required IR MEL Instrumentation family
--  (@RequiredIfInstrumentation). This package implements only the
--  Instrumentation-specific conditional surface -- send
--  (InstrumentationLevelCmd) and the InstrumentationReport metadata callback --
--  plus Enable and ChannelCapability. Instrumentation-specific copies of the
--  inherited generic Channel services (KeepAlive, CommsTest, the
--  ChannelCommsTest callback, and buffer registration) are deliberately absent;
--  those should be generalized across non-C2 channel families rather than
--  cloned into every family.
--
--  Facade lifecycle policy: Open attaches the upstream channel. Capabilities
--  is valid while attached or enabled. Metadata registration may occur while
--  attached. Enable explicitly calls upstream Channel::enable.
--  Instrumentation-specific submission requires an enabled channel.

package AMS.MEL.IR.Instrumentation is
   type Instrumentation_Config is private;
   function Create_Config
     (Channel_ID : UCI_ID; Platform_ID : UCI_ID; Sensor_Location : Component_Location)
      return Instrumentation_Config;

   type Instrumentation_Channel is limited private;
   function Open (Parent : Session; Config : Instrumentation_Config) return Instrumentation_Channel;
   function Is_Open (Channel : Instrumentation_Channel) return Boolean;
   procedure Enable (Channel : in out Instrumentation_Channel);
   function Capabilities (Channel : Instrumentation_Channel) return IR.Channel.Channel_Capability;
   procedure Close (Channel : in out Instrumentation_Channel);

   --  Upstream Priority is exactly Normal = 0 and Debug = 1; upstream defines
   --  no MaxExclusive value.
   type Priority is (Normal, Debug);
   for Priority use (Normal => 0, Debug => 1);

   type Instrumentation_Level_Command is record
      Command_ID : Interfaces.Unsigned_32;
      Priority   : Instrumentation.Priority;
   end record;

   type Instrumentation_Report is record
      Command_ID   : Interfaces.Unsigned_32;
      Size         : Interfaces.Unsigned_32;
      Timestamp_NS : Long_Long_Integer;
      Priority     : Instrumentation.Priority;
   end record;

   type Instrumentation_Outcome is (Success, Rejected);
   type Instrumentation_Error_Code is
     (None,
      Invalid_ID,
      Invalid_State,
      Invalid_Parameters,
      Insufficient_Permissions,
      Insufficient_Resources,
      Insufficient_Local_Resources,
      Insufficient_Remote_Resources,
      Unsupported);
   type Instrumentation_Result is private;
   function Status (Result : Instrumentation_Result) return Instrumentation_Outcome;
   function Report (Result : Instrumentation_Result) return Instrumentation_Report
   with Pre => Status (Result) = Success;
   function Rejection_Code (Result : Instrumentation_Result) return Instrumentation_Error_Code
   with Pre => Status (Result) = Rejected;
   function Description (Result : Instrumentation_Result) return String
   with Pre => Status (Result) = Rejected;

   --  Submission requires an enabled channel. Close is not cancellation: a
   --  pending request keeps the underlying provider future and channel alive
   --  independently of the public Instrumentation_Channel and Session owners.
   type Instrumentation_Request is limited private;
   function Submit
     (Channel : Instrumentation_Channel; Command : Instrumentation_Level_Command)
      return Instrumentation_Request;
   function Is_Open (Request : Instrumentation_Request) return Boolean;
   --  Timeout_Error is inherited from AMS.MEL.IR. Timeout never cancels or
   --  consumes the request. Wait may be repeated and a later Wait returns the
   --  identical cached terminal result. Close must not race Wait on the same
   --  Instrumentation_Request.
   function Wait
     (Request : Instrumentation_Request; Timeout_Milliseconds : Natural)
      return Instrumentation_Result;
   procedure Close (Request : in out Instrumentation_Request);

private
   package US renames Ada.Strings.Unbounded;
   type Instrumentation_Config is record
      Channel, Platform : UCI_ID;
      Location          : Component_Location;
   end record;
   type Instrumentation_Channel is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.Instrumentation_Handle := AMS.MEL_C_API.Null_Instrumentation;
   end record;
   overriding
   procedure Finalize (Channel : in out Instrumentation_Channel);

   type Instrumentation_Result is record
      Result_Status : Instrumentation_Outcome := Success;
      Result_Report : Instrumentation_Report := (0, 0, 0, Normal);
      Result_Code   : Instrumentation_Error_Code := None;
      Result_Text   : US.Unbounded_String;
   end record;
   type Instrumentation_Request_Owner is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.Instrumentation_Request_Handle :=
        AMS.MEL_C_API.Null_Instrumentation_Request;
   end record;
   overriding
   procedure Finalize (Request : in out Instrumentation_Request_Owner);
   type Instrumentation_Request is limited record
      Owner : Instrumentation_Request_Owner;
   end record;
end AMS.MEL.IR.Instrumentation;
