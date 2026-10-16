private with Ada.Containers.Vectors;
private with Ada.Strings.Unbounded;
with Interfaces;

--  VA-level connections, not E3's optional descriptor pipes. Plain copied
--  values retain no native/provider owner and survive VA/C2/provider unload.
--  Same-VA operations including Close require external serialization.

package AMS.MEL.RF.C2.Data_Pipes is
   --  Conditional VA-level connection snapshots and endpoint associations, separate from
   --  descriptor-level optional pipes. Values own their data and retain no provider lifetime.
   --  Each mutation obtains a fresh VA connection map and invokes the exact provider operation
   --  once, with no automatic capability gate, retry or readback. Externally serialize same-VA
   --  operations and Close; the existing VA owner remains valid after public C2 Close.

   type Connection_Snapshot is private;
   --  Ada-owned VA-level connection copy, distinct from optional ElementGroup descriptor
   --  pipes. Outer group keys ascend unsigned UTF-8 bytes without normalization or locale
   --  collation. Copies survive VA/C2 closure and provider unload.

   type Element_Group_Connection is private;
   --  Copied outer group lookup key and nested pipes, including successful empty groups. No
   --  native/provider ownership remains.

   type Data_Pipe_Connection is private;
   --  Copied inner lookup key, distinct provider-returned label and associated endpoint set.
   --  Inner pipes preserve provider std::map traversal, including aliases.

   type Endpoint_ID_Array is array (Natural range <>) of Interfaces.Unsigned_64;
   --  Caller-owned unsigned 64-bit IDs for a provider set request. All bits survive; input
   --  order is insignificant and duplicates collapse. A null array is a valid empty set.

   function Snapshot (Object : Virtual_Aperture'Class) return Connection_Snapshot;
   --  Make a fresh conditional VA getDataPipes call and copy labels and endpoint sets through
   --  temporary native snapshot ownership. Required upstream only with endpoint association.
   --  Invalid UTF-8, null pipes and native/provider failures raise Provider Error. No
   --  capability auto-gate or atomic multi-getter guarantee.
   --  @param Object Live VA; externally serialize all same-VA operations, including Close. The
   --  existing VA claim survives public C2 closure.
   --  @return Independent VA connections, including successful empty collections.

   function Group_Count (Value : Connection_Snapshot) return Natural;
   --  Count stored outer group map entries.
   --  @param Value Independent Ada-owned copied value; no live provider access or retained
   --  native owner.
   --  @return Copied group count, including zero.

   function Group_At
     (Value : Connection_Snapshot; Index : Positive) return Element_Group_Connection;
   --  Copy a group in ascending unsigned UTF-8 outer-key byte order.
   --  @param Value Independent Ada-owned copied value; no live provider access or retained
   --  native owner.
   --  @param Index Positive one-based position; beyond the stored count raises Constraint
   --  Error.
   --  @return Independent group and its copied nested pipes.

   --  Outer lookup keys ascend unsigned UTF-8 bytes, no normalization/locale.
   function Element_Group_Label (Value : Element_Group_Connection) return String;
   --  Read the original outer group lookup key, not a descriptor display-label query.
   --  @param Value Independent Ada-owned copied value; no live provider access or retained
   --  native owner.
   --  @return Copied UTF-8 group lookup-key bytes, not normalized.

   function Pipe_Count (Value : Element_Group_Connection) return Natural;
   --  Count stored inner pipe map entries, preserving aliases as separate entries.
   --  @param Value Independent Ada-owned copied value; no live provider access or retained
   --  native owner.
   --  @return Copied pipe count, including zero.

   --  Inner pipes preserve provider std::map traversal, including aliases.
   function Pipe_At
     (Value : Element_Group_Connection; Index : Positive) return Data_Pipe_Connection;
   --  Copy a pipe in provider std::map traversal order; no locale-dependent resorting.
   --  @param Value Independent Ada-owned copied value; no live provider access or retained
   --  native owner.
   --  @param Index Positive one-based position; beyond the stored count raises Constraint
   --  Error.
   --  @return Independent pipe with its lookup key, display label and endpoint IDs.

   function Lookup_Label (Value : Data_Pipe_Connection) return String;
   --  Read the original inner pipe lookup key, distinct from returned label and object
   --  identity.
   --  @param Value Independent Ada-owned copied value; no live provider access or retained
   --  native owner.
   --  @return Copied UTF-8 pipe map key.

   --  Returned label is NOT the lookup key or provider object identity.
   function Label (Value : Data_Pipe_Connection) return String;
   --  Read the provider-returned pipe display label, not its map key.
   --  @param Value Independent Ada-owned copied value; no live provider access or retained
   --  native owner.
   --  @return Copied UTF-8 label, which may differ from the lookup key.

   function Endpoint_Count (Value : Data_Pipe_Connection) return Natural;
   --  Count copied provider endpoint set entries; this is not a connectivity test.
   --  @param Value Independent Ada-owned copied value; no live provider access or retained
   --  native owner.
   --  @return Set cardinality with duplicates collapsed, including zero.

   function Endpoint_At
     (Value : Data_Pipe_Connection; Index : Positive) return Interfaces.Unsigned_64;
   --  Read the provider std::set endpoint order, not insertion order.
   --  @param Value Independent Ada-owned copied value; no live provider access or retained
   --  native owner.
   --  @param Index Positive one-based position; beyond the stored count raises Constraint
   --  Error.
   --  @return Unsigned 64-bit endpoint ID preserving all bits.

   --  Each command obtains its own fresh VA::getDataPipes value and invokes
   --  the exact mutation once. True/False is successful provider return data,
   --  not proof of routing, connectivity, RDMA/Q-pairs, hardware or persistence.
   --  No cache/capability gate/retry/readback. Missing keys raise Provider_Error.
   --  Embedded NUL raises Constraint_Error before native entry; C validates UTF-8.
   function Associate_Endpoint
     (Object                               : in out Virtual_Aperture'Class;
      Element_Group_Label, Data_Pipe_Label : String;
      Endpoint_ID                          : Interfaces.Unsigned_64) return Boolean;
   --  Obtain a fresh VA getDataPipes map, resolve existing exact keys, then invoke
   --  associateEndpoint exactly once. Missing keys are errors, never automatic insertions. No
   --  capability gate, retry, cache or state readback. Native/provider failures raise Provider
   --  Error. Provider acceptance does not establish actual connectivity, RDMA/Q-pairs,
   --  writable memory registration, hardware routing, persistent configuration or physical
   --  data delivery.
   --  @param Object Live VA; externally serialize all same-VA operations, including Close. The
   --  existing VA claim survives public C2 closure.
   --  @param Element_Group_Label Exact outer lookup key; embedded NUL raises Constraint Error
   --  before native entry. Native code validates UTF-8.
   --  @param Data_Pipe_Label Exact inner lookup key, not returned display label. Embedded NUL
   --  raises Constraint Error; native code validates UTF-8.
   --  @param Endpoint_ID Exact unsigned 64-bit endpoint ID, without narrowing or local
   --  membership checks.
   --  @return True or False is successful provider return data, not transport status or a
   --  connectivity guarantee.

   --  Upstream std::set semantics: ordering insignificant, duplicates collapse.
   --  An empty array is valid and forwarded as an empty set exactly once.
   function Associate_Endpoints
     (Object                               : in out Virtual_Aperture'Class;
      Element_Group_Label, Data_Pipe_Label : String;
      Endpoint_IDs                         : Endpoint_ID_Array) return Boolean;
   --  Obtain a fresh VA getDataPipes map, resolve existing exact keys, then invoke
   --  associateEndpoints exactly once. Missing keys are errors, never automatic insertions. No
   --  capability gate, retry, cache or state readback. Native/provider failures raise Provider
   --  Error. Provider acceptance does not establish actual connectivity, RDMA/Q-pairs,
   --  writable memory registration, hardware routing, persistent configuration or physical
   --  data delivery.
   --  @param Object Live VA; externally serialize all same-VA operations, including Close. The
   --  existing VA claim survives public C2 closure.
   --  @param Element_Group_Label Exact outer lookup key; embedded NUL raises Constraint Error
   --  before native entry. Native code validates UTF-8.
   --  @param Data_Pipe_Label Exact inner lookup key, not returned display label. Embedded NUL
   --  raises Constraint Error; native code validates UTF-8.
   --  @param Endpoint_IDs Unsigned 64-bit IDs converted to an ordered provider set; duplicates
   --  collapse. Empty is valid and forwarded exactly once.
   --  @return True or False is successful provider return data, not transport status or a
   --  connectivity guarantee.

private
   type Data_Pipe_Connection is record
      Key, Text : Ada.Strings.Unbounded.Unbounded_String;
      Endpoints : Endpoint_Vectors.Vector;
   end record;
   package Pipe_Vectors is new Ada.Containers.Vectors (Positive, Data_Pipe_Connection);
   type Element_Group_Connection is record
      Key   : Ada.Strings.Unbounded.Unbounded_String;
      Pipes : Pipe_Vectors.Vector;
   end record;
   package Group_Vectors is new Ada.Containers.Vectors (Positive, Element_Group_Connection);
   type Connection_Snapshot is record
      Groups : Group_Vectors.Vector;
   end record;
end AMS.MEL.RF.C2.Data_Pipes;
