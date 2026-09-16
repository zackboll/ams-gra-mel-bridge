private with Ada.Finalization;
private with Ada.Strings.Unbounded;
private with AMS.MEL_C_API;

package AMS.MEL is
   --  This is the independent C facade's version, not a provider/MEL version.
   type Version is record
      Major : Natural;
      Minor : Natural;
   end record;

   function ABI_Version return Version;
   --  Raises Program_Error if the native facade violates the version contract.
   --  Does not initialize or load a sensor provider.

   Provider_Error : exception;

   type Provider_Version_Number is mod 2 ** 32 with Size => 32;
   type Provider_Version is private;
   function API_Version (Value : Provider_Version)
     return Provider_Version_Number;
   function Library_Version (Value : Provider_Version)
     return Provider_Version_Number;
   function Vendor (Value : Provider_Version) return String;
   function Description (Value : Provider_Version) return String;

   type Session is limited private;
   function Open
     (Library_Path       : String;
      Instance           : String;
      Aperture_Config_ID : String := "") return Session;
   --  Library_Path, Instance, and Aperture_Config_ID are interpreted as UTF-8
   --  bytes without transcoding.
   --  Raises Constraint_Error if any contains an embedded NUL, which cannot be
   --  represented by the native NUL-terminated interface. An empty
   --  Aperture_Config_ID remains supported. Raises Provider_Error for native or
   --  provider failures.
   function Is_Open (Object : Session) return Boolean;
   function Query_Provider_Version
     (Object : Session) return Provider_Version;
   procedure Close (Object : in out Session);

private
   type Provider_Version is record
      API_Value         : Provider_Version_Number := 0;
      Library_Value     : Provider_Version_Number := 0;
      Vendor_Value      : Ada.Strings.Unbounded.Unbounded_String;
      Description_Value : Ada.Strings.Unbounded.Unbounded_String;
   end record;

   type Session is new Ada.Finalization.Limited_Controlled with record
      Handle : aliased AMS.MEL_C_API.Session_Handle :=
        AMS.MEL_C_API.Null_Session;
   end record;
   overriding procedure Finalize (Object : in out Session);
end AMS.MEL;
