; show: fc[a-z]+
.name ImmCase_fcor_big_dec
.syntax new
.vu
.init_vf_all
.init_vi_all
--enter
--endenter
   fcset 0
   fcor vi01, 16777216
   isw.x vi01, 0(vi00)
--exit
--endexit
