private with Ada.Finalization;
private with Ada.Strings.Unbounded;
with AMS.MEL.IR.Channel;
private with AMS.MEL_C_API;

package AMS.MEL.IR.C2.Common is
   function Send_Keep_Alive (Channel : Control_Channel) return Return_Request;
   type Comms_Request is limited private;
   function Submit_Comms_Test
     (Channel : Control_Channel; Channel_ID : IR.Channel.Comms_Channel_ID;
      Command_ID : C2.Command_ID; Request_ID : IR.Channel.Comms_Request_ID)
      return Comms_Request;
   type Comms_Result is private;
   function Status (Result : Comms_Result) return Outcome;
   function Report (Result : Comms_Result) return IR.Channel.Comms_Test_Report
     with Pre => Status (Result) = Success;
   function Rejection_Code (Result : Comms_Result) return Error_Code
     with Pre => Status (Result) = Rejected;
   function Description (Result : Comms_Result) return String
     with Pre => Status (Result) = Rejected;
   function Wait (Request : Comms_Request; Timeout_Milliseconds : Natural)
      return Comms_Result;
   procedure Close (Request : in out Comms_Request);
   function Capabilities (Channel : Control_Channel)
      return IR.Channel.Channel_Capability;
private
   package US renames Ada.Strings.Unbounded;
   type Comms_Owner is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.Comms_Request_Handle := AMS.MEL_C_API.Null_Comms_Request;
   end record;
   overriding procedure Finalize (Request : in out Comms_Owner);
   type Comms_Request is limited record Owner : Comms_Owner; end record;
   type Comms_Result is record
      Result_Status : Outcome := Success;
      Result_Report : IR.Channel.Comms_Test_Report := (0, 0);
      Result_Code : Error_Code := None;
      Result_Text : US.Unbounded_String;
   end record;
end AMS.MEL.IR.C2.Common;
