; show: [ls]q
.name ImmCase_mem_expr
.syntax new
.vu
.init_vf_all
.init_vi_all
--enter
--endenter
   lq vA, (1010 + 14)(vi00)
   sq vA, 0(vi00)
--exit
--endexit
