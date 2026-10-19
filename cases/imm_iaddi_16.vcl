.name ImmCase_iaddi_16
.syntax new
.vu
.init_vf_all
.init_vi_all
--enter
--endenter
   iaddi iA, vi00, 16
   isw.x iA, 0(vi00)
--exit
--endexit
