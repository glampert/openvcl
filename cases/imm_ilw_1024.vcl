; show: ilw
.name ImmCase_ilw_1024
.syntax new
.vu
.init_vf_all
.init_vi_all
--enter
--endenter
   ilw.x iA, 1024(vi00)
   isw.x iA, 0(vi00)
--exit
--endexit
