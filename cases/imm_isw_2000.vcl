; show: isw
.name ImmCase_isw_2000
.syntax new
.vu
.init_vf_all
.init_vi_all
--enter
--endenter
   iaddiu iA, vi00, 1
   isw.x iA, 2000(vi00)
--exit
--endexit
