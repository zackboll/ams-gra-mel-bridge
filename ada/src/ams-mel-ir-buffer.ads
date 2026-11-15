private with Ada.Finalization;
private with AMS.MEL_C_API;

package AMS.MEL.IR.Buffer is
   --  Standalone provider Buffer with stable, zero-initialized bridge-owned
   --  host bytes. No registration, acquisition, image view or checkout release.
   --  Serialize operations and closure on each owner. GPU/RDMA flags are only
   --  provider bookkeeping, never physical GPU/RDMA allocation promises.

   type Buffer_Flag is (Host, GPU, RDMA);
   --  Exact published alternatives in provider order.
   --  @enum Host Host bookkeeping value, representation zero.
   --  @enum GPU GPU bookkeeping value, representation one.
   --  @enum RDMA RDMA bookkeeping value, representation two.

   type Flag_Array is array (Positive range <>) of Buffer_Flag;
   --  Ordered copied flags, including duplicates; a null array is valid.

   type Image_Address_Kind is (Null_Image, Inside_Storage, Outside_Storage);
   --  Classification of the starting address only, not a validated image span.
   --  @enum Null_Image Provider returned a null image address.
   --  @enum Inside_Storage Starting address is within owned host bytes.
   --  @enum Outside_Storage Non-null starting address is outside host bytes.

   type Buffer_Properties is record
      Capacity_Bytes         : Natural;
      Initialized_Context    : Long_Long_Integer;
      Provider_Size          : Long_Long_Integer;
      Provider_Context       : Long_Long_Integer;
      Buffer_Address_Matches : Boolean;
      Image_Address          : Image_Address_Kind;
   end record;
   --  Independent snapshot; no native address can be dereferenced through it.
   --  @field Capacity_Bytes Original allocation size in bytes.
   --  @field Initialized_Context Exact signed context passed to init.
   --  @field Provider_Size Actual signed getSize result, without substitution.
   --  @field Provider_Context Actual signed getContext result.
   --  @field Buffer_Address_Matches Whether getBufferAddress equals owned storage.
   --  @field Image_Address Classification of getImageAddress, without layout inference.

   type Host_Buffer is limited private;
   --  Limited owner retaining the provider and Session graph independently of
   --  the public Session. Prefer explicit Close; finalization never raises.

   function Open
     (Parent : Session; Size_Bytes : Positive; User_Context : Long_Long_Integer) return Host_Buffer;
   --  Allocate stable host bytes and initialize one provider Buffer exactly once.
   --  @param Parent Live provider Session supplying its instance and API manager.
   --  @param Size_Bytes Positive allocation capacity in bytes.
   --  @param User_Context Complete signed 64-bit initialization context.
   --  @return Independently owned, unregistered Buffer with zeroed storage.
   --  @exception Provider_Error Closed parent, allocation, factory or init failure;
   --  actual init Return is included in the error when available.

   function Is_Open (Object : Host_Buffer) return Boolean;
   --  Inspect local handle presence, not proof of provider cleanup or health.
   --  @param Object Owner to inspect.
   --  @return True while a native owner is retained.

   function Properties (Object : Host_Buffer) return Buffer_Properties;
   --  Query all four actual provider getters without dereferencing their pointers.
   --  @param Object Open owner.
   --  @return Independent property snapshot, including signed provider values.
   --  @exception Provider_Error Closed owner or provider exception.

   function Size_Bytes (Object : Host_Buffer) return Natural;
   --  Read original allocation capacity, not a potentially different provider size.
   --  @param Object Open owner.
   --  @return Original host allocation size in bytes.
   --  @exception Provider_Error Closed owner or provider exception.

   function User_Context (Object : Host_Buffer) return Long_Long_Integer;
   --  Read the actual provider context, including negative values.
   --  @param Object Open owner.
   --  @return Exact signed getContext result.
   --  @exception Provider_Error Closed owner or provider exception.

   function Flags (Object : Host_Buffer) return Flag_Array;
   --  Copy a fresh ordered provider vector, preserving empty lists and duplicates.
   --  @param Object Open owner; serialize provider-side flag mutation during this call.
   --  @return Independent one-based flag snapshot, possibly empty.
   --  @exception Provider_Error Closed owner, getFlags refusal/exception, unknown
   --  provider flag, allocation failure or vector growth between count and copy.

   procedure Set_Flags (Object : in out Host_Buffer; Values : Flag_Array);
   --  Replace the complete vector; void upstream method has no invented Return.
   --  @param Object Open owner to modify.
   --  @param Values Ordered replacement, including duplicates or an empty vector.
   --  @exception Provider_Error Closed owner, allocation or provider exception.

   procedure Add_Flag (Object : in out Host_Buffer; Flag : Buffer_Flag);
   --  Append one bookkeeping value without changing storage or acquisition.
   --  @param Object Open owner to modify.
   --  @param Flag Exact published flag to append, even if already present.
   --  @exception Provider_Error Closed owner or provider exception.

   function Read_Storage
     (Object : Host_Buffer; Offset : Natural; Count : Natural) return Pixel_Array;
   --  Copy owned host bytes; neither provider pointers nor image layout are used.
   --  @param Object Open owner to read.
   --  @param Offset Zero-based byte offset, at most capacity for an empty read.
   --  @param Count Byte count that must fit the remaining capacity.
   --  @return Independent one-based byte array, possibly empty.
   --  @exception Constraint_Error Span exceeds the original allocation bounds.
   --  @exception Provider_Error Owner is closed or native operation fails.

   procedure Write_Storage (Object : in out Host_Buffer; Offset : Natural; Bytes : Pixel_Array);
   --  Copy bytes into the fixed allocation without resizing or retaining Ada memory.
   --  @param Object Open owner to modify.
   --  @param Offset Zero-based byte offset, at most capacity for an empty write.
   --  @param Bytes Source bytes in array order, with arbitrary array lower bound.
   --  @exception Constraint_Error Span exceeds the original allocation bounds.
   --  @exception Provider_Error Owner is closed or native operation fails.

   procedure Close (Object : in out Host_Buffer);
   --  Idempotent explicit cleanup; never invokes upstream checkout release.
   --  Provider destruction precedes storage and Session graph teardown. If an
   --  extra provider shared owner exists, retain the graph instead of freeing it.
   --  @param Object Owner cleared only after proven ordinary cleanup.
   --  @exception Provider_Error Cleanup is uncertain; native graph stays retained.

private
   type Host_Buffer is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.Buffer_Handle := AMS.MEL_C_API.Null_Buffer;
   end record;
   overriding
   procedure Finalize (Object : in out Host_Buffer);
end AMS.MEL.IR.Buffer;
