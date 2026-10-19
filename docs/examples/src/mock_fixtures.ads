with AMS.MEL.IR.Image;
with AMS.MEL.RF.C2;

--  Inputs required by the existing native mock, not deployment defaults.

package Mock_Fixtures is
   function Navigation_Fidelity_Report return AMS.MEL.IR.Image.Navigation_Report;
   function Job_Mock_Aperture_Config return AMS.MEL.RF.C2.Virtual_Aperture_Config;
   function Job_Mock_Config return AMS.MEL.RF.C2.Job_Config;
end Mock_Fixtures;
