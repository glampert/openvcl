.name ImmCase_mem_edges
.syntax new
.vu
.init_vf_all
.init_vi_all
--enter
--endenter
   lq vA, 1023(vi00)
   sq vA, -1024(vi00)
--exit
--endexit
