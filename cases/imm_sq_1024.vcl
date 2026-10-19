; show: sq
.name ImmCase_sq_1024
.syntax new
.vu
.init_vf_all
.init_vi_all
--enter
--endenter
   lq vA, 0(vi00)
   sq vA, 1024(vi00)
--exit
--endexit
