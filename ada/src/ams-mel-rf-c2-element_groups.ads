private with Ada.Containers.Vectors;
private with Ada.Strings.Unbounded;
with Interfaces;

--  Plain copied values: no native handle, Close, or provider-aware finalizer.
--  Same-VA operations, including Close, require external serialization.
--  One snapshot uses multiple provider getters, not an atomic capability read.

package AMS.MEL.RF.C2.Element_Groups is
   type Element_Group_Mode is (Receive, Transmit);
   for Element_Group_Mode use (Receive => 0, Transmit => 1);
   type Element_Group_List is private;
   type Element_Group_Descriptor is private;
   type Data_Pipe_Info is private;
   function Snapshot_Element_Groups
     (Object : Virtual_Aperture'Class; Include_Data_Pipes : Boolean := False)
      return Element_Group_List;
   function Count (Value : Element_Group_List) return Natural;
   function Descriptor_At
     (Value : Element_Group_List; Index : Positive) return Element_Group_Descriptor;
   function Data_Pipes_Included (Value : Element_Group_List) return Boolean;
   function Data_Pipes_Included (Value : Element_Group_Descriptor) return Boolean;
   --  Lookup keys and returned labels are distinct, not provider identities.
   --  Descriptors ascend unsigned UTF-8 lookup-key bytes without normalization.
   function Lookup_Label (Value : Element_Group_Descriptor) return String;
   function Label (Value : Element_Group_Descriptor) return String;
   function Mode (Value : Element_Group_Descriptor) return Element_Group_Mode;
   function Max_RF_Bandwidth_Hz (Value : Element_Group_Descriptor) return Long_Float;
   function Max_Sample_Rate_Samples_Per_Second (Value : Element_Group_Descriptor) return Long_Float;
   function Max_Data_Rate_Bits_Per_Second (Value : Element_Group_Descriptor) return Long_Float;
   --  Dimensionless; upstream states 0 < value <= 1. Values are not repaired.
   --  All numerics forward negative values, signed zero, infinity and NaN.
   --  NaN payload-bit preservation and physical validity are not promised.
   function Max_Duty_Factor (Value : Element_Group_Descriptor) return Long_Float;
   --  Count is stored entries. CHECK Data_Pipes_Included before interpreting
   --  zero as an observed absence. False means not queried, not unsupported.
   function Pipe_Count (Value : Element_Group_Descriptor) return Natural;
   function Pipe_At (Value : Element_Group_Descriptor; Index : Positive) return Data_Pipe_Info;
   function Lookup_Label (Value : Data_Pipe_Info) return String;
   function Label (Value : Data_Pipe_Info) return String;
   function Endpoint_Count (Value : Data_Pipe_Info) return Natural;
   function Endpoint_At (Value : Data_Pipe_Info; Index : Positive) return Interfaces.Unsigned_64;
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
