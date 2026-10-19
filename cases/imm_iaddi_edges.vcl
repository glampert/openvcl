.name ImmCase_iaddi_edges
.syntax new
.vu
.init_vf_all
.init_vi_all
--enter
--endenter
   iaddi iA, vi00, -16
   iaddi iB, vi00, 15
   isw.x iA, 0(vi00)
   isw.x iB, 1(vi00)
--exit
--endexit
