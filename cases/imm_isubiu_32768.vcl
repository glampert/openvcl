; show: isubiu
.name ImmCase_isubiu_32768
.syntax new
.vu
.init_vf_all
.init_vi_all
--enter
--endenter
   isubiu iA, vi00, 32768
   isw.x iA, 0(vi00)
--exit
--endexit
