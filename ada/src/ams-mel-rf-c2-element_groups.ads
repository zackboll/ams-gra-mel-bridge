private with Ada.Containers.Vectors;
private with Ada.Strings.Unbounded;
with Interfaces;

--  Plain copied values: no native handle, Close, or provider-aware finalizer.
--  Same-VA operations, including Close, require external serialization.
--  One snapshot uses multiple provider getters, not an atomic capability read.

package AMS.MEL.RF.C2.Element_Groups is
   --  Snapshots of required ElementGroup descriptors with optional conditional nested DataPipe
   --  acquisition. One explicit snapshot uses multiple provider getters, not an atomic
   --  transaction. Lookup keys and display labels remain distinct. Copied getters never access
   --  the provider; live acquisition requires external serialization with all same-VA
   --  operations, including Close.

   type Element_Group_Mode is (Receive, Transmit);
   --  Copied provider group direction; unknown native mode values reject snapshot acquisition
   --  with Provider Error.
   --  @enum Receive Provider RX group mode.
   --  @enum Transmit Provider TX group mode, not evidence of actual transmission.

   for Element_Group_Mode use (Receive => 0, Transmit => 1);
   type Element_Group_List is private;
   --  Ada-owned descriptor collection sorted by unsigned UTF-8 lookup-key bytes. No text
   --  normalization or locale collation; no native handle, Close or provider-aware finalizer.
   --  Copies survive VA/C2/provider closure.

   type Element_Group_Descriptor is private;
   --  Copied lookup key, provider display label, direction and numeric limits, with explicit
   --  nested-pipe inclusion state. No provider ownership remains.

   type Data_Pipe_Info is private;
   --  Copied optional descriptor-level pipe lookup key, returned label and endpoint set.
   --  Distinct from the VA-level connections exposed by Data Pipes.

   function Snapshot_Element_Groups
     (Object : Virtual_Aperture'Class; Include_Data_Pipes : Boolean := False)
      return Element_Group_List;
   --  Obtain required getElementGroups and multiple descriptor getters, optionally acquiring
   --  conditional descriptor DataPipes and their labels/endpoints. This is not an atomic
   --  multi-getter transaction. Native snapshot storage is copied to Ada then released.
   --  Invalid UTF-8, null provider objects, unknown modes and native/provider failures raise
   --  Provider Error; no partial snapshot is returned.
   --  @param Object Live VA; externally serialize all same-VA operations, including Close. The
   --  existing VA claim survives public C2 closure.
   --  @param Include_Data_Pipes If True, query nested descriptor pipes. False means not
   --  queried, not absent or unsupported; an included empty map is different.
   --  @return Independent sorted descriptors; successful empty collections are retained.

   function Count (Value : Element_Group_List) return Natural;
   --  Count copied descriptors without querying the provider.
   --  @param Value Independent Ada-owned copied value; no live provider access or retained
   --  native owner.
   --  @return Stored descriptor count, possibly zero.

   function Descriptor_At
     (Value : Element_Group_List; Index : Positive) return Element_Group_Descriptor;
   --  Copy a descriptor in deterministic unsigned UTF-8 lookup-key byte order.
   --  @param Value Independent Ada-owned copied value; no live provider access or retained
   --  native owner.
   --  @param Index Positive one-based position; beyond the stored count raises Constraint
   --  Error.
   --  @return Independent descriptor, not a live provider object.

   function Data_Pipes_Included (Value : Element_Group_List) return Boolean;
   --  Inspect stored nested-pipe acquisition state, not provider capability. False means no
   --  nested query was made; True with zero pipes means a queried empty result.
   --  @param Value Independent Ada-owned copied value; no live provider access or retained
   --  native owner.
   --  @return Whether descriptor-level DataPipes were included at acquisition.

   function Data_Pipes_Included (Value : Element_Group_Descriptor) return Boolean;
   --  Inspect stored nested-pipe acquisition state, not provider capability. False means no
   --  nested query was made; True with zero pipes means a queried empty result.
   --  @param Value Independent Ada-owned copied value; no live provider access or retained
   --  native owner.
   --  @return Whether descriptor-level DataPipes were included at acquisition.

   --  Lookup keys and returned labels are distinct, not provider identities.
   --  Descriptors ascend unsigned UTF-8 lookup-key bytes without normalization.
   function Lookup_Label (Value : Element_Group_Descriptor) return String;
   --  Read the original provider map lookup key, distinct from the returned display label and
   --  not a provider-object identity.
   --  @param Value Independent Ada-owned copied value; no live provider access or retained
   --  native owner.
   --  @return Copied UTF-8 lookup-key bytes without normalization.

   function Label (Value : Element_Group_Descriptor) return String;
   --  Read the provider-returned display label, which may differ from the map lookup key. No
   --  identity equivalence is inferred.
   --  @param Value Independent Ada-owned copied value; no live provider access or retained
   --  native owner.
   --  @return Copied UTF-8 provider label without normalization.

   function Mode (Value : Element_Group_Descriptor) return Element_Group_Mode;
   --  Read the copied provider RX/TX direction without live access.
   --  @param Value Independent Ada-owned copied value; no live provider access or retained
   --  native owner.
   --  @return Receive or Transmit group mode; not physical activity confirmation.

   function Max_RF_Bandwidth_Hz (Value : Element_Group_Descriptor) return Long_Float;
   --  Read copied provider maximum RF bandwidth. No clipping, range enforcement or physical-
   --  validity inference; signed zero, negative values, infinity and NaN classification
   --  survive numeric conversion, without a NaN payload-bit guarantee.
   --  @param Value Independent Ada-owned copied value; no live provider access or retained
   --  native owner.
   --  @return Provider maximum RF bandwidth in Hz.

   function Max_Sample_Rate_Samples_Per_Second (Value : Element_Group_Descriptor) return Long_Float;
   --  Read copied provider maximum sample rate. No clipping, range enforcement or physical-
   --  validity inference; signed zero, negative values, infinity and NaN classification
   --  survive numeric conversion, without a NaN payload-bit guarantee.
   --  @param Value Independent Ada-owned copied value; no live provider access or retained
   --  native owner.
   --  @return Provider maximum sample rate in samples per second.

   function Max_Data_Rate_Bits_Per_Second (Value : Element_Group_Descriptor) return Long_Float;
   --  Read copied provider maximum data rate. No clipping, range enforcement or physical-
   --  validity inference; signed zero, negative values, infinity and NaN classification
   --  survive numeric conversion, without a NaN payload-bit guarantee.
   --  @param Value Independent Ada-owned copied value; no live provider access or retained
   --  native owner.
   --  @return Provider maximum data rate in bits per second.

   --  Dimensionless; upstream states 0 < value <= 1. Values are not repaired.
   --  All numerics forward negative values, signed zero, infinity and NaN.
   --  NaN payload-bit preservation and physical validity are not promised.
   function Max_Duty_Factor (Value : Element_Group_Descriptor) return Long_Float;
   --  Read copied provider maximum duty factor. No clipping, range enforcement or physical-
   --  validity inference; signed zero, negative values, infinity and NaN classification
   --  survive numeric conversion, without a NaN payload-bit guarantee. Upstream describes 0 <
   --  value <= 1, with 1 denoting CW; the bridge does not enforce or repair that interval.
   --  @param Value Independent Ada-owned copied value; no live provider access or retained
   --  native owner.
   --  @return Provider maximum duty factor in dimensionless ratio.

   --  Count is stored entries. CHECK Data_Pipes_Included before interpreting
   --  zero as an observed absence. False means not queried, not unsupported.
   function Pipe_Count (Value : Element_Group_Descriptor) return Natural;
   --  Count stored descriptor pipes. Check Data Pipes Included first: zero with False means
   --  not queried, unlike a successful included empty result.
   --  @param Value Independent Ada-owned copied value; no live provider access or retained
   --  native owner.
   --  @return Stored nested pipe count, possibly zero.

   function Pipe_At (Value : Element_Group_Descriptor; Index : Positive) return Data_Pipe_Info;
   --  Copy a nested descriptor pipe in provider std::map traversal order.
   --  @param Value Independent Ada-owned copied value; no live provider access or retained
   --  native owner.
   --  @param Index Positive one-based position; beyond the stored count raises Constraint
   --  Error.
   --  @return Independent nested pipe information, with lookup key distinct from display
   --  label.

   function Lookup_Label (Value : Data_Pipe_Info) return String;
   --  Read the original provider map lookup key, distinct from the returned display label and
   --  not a provider-object identity.
   --  @param Value Independent Ada-owned copied value; no live provider access or retained
   --  native owner.
   --  @return Copied UTF-8 lookup-key bytes without normalization.

   function Label (Value : Data_Pipe_Info) return String;
   --  Read the provider-returned display label, which may differ from the map lookup key. No
   --  identity equivalence is inferred.
   --  @param Value Independent Ada-owned copied value; no live provider access or retained
   --  native owner.
   --  @return Copied UTF-8 provider label without normalization.

   function Endpoint_Count (Value : Data_Pipe_Info) return Natural;
   --  Count copied associated endpoint set entries, without querying connectivity.
   --  @param Value Independent Ada-owned copied value; no live provider access or retained
   --  native owner.
   --  @return Stored endpoint count, including zero.

   function Endpoint_At (Value : Data_Pipe_Info; Index : Positive) return Interfaces.Unsigned_64;
   --  Read an associated endpoint in provider std::set order; duplicate values are collapsed
   --  by set semantics.
   --  @param Value Independent Ada-owned copied value; no live provider access or retained
   --  native owner.
   --  @param Index Positive one-based position; beyond the stored count raises Constraint
   --  Error.
   --  @return Unsigned 64-bit endpoint ID with all bits preserved, not a positional ID.

private
   type Data_Pipe_Info is record
      Key, Text : Ada.Strings.Unbounded.Unbounded_String;
      Endpoints : Endpoint_Vectors.Vector;
   end record;
   package Pipe_Vectors is new Ada.Containers.Vectors (Positive, Data_Pipe_Info);
   --  Primitive numeric storage avoids optional GNAT float-validity checks
   --  during container copies; only checked numeric copy/access converts it.
   type Numeric_Storage is array (Positive range 1 .. 4) of Interfaces.Unsigned_64;
   type Element_Group_Descriptor is record
      Key, Text : Ada.Strings.Unbounded.Unbounded_String;
      State     : Element_Group_Mode := Receive;
      Numbers   : Numeric_Storage := [others => 0];
      Included  : Boolean := False;
      Pipes     : Pipe_Vectors.Vector;
   end record;
   package Descriptor_Vectors is new Ada.Containers.Vectors (Positive, Element_Group_Descriptor);
   type Element_Group_List is record
      Included    : Boolean := False;
      Descriptors : Descriptor_Vectors.Vector;
   end record;
end AMS.MEL.RF.C2.Element_Groups;
