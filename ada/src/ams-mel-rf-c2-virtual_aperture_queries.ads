with Interfaces;
private with Ada.Containers.Vectors;

--  Fresh synchronous queries on the parent's existing claimed owner. Calls
--  must be serialized with Close. Separate calls are not an atomic transaction.

package AMS.MEL.RF.C2.Virtual_Aperture_Queries is
   --  Fresh explicit synchronous VA queries and Ada-owned snapshots. These operations are
   --  required upstream, including the capability Booleans; LF-specific conditional queries
   --  live in Local Functions. Separate calls have no atomicity or consistency guarantee and
   --  do not consume VA notifications. Query success is distinct from the returned status or
   --  False capability data.

   type Status_Kind is (None, Operational, Degraded, Failed);
   --  Provider-reported VA or Local Function status, not transport success and not independent
   --  hardware qualification.
   --  @enum None No status reported.
   --  @enum Operational Provider reports operational state.
   --  @enum Degraded Provider reports degraded state.
   --  @enum Failed Provider reports failed state; the query itself can still succeed.

   for Status_Kind use (None => 0, Operational => 1, Degraded => 2, Failed => 3);
   function Query_ID (Object : Virtual_Aperture'Class) return Interfaces.Unsigned_32;
   --  Invoke required BaseVirtualAperture.getID afresh; no cached Claim value or
   --  request ID substitution. A closed owner or native/provider failure raises
   --  Provider Error; no status enum is read or decoded.
   --  @param Object Live claimed VA; externally serialize with all same-VA calls, including
   --  Close.
   --  @return Provider-returned VA definition ID, not a VA instance index.

   --  Required live capability calls; False is successful provider data.
   function Cached_Waveform_Supported (Object : Virtual_Aperture'Class) return Boolean;
   --  Invoke the required provider isCachedWaveformSupported capability query afresh. False is
   --  successful data, not an unsupported-operation error. Native/provider failures raise
   --  Provider Error.
   --  @param Object Live claimed VA; externally serialize with all same-VA calls, including
   --  Close.
   --  @return Provider Boolean indicating support for cached-waveform modulation; no implicit
   --  gate for other calls.

   function Dynamic_Weights_Supported (Object : Virtual_Aperture'Class) return Boolean;
   --  Invoke the required provider dynamicWeightsSupported capability query afresh. False is
   --  successful data, not an unsupported-operation error. Native/provider failures raise
   --  Provider Error.
   --  @param Object Live claimed VA; externally serialize with all same-VA calls, including
   --  Close.
   --  @return Provider Boolean indicating support for dynamic weighting; no implicit gate for
   --  other calls.

   function Query_Status (Object : Virtual_Aperture'Class) return Status_Kind;
   --  Invoke required getStatus afresh; this is an explicit synchronous query, not a
   --  notification or callback snapshot. Closed owners, native failures and unknown status
   --  enums raise Provider Error; reported Failed or Degraded states are successful data.
   --  @param Object Live claimed VA; externally serialize with all same-VA calls, including
   --  Close.
   --  @return Current provider-reported VA status.

   function Query_Instance_Status
     (Object : Virtual_Aperture'Class; Instance_ID : Interfaces.Unsigned_32) return Status_Kind;
   --  Invoke required getInstanceStatus afresh without inferring state from other queries.
   --  Closed owners, native failures and unknown status enums raise Provider Error; reported
   --  Failed or Degraded states are successful data.
   --  @param Object Live claimed VA; externally serialize with all same-VA calls, including
   --  Close.
   --  @param Instance_ID Provider-defined VA instance ID passed unchanged, not an Ada
   --  position.
   --  @return Provider-reported instance state, not command acceptance.

   --  Ordinary Ada-owned values: no native owner, provider reference or Close.
   --  Lists preserve vector order, repeated IDs, and successful empty results.
   type Instance_ID_List is private;
   --  Ordinary Ada-owned copy of a provider instance vector. Order, duplicates and successful
   --  empty vectors survive. No Close or native owner remains; copies survive VA and C2
   --  closure.

   function Snapshot_All_Instances (Object : Virtual_Aperture'Class) return Instance_ID_List;
   --  Call required getAllInstances once and copy its vector without sorting or deduplication.
   --  Native failures raise Provider Error.
   --  @param Object Live claimed VA; externally serialize with all same-VA calls, including
   --  Close.
   --  @return Independent ID list; empty is successful data, not unsupported functionality.

   function Snapshot_Instances
     (Object : Virtual_Aperture'Class; Face_ID : Interfaces.Unsigned_32) return Instance_ID_List;
   --  Call required getInstances once for the given face and copy the provider vector. No
   --  membership validation or implicit all-instances query. Native failures raise
   --  Provider Error.
   --  @param Object Live claimed VA; externally serialize with all same-VA calls, including
   --  Close.
   --  @param Face_ID Provider face ID forwarded unchanged, not a list position.
   --  @return Independent IDs in provider vector order, including duplicates and empty
   --  results.

   function Count (Value : Instance_ID_List) return Natural;
   --  Count stored instance IDs without a live query.
   --  @param Value Ada-owned copied value; no provider access or retained provider owner.
   --  @return Stored vector length, possibly zero.

   function Instance_ID_At
     (Value : Instance_ID_List; Index : Positive) return Interfaces.Unsigned_32;
   --  Read the stored vector in provider order, preserving repeated IDs.
   --  @param Value Ada-owned copied value; no provider access or retained provider owner.
   --  @param Index Positive one-based position; beyond the stored count raises
   --  Constraint Error.
   --  @return Exact unsigned 32-bit provider instance ID.

   type Local_Function_Status_Group is private;
   --  Ada-owned report group containing one provider LF type-ID map key and its status vector,
   --  including empty vectors. No provider reference remains.

   function Local_Function_Type_ID
     (Value : Local_Function_Status_Group) return Interfaces.Unsigned_32;
   --  Read the copied LF map key, not a vector position.
   --  @param Value Ada-owned copied value; no provider access or retained provider owner.
   --  @return Exact provider LF type ID.

   function Instance_Status_Count (Value : Local_Function_Status_Group) return Natural;
   --  Count copied LF-instance statuses within this report group.
   --  @param Value Ada-owned copied value; no provider access or retained provider owner.
   --  @return Status vector length, including zero.

   function Instance_Status_At
     (Value : Local_Function_Status_Group; Index : Positive) return Status_Kind;
   --  Read LF status in provider vector order. Ada position 1 corresponds to provider vector
   --  index 0, not a new provider identity.
   --  @param Value Ada-owned copied value; no provider access or retained provider owner.
   --  @param Index Positive one-based position; beyond the stored count raises
   --  Constraint Error.
   --  @return Stored status; no live query or execution confirmation.

   --  Copy of one provider-returned report, including empty LF vectors. Its
   --  returned ID is authoritative and need not match the requested ID. Groups
   --  use ascending type-ID order; vector positions are provider LF ordering.
   type Instance_Status_Report is private;
   --  Independent Ada-owned copy of one required provider instance report. Returned instance
   --  identity is authoritative and may differ from the requested ID. LF groups ascend native
   --  map type-ID keys; each vector retains provider order and emptiness. Copies survive VA/C2
   --  closure.

   function Snapshot_Instance_Status_Report
     (Object : Virtual_Aperture'Class; Instance_ID : Interfaces.Unsigned_32)
      return Instance_Status_Report;
   --  Call required getInstanceStatusReport once and copy the returned identity, status and LF
   --  map. No comparison against the requested identity or separate live queries is imposed.
   --  Closed owners, native failures and unknown status enums raise Provider Error; reported
   --  Failed or Degraded states are successful data.
   --  @param Object Live claimed VA; externally serialize with all same-VA calls, including
   --  Close.
   --  @param Instance_ID Requested provider instance ID; need not equal the returned report
   --  ID.
   --  @return Independent report, retaining empty LF groups and provider-returned identity.

   function Instance_ID (Value : Instance_Status_Report) return Interfaces.Unsigned_32;
   --  Read the identity returned inside the report, not the original request argument.
   --  @param Value Ada-owned copied value; no provider access or retained provider owner.
   --  @return Copied unsigned 32-bit report instance ID.

   function Status (Value : Instance_Status_Report) return Status_Kind;
   --  Read the report top-level status without querying or consuming notifications.
   --  @param Value Ada-owned copied value; no provider access or retained provider owner.
   --  @return Copied provider status, not transport status.

   function Local_Function_Type_Count (Value : Instance_Status_Report) return Natural;
   --  Count LF type groups retained in the copied report.
   --  @param Value Ada-owned copied value; no provider access or retained provider owner.
   --  @return Number of map entries, including groups with empty status vectors.

   function Local_Function_Group_At
     (Value : Instance_Status_Report; Index : Positive) return Local_Function_Status_Group;
   --  Copy a report group in ascending numeric LF type-ID map-key order.
   --  @param Value Ada-owned copied value; no provider access or retained provider owner.
   --  @param Index Positive one-based position; beyond the stored count raises
   --  Constraint Error.
   --  @return Independent LF group with its provider-ordered status vector.

private
   package Status_Vectors is new Ada.Containers.Vectors (Positive, Status_Kind);
   type Instance_ID_List is record
      IDs : ID_Vectors.Vector;
   end record;
   type Local_Function_Status_Group is record
      ID       : Interfaces.Unsigned_32 := 0;
      Statuses : Status_Vectors.Vector;
   end record;
   package Group_Vectors is new Ada.Containers.Vectors (Positive, Local_Function_Status_Group);
   type Instance_Status_Report is record
      ID     : Interfaces.Unsigned_32 := 0;
      State  : Status_Kind := None;
      Groups : Group_Vectors.Vector;
   end record;
end AMS.MEL.RF.C2.Virtual_Aperture_Queries;
