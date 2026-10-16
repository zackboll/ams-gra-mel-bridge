with Interfaces;
with AMS.MEL.RF.C2.Virtual_Aperture_Queries;
private with Ada.Containers.Vectors;

--  Conditional upstream LF queries: empty values are successful provider data.
--  Each snapshot makes one fresh exact provider call, independent of E1 reports.
--  Same-VA operations (including Close) require external serialization.
--  No membership/count reconciliation or atomic consistency across live calls.
--  Returned values own ordinary Ada storage, with no native handle or Close.

package AMS.MEL.RF.C2.Local_Functions is
   --  Conditional LF catalog and status queries, required upstream only when the MFA supports
   --  Local Functions. Each explicit snapshot makes a separate fresh provider call; snapshots
   --  can observe different states and imply no catalog/status membership or count
   --  consistency. Values retain no provider owner. These queries neither execute Local
   --  Functions nor perform register writes.

   type Local_Function_List is private;
   --  Ada-owned catalog copy sorted by provider numeric LF type-ID map keys. Zero-instance
   --  entries remain present; copies survive provider/VA/C2 closure without a native owner.

   type Local_Function_Info is private;
   --  Copied LF type-ID and instance count per VA instance, not evidence of membership
   --  validation or physical LF execution.

   type Status_List is private;
   --  Ada-owned copy of the provider LF status vector, reusing
   --  Virtual Aperture Queries.Status Kind. Provider order, repetitions and empty values
   --  remain intact.

   function Snapshot_Local_Functions (Object : Virtual_Aperture'Class) return Local_Function_List;
   --  Make one fresh conditional getLocalFunctions call. Upstream requires it only with LF
   --  support; no capability auto-gate or implicit status query. Native/provider failures
   --  raise Provider Error; an empty returned catalog succeeds.
   --  @param Object Live claimed VA; externally serialize with all same-VA calls, including
   --  Close.
   --  @return Independent ordered catalog, retaining zero-instance entries.

   function Count (Value : Local_Function_List) return Natural;
   --  Count stored LF type entries, not the sum of their instance counts.
   --  @param Value Ada-owned copied value; no provider access or retained provider owner.
   --  @return Catalog length, including zero.

   --  Ascending provider map keys; zero-count entries remain present.
   function Info_At (Value : Local_Function_List; Index : Positive) return Local_Function_Info;
   --  Copy an LF catalog entry in ascending type-ID map-key order.
   --  @param Value Ada-owned copied value; no provider access or retained provider owner.
   --  @param Index Positive one-based position; beyond the stored count raises
   --  Constraint Error.
   --  @return Independent type-ID/count pair, including zero counts.

   function Type_ID (Value : Local_Function_Info) return Interfaces.Unsigned_32;
   --  Read the provider LF type-ID map key.
   --  @param Value Ada-owned copied value; no provider access or retained provider owner.
   --  @return Exact unsigned 32-bit LF type ID, not a positional index.

   function Instance_Count (Value : Local_Function_Info) return Interfaces.Unsigned_64;
   --  Read the catalog count of instances of this LF type within each VA instance.
   --  @param Value Ada-owned copied value; no provider access or retained provider owner.
   --  @return Unsigned 64-bit copy of the native size t count without narrowing.

   function Snapshot_Local_Function_Status
     (Object                 : Virtual_Aperture'Class;
      VA_Instance_ID         : Interfaces.Unsigned_32;
      Local_Function_Type_ID : Interfaces.Unsigned_32) return Status_List;
   --  Make one fresh conditional getLocalFunctionStatus call, independent of catalog and
   --  instance-report snapshots. No membership/count consistency check is performed. Unknown
   --  status enums or native/provider failures raise Provider Error; empty vectors succeed.
   --  @param Object Live claimed VA; externally serialize with all same-VA calls, including
   --  Close.
   --  @param VA_Instance_ID Provider VA instance ID passed unchanged.
   --  @param Local_Function_Type_ID Provider LF type ID passed unchanged; not validated
   --  against a cached catalog.
   --  @return Independent status vector in provider order; no register writes or LF execution
   --  performed.

   function Count (Value : Status_List) return Natural;
   --  Count copied LF-instance statuses, not catalog type entries.
   --  @param Value Ada-owned copied value; no provider access or retained provider owner.
   --  @return Status vector length, possibly zero.

   --  Order and duplicates unchanged. Ada Index 1 corresponds to upstream LF
   --  instance index 0; it does not invent a one-based provider instance ID.
   function Status_At
     (Value : Status_List; Index : Positive) return Virtual_Aperture_Queries.Status_Kind;
   --  Read provider LF vector order. Ada Index 1 corresponds to provider LF instance index 0,
   --  without inventing one-based provider IDs.
   --  @param Value Ada-owned copied value; no provider access or retained provider owner.
   --  @param Index Positive one-based position; beyond the stored count raises
   --  Constraint Error.
   --  @return Copied provider status, including successful Failed status data.

private
   type Local_Function_Info is record
      ID        : Interfaces.Unsigned_32 := 0;
      Instances : Interfaces.Unsigned_64 := 0;
   end record;
   package Info_Vectors is new Ada.Containers.Vectors (Positive, Local_Function_Info);
   package Status_Vectors is new
     Ada.Containers.Vectors
       (Positive,
        Virtual_Aperture_Queries.Status_Kind,
        Virtual_Aperture_Queries."=");
   type Local_Function_List is record
      Items : Info_Vectors.Vector;
   end record;
   type Status_List is record
      Items : Status_Vectors.Vector;
   end record;
end AMS.MEL.RF.C2.Local_Functions;
