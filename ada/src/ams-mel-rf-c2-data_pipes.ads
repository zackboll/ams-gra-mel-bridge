private with Ada.Containers.Vectors;
private with Ada.Strings.Unbounded;
with Interfaces;

--  VA-level connections, not E3's optional descriptor pipes. Plain copied
--  values retain no native/provider owner and survive VA/C2/provider unload.
--  Same-VA operations including Close require external serialization.

package AMS.MEL.RF.C2.Data_Pipes is
   type Connection_Snapshot is private;
   type Element_Group_Connection is private;
   type Data_Pipe_Connection is private;
   type Endpoint_ID_Array is array (Natural range <>) of Interfaces.Unsigned_64;
   function Snapshot (Object : Virtual_Aperture'Class) return Connection_Snapshot;
   function Group_Count (Value : Connection_Snapshot) return Natural;
   function Group_At
     (Value : Connection_Snapshot; Index : Positive) return Element_Group_Connection;
   --  Outer lookup keys ascend unsigned UTF-8 bytes, no normalization/locale.
   function Element_Group_Label (Value : Element_Group_Connection) return String;
   function Pipe_Count (Value : Element_Group_Connection) return Natural;
   --  Inner pipes preserve provider std::map traversal, including aliases.
   function Pipe_At
     (Value : Element_Group_Connection; Index : Positive) return Data_Pipe_Connection;
   function Lookup_Label (Value : Data_Pipe_Connection) return String;
   --  Returned label is NOT the lookup key or provider object identity.
   function Label (Value : Data_Pipe_Connection) return String;
   function Endpoint_Count (Value : Data_Pipe_Connection) return Natural;
   function Endpoint_At
     (Value : Data_Pipe_Connection; Index : Positive) return Interfaces.Unsigned_64;
   --  Each command obtains its own fresh VA::getDataPipes value and invokes
   --  the exact mutation once. True/False is successful provider return data,
   --  not proof of routing, connectivity, RDMA/Q-pairs, hardware or persistence.
   --  No cache/capability gate/retry/readback. Missing keys raise Provider_Error.
   --  Embedded NUL raises Constraint_Error before native entry; C validates UTF-8.
   function Associate_Endpoint
     (Object                               : in out Virtual_Aperture'Class;
      Element_Group_Label, Data_Pipe_Label : String;
      Endpoint_ID                          : Interfaces.Unsigned_64) return Boolean;
   --  Upstream std::set semantics: ordering insignificant, duplicates collapse.
   --  An empty array is valid and forwarded as an empty set exactly once.
   function Associate_Endpoints
     (Object                               : in out Virtual_Aperture'Class;
      Element_Group_Label, Data_Pipe_Label : String;
      Endpoint_IDs                         : Endpoint_ID_Array) return Boolean;
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
