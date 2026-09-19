private with Ada.Finalization;
private with AMS.MEL_C_API;
with AMS.MEL.IR.Channel;

package AMS.MEL.IR.Health_Status is
   type Health_Config is private;
   function Create_Config
     (Channel_ID : UCI_ID; Platform_ID : UCI_ID; Sensor_Location : Component_Location)
      return Health_Config;

   type Health_Channel is limited private;
   function Open (Parent : Session; Config : Health_Config) return Health_Channel;
   function Is_Open (Channel : Health_Channel) return Boolean;
   procedure Enable (Channel : in out Health_Channel);
   function Capabilities (Channel : Health_Channel) return IR.Channel.Channel_Capability;
   procedure Close (Channel : in out Health_Channel);

private
   type Health_Config is record
      Channel, Platform : UCI_ID;
      Location          : Component_Location;
   end record;
   type Health_Channel is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.Health_Handle := AMS.MEL_C_API.Null_Health;
   end record;
   overriding
   procedure Finalize (Channel : in out Health_Channel);
end AMS.MEL.IR.Health_Status;
