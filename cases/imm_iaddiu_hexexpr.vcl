.name ImmCase_iaddiu_hexexpr
.syntax new
.vu
.init_vf_all
.init_vi_all
--enter
--endenter
   iaddiu iA, vi00, 0x3f | 0x40
   isw.x iA, 0(vi00)
--exit
--endexit
