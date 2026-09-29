private with Ada.Containers.Vectors;
private with Ada.Finalization;
private with Ada.Strings.Unbounded;
private with AMS.MEL_C_API;
with Interfaces;
private with System;

package AMS.MEL.RF.Product_Rx is
   Timeout_Error, Stream_Stopped : exception;
   type Config is private;
   function Create_Config
     (Region_Size_Bytes     : Interfaces.Unsigned_64;
      Queue_Capacity        : Positive;
      Max_Samples_Per_Event : Positive) return Config;
   type Create_Request is tagged limited private;
   function Submit (Parent : Data_MEL; Value : Config) return Create_Request;
   function Is_Open (Request : Create_Request) return Boolean;
   procedure Close (Request : in out Create_Request);
   type Create_Outcome is (Created, Failed);
   type Create_Error_Code is
     (None,
      Invalid_ID,
      Invalid_State,
      Invalid_Parameters,
      Insufficient_Permissions,
      Insufficient_Resources,
      Insufficient_Local_Resources,
      Insufficient_Remote_Resources,
      Unsupported);
   for Create_Error_Code use
     (None                          => 0,
      Invalid_ID                    => 1,
      Invalid_State                 => 2,
      Invalid_Parameters            => 3,
      Insufficient_Permissions      => 4,
      Insufficient_Resources        => 5,
      Insufficient_Local_Resources  => 6,
      Insufficient_Remote_Resources => 7,
      Unsupported                   => 8);
   for Create_Error_Code'Size use 32;
   type Create_Result is private;
   function Outcome (Result : Create_Result) return Create_Outcome;
   function Error_Code (Result : Create_Result) return Create_Error_Code
   with Pre => Outcome (Result) = Failed;
   function Description (Result : Create_Result) return String
   with Pre => Outcome (Result) = Failed;
   function Wait (Request : Create_Request; Timeout_Milliseconds : Natural) return Create_Result;

   type Endpoint is tagged limited private;
   function Claim (Request : Create_Request'Class) return Endpoint;
   function Is_Open (Object : Endpoint) return Boolean;
   function Endpoint_ID (Object : Endpoint) return Interfaces.Unsigned_64;
   function Assigned_Data_Format (Object : Endpoint) return Job_Data_Format;
   procedure Close (Object : in out Endpoint);
   type Counter is mod 2**64 with Size => 64;
   type Counters is record
      Callbacks_Received          : Counter;
      Products_Queued             : Counter;
      Products_Dropped_Queue_Full : Counter;
      Malformed_Or_Unsupported    : Counter;
      Allocation_Failures         : Counter;
      Callbacks_After_Close       : Counter;
   end record;
   function Statistics (Object : Endpoint) return Counters;

   type Complex_I16 is record
      Real : Interfaces.Integer_16;
      Imag : Interfaces.Integer_16;
   end record
   with Convention => C;
   type Complex_I16_Array is array (Natural range <>) of Complex_I16 with Convention => C;

   type Product_Metadata is private;
   function MEL_Protocol_Version_ID (Value : Product_Metadata) return Interfaces.Unsigned_32;
   function VA_Definition_ID (Value : Product_Metadata) return Interfaces.Unsigned_32;
   function VA_Instance_ID (Value : Product_Metadata) return Interfaces.Unsigned_32;
   function Job_Details_ID (Value : Product_Metadata) return Interfaces.Unsigned_32;
   function Job_Interval_ID (Value : Product_Metadata) return Interfaces.Unsigned_32;
   function LF_Type_ID (Value : Product_Metadata) return Interfaces.Unsigned_32;
   function LF_Instance_ID (Value : Product_Metadata) return Interfaces.Unsigned_32;
   function Phase_Coherence_With_Prior (Value : Product_Metadata) return Boolean;
   function First_Rx_Event_Start_Seconds (Value : Product_Metadata) return Interfaces.Integer_64;
   function First_Rx_Event_Start_Femtoseconds
     (Value : Product_Metadata) return Interfaces.Integer_64;
   function Rx_Stream_ID_Count (Value : Product_Metadata) return Natural;
   function Rx_Stream_ID_At
     (Value : Product_Metadata; Index : Positive) return Interfaces.Unsigned_32;

   type Event is limited private;
   function Receive (Object : Endpoint'Class; Timeout_Milliseconds : Natural := 0) return Event;
   function Is_Open (Value : Event) return Boolean;
   function Endpoint_ID (Value : Event) return Interfaces.Unsigned_64;
   function Data_Format (Value : Event) return Job_Data_Format;
   function Metadata (Value : Event) return Product_Metadata;
   function Sample_Count (Value : Event) return Natural;
   --  Borrows the native event payload: no additional bulk copy. Valid only
   --  during Process, while the Event remains open and is not concurrently closed.
   procedure With_Samples
     (Value : Event; Process : not null access procedure (Samples : Complex_I16_Array));
   --  Explicit Ada-owned payload copy, unlike With_Samples.
   function Copy_Samples (Value : Event) return Complex_I16_Array;
   procedure Close (Value : in out Event);
private
   use type Interfaces.Unsigned_32;
   pragma
     Compile_Time_Error
       (Complex_I16'Size /= AMS.MEL_C_API.RF_Complex_I16_V1'Size
          or else Complex_I16'Object_Size /= AMS.MEL_C_API.RF_Complex_I16_V1'Object_Size
          or else Complex_I16'Alignment /= AMS.MEL_C_API.RF_Complex_I16_V1'Alignment,
        "Complex_I16 must match the C ABI");
   package ID_Vectors is new Ada.Containers.Vectors (Positive, Interfaces.Unsigned_32);
   type Config is record
      Region         : Interfaces.Unsigned_64;
      Queue, Maximum : Positive;
   end record;
   type Create_Request is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.RF_Product_Rx_Request_Handle :=
        AMS.MEL_C_API.Null_RF_Product_Rx_Request;
   end record;
   overriding
   procedure Finalize (Request : in out Create_Request);
   type Create_Result is record
      State : Create_Outcome := Created;
      Code  : Create_Error_Code := None;
      Text  : Ada.Strings.Unbounded.Unbounded_String;
   end record;
   type Endpoint is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.RF_Product_Rx_Handle := AMS.MEL_C_API.Null_RF_Product_Rx;
      ID     : Interfaces.Unsigned_64 := 0;
      Format : Job_Data_Format := Complex_INT16;
   end record;
   overriding
   procedure Finalize (Object : in out Endpoint);
   type Product_Metadata is record
      Protocol, Definition, Instance, Details, Interval, LF_Type, LF_Instance :
        Interfaces.Unsigned_32 := 0;
      Coherent                                                                : Boolean := False;
      Seconds, Femtoseconds                                                   :
        Interfaces.Integer_64 := 0;
      Stream_IDs                                                              : ID_Vectors.Vector;
   end record;
   type Event is new Ada.Finalization.Limited_Controlled with record
      Handle         : aliased AMS.MEL_C_API.RF_Product_Rx_Event_Handle :=
        AMS.MEL_C_API.Null_RF_Product_Rx_Event;
      ID             : Interfaces.Unsigned_64 := 0;
      Format         : Job_Data_Format := Complex_INT16;
      Info           : Product_Metadata;
      Sample_Address : System.Address := System.Null_Address;
      Count          : Natural := 0;
   end record;
   overriding
   procedure Finalize (Value : in out Event);
end AMS.MEL.RF.Product_Rx;
