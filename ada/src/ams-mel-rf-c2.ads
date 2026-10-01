private with Ada.Finalization;
private with Ada.Containers.Vectors;
private with Ada.Strings.Unbounded;
private with AMS.MEL_C_API;
with AMS.MEL.IR;
with Interfaces;

package AMS.MEL.RF.C2 is
   type C2_MEL is tagged limited private;
   function Open (Library_Path : String; Configuration : String) return C2_MEL;
   function Is_Open (Object : C2_MEL) return Boolean;
   procedure Close (Object : in out C2_MEL);
   Timeout_Error : exception;
   type Virtual_Aperture_Config is private;
   function Create_Virtual_Aperture_Config
     (VA_Definition_ID        : Interfaces.Unsigned_32;
      Priority                : Interfaces.Unsigned_32;
      VA_Definition_File_Info : String) return Virtual_Aperture_Config;
   procedure Append_Local_Function_Info (Config : in out Virtual_Aperture_Config; Value : String);
   procedure Append_Capability_ID
     (Config : in out Virtual_Aperture_Config; Value : AMS.MEL.IR.UCI_ID);
   type Virtual_Aperture_Request is tagged limited private;
   function Submit_Virtual_Aperture
     (Parent : C2_MEL'Class; Config : Virtual_Aperture_Config) return Virtual_Aperture_Request;
   function Is_Open (Request : Virtual_Aperture_Request) return Boolean;
   procedure Close (Request : in out Virtual_Aperture_Request);
   type Request_Outcome is (Created, Failed);
   type Request_Error_Code is
     (None,
      Invalid_ID,
      Invalid_State,
      Invalid_Parameters,
      Insufficient_Permissions,
      Insufficient_Resources,
      Insufficient_Local_Resources,
      Insufficient_Remote_Resources,
      Unsupported);
   for Request_Error_Code use
     (None                          => 0,
      Invalid_ID                    => 1,
      Invalid_State                 => 2,
      Invalid_Parameters            => 3,
      Insufficient_Permissions      => 4,
      Insufficient_Resources        => 5,
      Insufficient_Local_Resources  => 6,
      Insufficient_Remote_Resources => 7,
      Unsupported                   => 8);
   type Virtual_Aperture_Result is private;
   function Outcome (Result : Virtual_Aperture_Result) return Request_Outcome;
   function Error_Code (Result : Virtual_Aperture_Result) return Request_Error_Code;
   function Description (Result : Virtual_Aperture_Result) return String;
   function Wait
     (Request : Virtual_Aperture_Request; Timeout_Milliseconds : Natural)
      return Virtual_Aperture_Result;
   type Virtual_Aperture is limited private;
   function Claim (Request : Virtual_Aperture_Request'Class) return Virtual_Aperture;
   function Is_Open (Object : Virtual_Aperture) return Boolean;
   function VA_Instance_ID_Count (Object : Virtual_Aperture) return Natural;
   function VA_Instance_ID_At
     (Object : Virtual_Aperture; Index : Positive) return Interfaces.Unsigned_32;
   function Element_Group_Label_Count (Object : Virtual_Aperture) return Natural;
   function Element_Group_Label_At (Object : Virtual_Aperture; Index : Positive) return String;
   function Is_Single_Group (Object : Virtual_Aperture) return Boolean;
   procedure Close (Object : in out Virtual_Aperture);
private
   package Text_Vectors is new
     Ada.Containers.Vectors
       (Positive,
        Ada.Strings.Unbounded.Unbounded_String,
        Ada.Strings.Unbounded."=");
   package UCI_Vectors is new Ada.Containers.Vectors (Positive, AMS.MEL.IR.UCI_ID, AMS.MEL.IR."=");
   package ID_Vectors is new
     Ada.Containers.Vectors (Positive, Interfaces.Unsigned_32, Interfaces."=");
   type Virtual_Aperture_Config is record
      ID, Priority : Interfaces.Unsigned_32;
      File_Info    : Ada.Strings.Unbounded.Unbounded_String;
      Local        : Text_Vectors.Vector;
      Capabilities : UCI_Vectors.Vector;
   end record;
   type Virtual_Aperture_Result is record
      State : Request_Outcome := Created;
      Code  : Request_Error_Code := None;
      Text  : Ada.Strings.Unbounded.Unbounded_String;
   end record;
   type Virtual_Aperture_Request is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.RF_VA_Request_Handle := AMS.MEL_C_API.Null_RF_VA_Request;
   end record;
   overriding
   procedure Finalize (Request : in out Virtual_Aperture_Request);
   type Virtual_Aperture is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.RF_VA_Handle := AMS.MEL_C_API.Null_RF_VA;
      IDs    : ID_Vectors.Vector;
      Labels : Text_Vectors.Vector;
      Single : Boolean := False;
   end record;
   overriding
   procedure Finalize (Object : in out Virtual_Aperture);
   type C2_MEL is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.RF_C2_Handle := AMS.MEL_C_API.Null_RF_C2;
   end record;
   overriding
   procedure Finalize (Object : in out C2_MEL);
end AMS.MEL.RF.C2;
