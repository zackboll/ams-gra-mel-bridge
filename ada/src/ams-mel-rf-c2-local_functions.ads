with Interfaces;
with AMS.MEL.RF.C2.Virtual_Aperture_Queries;
private with Ada.Containers.Vectors;

--  Conditional upstream LF queries: empty values are successful provider data.
--  Each snapshot makes one fresh exact provider call, independent of E1 reports.
--  Same-VA operations (including Close) require external serialization.
--  No membership/count reconciliation or atomic consistency across live calls.
--  Returned values own ordinary Ada storage, with no native handle or Close.

package AMS.MEL.RF.C2.Local_Functions is
   type Local_Function_List is private;
   type Local_Function_Info is private;
   type Status_List is private;
   function Snapshot_Local_Functions (Object : Virtual_Aperture'Class) return Local_Function_List;
   function Count (Value : Local_Function_List) return Natural;
   --  Ascending provider map keys; zero-count entries remain present.
   function Info_At (Value : Local_Function_List; Index : Positive) return Local_Function_Info;
   function Type_ID (Value : Local_Function_Info) return Interfaces.Unsigned_32;
   function Instance_Count (Value : Local_Function_Info) return Interfaces.Unsigned_64;
   function Snapshot_Local_Function_Status
     (Object                 : Virtual_Aperture'Class;
      VA_Instance_ID         : Interfaces.Unsigned_32;
      Local_Function_Type_ID : Interfaces.Unsigned_32) return Status_List;
   function Count (Value : Status_List) return Natural;
   --  Order and duplicates unchanged. Ada Index 1 corresponds to upstream LF
   --  instance index 0; it does not invent a one-based provider instance ID.
   function Status_At
     (Value : Status_List; Index : Positive) return Virtual_Aperture_Queries.Status_Kind;
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
