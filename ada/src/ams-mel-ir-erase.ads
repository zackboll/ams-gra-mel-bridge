with AMS.MEL.IR.Channel;
private with Ada.Strings.Unbounded;

package AMS.MEL.IR.Erase is
   --  Optional C2 EraseCommand values. A successful asynchronous return does not
   --  verify physical erasure: the pinned interface defines no such verification.
   subtype Command_ID is AMS.MEL.IR.Channel.Command_ID;
   --  Exact unsigned 32-bit command identifier.
   type Erase_Type is (Zeroize, Classified_Data_Erase, Disable, CDE_AND_Zeroize);
   --  Exactly the pinned EraseCommandType; no sentinel and no inferred operation.
   --  @enum Zeroize Upstream Zeroize, 0.
   --  @enum Classified_Data_Erase Upstream Classified_data_erase, 1.
   --  @enum Disable Upstream Disable, 2.
   --  @enum CDE_AND_Zeroize Upstream CDE_AND_Zeroize, 3.
   for Erase_Type use
     (Zeroize => 0, Classified_Data_Erase => 1, Disable => 2, CDE_AND_Zeroize => 3);
   type Erase_Command is private;
   --  Owned complete command; only Create_Command constructs a submission value.
   function Create_Command (ID : Command_ID; Operation : Erase_Type) return Erase_Command;
   --  Require an explicit operation; never default a submission to Zeroize.
   --  @param ID Exact caller command identifier.
   --  @param Operation Explicit selected erase operation.
   --  @return Owned complete command.
   function ID (Command : Erase_Command) return Command_ID;
   --  @param Command Owned command.
   --  @return Exact identifier.
   function Operation (Command : Erase_Command) return Erase_Type;
   --  @param Command Owned command.
   --  @return Explicit selected operation.

   subtype Outcome is AMS.MEL.IR.Channel.Outcome;
   use type Outcome;
   subtype Error_Code is AMS.MEL.IR.Channel.Error_Code;
   type Erase_Result is private;
   --  Owned Success or explicit MEL Rejected result. Provider/payload/cleanup
   --  failures raise Provider_Error, not a MEL rejection or successful result.
   function Status (Result : Erase_Result) return Outcome;
   --  @param Result Owned terminal result.
   --  @return Success or Rejected.
   function Response (Result : Erase_Result) return Erase_Type
   with Pre => Status (Result) = AMS.MEL.IR.Channel.Success;
   --  Independent of the submitted type; no physical completion guarantee.
   --  @param Result Successful result.
   --  @return Exact returned EraseCommandType.
   function Rejection_Code (Result : Erase_Result) return Error_Code
   with Pre => Status (Result) = AMS.MEL.IR.Channel.Rejected;
   --  @param Result Rejected result.
   --  @return Known MEL error code.
   function Description (Result : Erase_Result) return String
   with Pre => Status (Result) = AMS.MEL.IR.Channel.Rejected;
   --  @param Result Rejected result.
   --  @return Complete owned diagnostic, including long descriptions.
   type Erase_Request is new AMS.MEL.IR.Erase_Request_Owner with private;
   --  Limited controlled observer; finalization never raises. Provider execution
   --  retains the native C2/Session/provider graph independently of this observer.
   function Is_Open (Request : Erase_Request) return Boolean;
   --  @param Request Observer to inspect.
   --  @return Local handle presence, not completion state.
   function Wait (Request : Erase_Request; Timeout_Milliseconds : Natural) return Erase_Result;
   --  Cached terminal wait; timeout never cancels or consumes; no automatic retry.
   --  @param Request Open observer; serialize against Close.
   --  @param Timeout_Milliseconds Finite wait; zero polls.
   --  @return Owned success or explicit MEL rejection.
   --  @exception Timeout_Error Still pending.
   --  @exception Provider_Error Closed owner, exception, null/invalid successful
   --  payload, unknown enum/error code, native failure or deferred cleanup failure.
   procedure Close (Request : in out Erase_Request);
   --  Idempotently close the observer only; never cancel provider execution.
   --  @param Request Observer; serialize against Wait.
   --  @exception Provider_Error Native close failed.
private
   package US renames Ada.Strings.Unbounded;
   type Erase_Command is record
      Value_ID        : Command_ID := 0;
      Value_Operation : Erase_Type := Disable;
      Selected        : Boolean := False;
   end record;
   type Erase_Request is new AMS.MEL.IR.Erase_Request_Owner with record
      null;
   end record;
   type Erase_Result is record
      Result_Status   : Outcome := AMS.MEL.IR.Channel.Success;
      Result_Response : Erase_Type := Disable;
      Result_Code     : Error_Code := AMS.MEL.IR.Channel.None;
      Result_Text     : US.Unbounded_String;
   end record;
end AMS.MEL.IR.Erase;
