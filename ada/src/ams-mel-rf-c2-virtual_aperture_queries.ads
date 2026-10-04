with Interfaces;
private with Ada.Containers.Vectors;

--  Fresh synchronous queries on the parent's existing claimed owner. Calls
--  must be serialized with Close. Separate calls are not an atomic transaction.

package AMS.MEL.RF.C2.Virtual_Aperture_Queries is
   type Status_Kind is (None, Operational, Degraded, Failed);
   for Status_Kind use (None => 0, Operational => 1, Degraded => 2, Failed => 3);
   function Query_ID (Object : Virtual_Aperture'Class) return Interfaces.Unsigned_32;
   --  Required live capability calls; False is successful provider data.
   function Cached_Waveform_Supported (Object : Virtual_Aperture'Class) return Boolean;
   function Dynamic_Weights_Supported (Object : Virtual_Aperture'Class) return Boolean;
   function Query_Status (Object : Virtual_Aperture'Class) return Status_Kind;
   function Query_Instance_Status
     (Object : Virtual_Aperture'Class; Instance_ID : Interfaces.Unsigned_32) return Status_Kind;

   --  Ordinary Ada-owned values: no native owner, provider reference or Close.
   --  Lists preserve vector order, repeated IDs, and successful empty results.
   type Instance_ID_List is private;
   function Snapshot_All_Instances (Object : Virtual_Aperture'Class) return Instance_ID_List;
   function Snapshot_Instances
     (Object : Virtual_Aperture'Class; Face_ID : Interfaces.Unsigned_32) return Instance_ID_List;
   function Count (Value : Instance_ID_List) return Natural;
   function Instance_ID_At
     (Value : Instance_ID_List; Index : Positive) return Interfaces.Unsigned_32;

   type Local_Function_Status_Group is private;
   function Local_Function_Type_ID
     (Value : Local_Function_Status_Group) return Interfaces.Unsigned_32;
   function Instance_Status_Count (Value : Local_Function_Status_Group) return Natural;
   function Instance_Status_At
     (Value : Local_Function_Status_Group; Index : Positive) return Status_Kind;

   --  Copy of one provider-returned report, including empty LF vectors. Its
   --  returned ID is authoritative and need not match the requested ID. Groups
   --  use ascending type-ID order; vector positions are provider LF ordering.
   type Instance_Status_Report is private;
   function Snapshot_Instance_Status_Report
     (Object : Virtual_Aperture'Class; Instance_ID : Interfaces.Unsigned_32)
      return Instance_Status_Report;
   function Instance_ID (Value : Instance_Status_Report) return Interfaces.Unsigned_32;
   function Status (Value : Instance_Status_Report) return Status_Kind;
   function Local_Function_Type_Count (Value : Instance_Status_Report) return Natural;
   function Local_Function_Group_At
     (Value : Instance_Status_Report; Index : Positive) return Local_Function_Status_Group;
private
   package Status_Vectors is new Ada.Containers.Vectors (Positive, Status_Kind);
   type Instance_ID_List is record
      IDs : ID_Vectors.Vector;
   end record;
   type Local_Function_Status_Group is record
      ID       : Interfaces.Unsigned_32 := 0;
      Statuses : Status_Vectors.Vector;
   end record;
   package Group_Vectors is new Ada.Containers.Vectors (Positive, Local_Function_Status_Group);
   type Instance_Status_Report is record
      ID     : Interfaces.Unsigned_32 := 0;
      State  : Status_Kind := None;
      Groups : Group_Vectors.Vector;
   end record;
end AMS.MEL.RF.C2.Virtual_Aperture_Queries;
