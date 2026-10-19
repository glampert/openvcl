; show: isubiu
.name ImmCase_isubiu_m1
.syntax new
.vu
.init_vf_all
.init_vi_all
--enter
--endenter
   isubiu iA, vi00, -1
   isw.x iA, 0(vi00)
--exit
--endexit
