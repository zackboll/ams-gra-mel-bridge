private with Ada.Finalization;
private with Ada.Strings.Unbounded;
private with AMS.MEL_C_API;

package AMS.MEL.IR.C2 is
   type Control_Config is private;
   function Create_Config
     (Channel_ID      : UCI_ID;
      Platform_ID     : UCI_ID;
      Sensor_Location : Component_Location) return Control_Config;

   type Control_Channel is limited private;
   function Open (Parent : Session; Config : Control_Config)
     return Control_Channel;
   function Is_Open (Channel : Control_Channel) return Boolean;
   --  Enable, Submit_Operate, Submit_BIT_No_Op, and Close calls for one
   --  Control_Channel must be externally serialized. Parent Session close
   --  rules are unchanged.
   procedure Enable (Channel : in out Control_Channel);

   type Command_ID is mod 2 ** 32 with Size => 32;
   type Mode_Request is limited private;
   function Submit_Operate
     (Channel : Control_Channel; ID : Command_ID := 0) return Mode_Request;
   function Is_Open (Request : Mode_Request) return Boolean;

   type Return_Request is limited private;
   function Submit_BIT_No_Op
     (Channel : Control_Channel; ID : Command_ID := 0) return Return_Request;
   function Is_Open (Request : Return_Request) return Boolean;

   type Outcome is (Success, Rejected);
   type MFA_Mode is (Unused, Task_Sched, Scan_Volume_Sched, Scan_Bar_Sched);
   type Error_Code is
     (None, Invalid_ID, Invalid_State, Invalid_Parameters,
      Insufficient_Permissions, Insufficient_Resources,
      Insufficient_Local_Resources, Insufficient_Remote_Resources,
      Unsupported);
   type Mode_Result is private;
   function Status (Result : Mode_Result) return Outcome;
   function Mode (Result : Mode_Result) return MFA_Mode
     with Pre => Status (Result) = Success;
   function Rejection_Code (Result : Mode_Result) return Error_Code
     with Pre => Status (Result) = Rejected;
   function Description (Result : Mode_Result) return String
     with Pre => Status (Result) = Rejected;
   type Command_Return is
     (Return_Success, Bad_Pointer, Fail, Not_Supported, Not_Implemented);
   --  Success means RequestFor<Return> completed with a Command_Return value;
   --  that value may be Fail or any other enumerator above. Rejected represents
   --  upstream ErrorOr(Error), so Success with Value = Fail is not a facade or
   --  provider failure.
   type Return_Result is private;
   function Status (Result : Return_Result) return Outcome;
   function Value (Result : Return_Result) return Command_Return
     with Pre => Status (Result) = Success;
   function Rejection_Code (Result : Return_Result) return Error_Code
     with Pre => Status (Result) = Rejected;
   function Description (Result : Return_Result) return String
     with Pre => Status (Result) = Rejected;

   --  Timeout_Error is inherited from AMS.MEL.IR. Timeout never cancels or
   --  consumes either request type; Wait may be repeated and a later Wait may
   --  return its cached terminal result. Close must not race Wait on the same
   --  Mode_Request or on the same Return_Request.
   function Wait
     (Request : Mode_Request; Timeout_Milliseconds : Natural) return Mode_Result;
   function Wait
     (Request : Return_Request; Timeout_Milliseconds : Natural) return Return_Result;

   procedure Close (Request : in out Mode_Request);
   procedure Close (Request : in out Return_Request);
   procedure Close (Channel : in out Control_Channel);

private
   package US renames Ada.Strings.Unbounded;
   type Control_Config is record
      Channel  : UCI_ID;
      Platform : UCI_ID;
      Location : Component_Location;
   end record;
   type Control_Channel is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.C2_Handle := AMS.MEL_C_API.Null_C2;
   end record;
   overriding procedure Finalize (Channel : in out Control_Channel);
   type Request_Owner is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.Mode_Request_Handle :=
        AMS.MEL_C_API.Null_Mode_Request;
   end record;
   overriding procedure Finalize (Request : in out Request_Owner);
   type Mode_Request is limited record
      Owner : Request_Owner;
   end record;
   type Return_Request_Owner is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.Return_Request_Handle :=
        AMS.MEL_C_API.Null_Return_Request;
   end record;
   overriding procedure Finalize (Request : in out Return_Request_Owner);
   type Return_Request is limited record
      Owner : Return_Request_Owner;
   end record;
   type Mode_Result is record
      Result_Status : Outcome := Success;
      Result_Mode   : MFA_Mode := Unused;
      Result_Code   : Error_Code := None;
      Result_Text   : US.Unbounded_String;
   end record;
   type Return_Result is record
      Result_Status : Outcome := Success;
      Result_Value  : Command_Return := Return_Success;
      Result_Code   : Error_Code := None;
      Result_Text   : US.Unbounded_String;
   end record;
end AMS.MEL.IR.C2;
