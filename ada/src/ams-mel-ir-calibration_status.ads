with AMS.MEL.IR.Channel;
private with Ada.Containers.Vectors;
private with Ada.Strings.Unbounded;

package AMS.MEL.IR.Calibration_Status is
   --  Complete owned values for the pinned optional CalibrationStatusCmd and CalibrationStatus.
   --  No Request_ID correlation, calibration execution, epoch or response deadline is asserted.
   Not_Supported : exception;
   --  Explicit optional callback registration NotSupported; not a request rejection.
   subtype Identifier is AMS.MEL.IR.Channel.Command_ID;
   --  Unsigned 32-bit identifier preserving every bit without inferred correlation.
   type Status_Command is record
      Command_ID : Identifier := 0;
      Request_ID : Identifier := 0;
   end record;
   --  Independent unsigned 32-bit fields, including zero and UINT32_MAX.
   --  @field Command_ID Upstream command identifier, passed unconverted.
   --  @field Request_ID Independent upstream request identifier, passed unconverted.
   type Nanoseconds is range -(2**63) .. (2**63) - 1 with Size => 64;
   --  Exact signed std::chrono::nanoseconds count; no epoch or clock domain is asserted.
   type Calibration_Result is (Not_Set, Completed, Failed, Interrupted, Not_Calibrated);
   --  Exactly pinned values 0 .. 4. MaxExclusive (5) and future values are rejected, never remapped.
   --  @enum Not_Set Upstream NotSet (0), a meaningful unset result.
   --  @enum Completed Upstream Completed (1).
   --  @enum Failed Upstream Failed (2).
   --  @enum Interrupted Upstream Interrupted (3).
   --  @enum Not_Calibrated Upstream NotCalibrated (4).
   for Calibration_Result use
     (Not_Set => 0, Completed => 1, Failed => 2, Interrupted => 3, Not_Calibrated => 4);
   type Completed_Item is private;
   --  One independently owned upstream SubsystemCompletedCalibrationItemType. Copies are deep.
   function Item_Name (Item : Completed_Item) return String;
   --  Copy the complete item name, possibly empty.
   --  @param Item Owned item to read.
   --  @return Complete item name.
   function Result (Item : Completed_Item) return Calibration_Result;
   --  Read the exact item result alternative, never a boundary sentinel.
   --  @param Item Owned item to read.
   --  @return Item result.
   function Fail_Reason (Item : Completed_Item) return String;
   --  Copy the complete item failure description, possibly empty.
   --  @param Item Owned item to read.
   --  @return Complete item failure description.
   type Completed_Calibration is private;
   --  One independently owned upstream SubsystemCompletedCalibration. Copies are deep.
   function Calibration_ID (Calibration : Completed_Calibration) return UCI_ID;
   --  Copy the complete calibration identity: every UUID byte and the full label.
   --  @param Calibration Owned completed calibration to read.
   --  @return Calibration identifier.
   function Time_Tag (Calibration : Completed_Calibration) return Nanoseconds;
   --  Read the exact signed nanoseconds when the calibration ended; no epoch is implied.
   --  @param Calibration Owned completed calibration to read.
   --  @return Unconverted signed nanosecond count.
   function Result (Calibration : Completed_Calibration) return Calibration_Result;
   --  Read the exact overall result alternative, never a boundary sentinel.
   --  @param Calibration Owned completed calibration to read.
   --  @return Overall result.
   function Fail_Reason (Calibration : Completed_Calibration) return String;
   --  Copy the complete overall failure description, possibly empty.
   --  @param Calibration Owned completed calibration to read.
   --  @return Complete overall failure description.
   function Item_Count (Calibration : Completed_Calibration) return Natural;
   --  Number of independently sized ordered items, possibly zero.
   --  @param Calibration Owned completed calibration to read.
   --  @return Item count.
   function Item_At (Calibration : Completed_Calibration; Index : Positive) return Completed_Item;
   --  Copy one item (one-based, upstream order, duplicates kept); invalid indices raise
   --  Constraint_Error.
   --  @param Calibration Owned completed calibration to read.
   --  @param Index One-based item position, not an identifier.
   --  @return Independent owned item.
   type Status_Value is private;
   --  Complete owned upstream CalibrationStatus. The active and completed collections are
   --  independent: no equal lengths, one-to-one correspondence, uniqueness or ordering
   --  relationship is implied. Ordinary assignment creates independent Ada values.
   function Active_Count (Value : Status_Value) return Natural;
   --  Number of ordered active calibration identifiers, possibly zero.
   --  @param Value Owned status to read.
   --  @return Active identifier count.
   function Active_ID (Value : Status_Value; Index : Positive) return UCI_ID;
   --  Copy one active identifier (one-based, upstream order, duplicates kept); invalid indices
   --  raise Constraint_Error.
   --  @param Value Owned status to read.
   --  @param Index One-based position, not an identifier.
   --  @return Complete identifier with every UUID byte and the full label.
   function Completed_Count (Value : Status_Value) return Natural;
   --  Number of ordered completed calibrations, possibly zero and independent of Active_Count.
   --  @param Value Owned status to read.
   --  @return Completed calibration count.
   function Completed_At (Value : Status_Value; Index : Positive) return Completed_Calibration;
   --  Copy one completed calibration (one-based, upstream order); invalid indices raise
   --  Constraint_Error.
   --  @param Value Owned status to read.
   --  @param Index One-based position, not an identifier.
   --  @return Independent owned completed calibration, including its items.
   function Create_Item
     (Item_Name : String; Result : Calibration_Result; Fail_Reason : String) return Completed_Item;
   --  Build an independently owned item from complete values.
   --  @param Item_Name Complete item name, possibly empty.
   --  @param Result Exact item result alternative.
   --  @param Fail_Reason Complete item failure description, possibly empty.
   --  @return Owned item.
   function Create_Calibration
     (Calibration_ID : UCI_ID;
      Time_Tag       : Nanoseconds;
      Result         : Calibration_Result;
      Fail_Reason    : String) return Completed_Calibration;
   --  Build an independently owned completed calibration with no items.
   --  @param Calibration_ID Complete calibration identity.
   --  @param Time_Tag Exact signed nanosecond count; no epoch is implied.
   --  @param Result Exact overall result alternative.
   --  @param Fail_Reason Complete overall failure description, possibly empty.
   --  @return Owned completed calibration.
   procedure Append_Item (Calibration : in out Completed_Calibration; Item : Completed_Item);
   --  Append a complete owned item copy, preserving order and duplicates.
   --  @param Calibration Destination completed calibration.
   --  @param Item Item copied to the end.
   procedure Append_Active (Value : in out Status_Value; ID : UCI_ID);
   --  Append an active identifier copy, preserving order and duplicates.
   --  @param Value Destination status.
   --  @param ID Identifier copied to the end of the active collection only.
   procedure Append_Completed (Value : in out Status_Value; Calibration : Completed_Calibration);
   --  Append a completed calibration copy, preserving order; the active collection is unchanged.
   --  @param Value Destination status.
   --  @param Calibration Completed calibration copied to the end of the completed collection only.
   subtype Outcome is AMS.MEL.IR.Channel.Outcome;
   --  Success, including two empty collections, or explicit MEL rejection.
   use type Outcome;
   subtype Error_Code is AMS.MEL.IR.Channel.Error_Code;
   --  Canonical known MEL error categories; unknown provider codes are errors.
   type Status_Result is private;
   --  Success (possibly empty) or explicit MEL rejection. Other failures raise Provider_Error.
   function Status (Result : Status_Result) return Outcome;
   --  Inspect terminal category without provider access.
   --  @param Result Owned terminal result.
   --  @return Success or Rejected.
   function Value (Result : Status_Result) return Status_Value
   with Pre => Status (Result) = AMS.MEL.IR.Channel.Success;
   --  Return a deep Ada copy, independent of native request, C2 and Session lifetimes.
   --  @param Result Successful result, including an empty status.
   --  @return Complete owned calibration status.
   function Rejection_Code (Result : Status_Result) return Error_Code
   with Pre => Status (Result) = AMS.MEL.IR.Channel.Rejected;
   --  Read an explicit MEL rejection category, not a callback registration result.
   --  @param Result Rejected terminal result.
   --  @return Known mapped MEL error category.
   function Description (Result : Status_Result) return String
   with Pre => Status (Result) = AMS.MEL.IR.Channel.Rejected;
   --  Copy the complete cached provider rejection text, including long descriptions.
   --  @param Result Rejected terminal result.
   --  @return Independent description string.
   type Status_Request is new AMS.MEL.IR.Calibration_Status_Request_Owner with private;
   --  Limited controlled observer. Pending work retains C2/Session/provider independently.
   function Is_Open (Request : Status_Request) return Boolean;
   --  Test local handle presence, not completion or provider availability.
   --  @param Request Request owner to inspect.
   --  @return True while a public observer is present.
   function Wait (Request : Status_Request; Timeout_Milliseconds : Natural) return Status_Result;
   --  Finite wait; Timeout_Error is not cancellation. Repeated Wait copies the cached result.
   --  Provider/future exceptions, invalid data, null success and cleanup failures raise Provider_Error.
   --  @param Request Open request owner; serialize against Close.
   --  @param Timeout_Milliseconds Maximum wait in milliseconds; zero polls.
   --  @return Owned success status or explicit MEL rejection.
   --  @exception Timeout_Error The request remains pending.
   --  @exception Provider_Error Closed request, native failure, malformed provider result or deferred cleanup failure.
   procedure Close (Request : in out Status_Request);
   --  Idempotent observer closure, not cancellation. Serialize against Wait.
   --  @param Request Observer to release; pending work continues independently.
   --  @exception Provider_Error Native observer closure failed.
private
   package US renames Ada.Strings.Unbounded;
   type Completed_Item is record
      Name        : US.Unbounded_String;
      Item_Result : Calibration_Result := Not_Set;
      Reason      : US.Unbounded_String;
   end record;
   package Item_Vectors is new Ada.Containers.Vectors (Positive, Completed_Item);
   type Completed_Calibration is record
      ID      : UCI_ID;
      Time    : Nanoseconds := 0;
      Overall : Calibration_Result := Not_Set;
      Reason  : US.Unbounded_String;
      Items   : Item_Vectors.Vector;
   end record;
   package ID_Vectors is new Ada.Containers.Vectors (Positive, UCI_ID);
   package Completed_Vectors is new Ada.Containers.Vectors (Positive, Completed_Calibration);
   type Status_Value is record
      Active      : ID_Vectors.Vector;
      Completions : Completed_Vectors.Vector;
   end record;
   type Status_Request is new AMS.MEL.IR.Calibration_Status_Request_Owner with record
      null;
   end record;
   type Status_Result is record
      Result_Status : Outcome := AMS.MEL.IR.Channel.Success;
      Result_Value  : Status_Value;
      Result_Code   : Error_Code := AMS.MEL.IR.Channel.None;
      Result_Text   : US.Unbounded_String;
   end record;
end AMS.MEL.IR.Calibration_Status;
