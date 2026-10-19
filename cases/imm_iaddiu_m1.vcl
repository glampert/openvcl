.name ImmCase_iaddiu_m1
.syntax new
.vu
.init_vf_all
.init_vi_all
--enter
--endenter
   iaddiu iA, vi00, -1
   isw.x iA, 0(vi00)
--exit
--endexit
