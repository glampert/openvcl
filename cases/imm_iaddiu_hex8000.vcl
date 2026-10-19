.name ImmCase_iaddiu_hex8000
.syntax new
.vu
.init_vf_all
.init_vi_all
--enter
--endenter
   iaddiu iA, vi00, 0x8000
   isw.x iA, 0(vi00)
--exit
--endexit
