.name ImmCase_fcset_big
.syntax new
.vu
.init_vf_all
.init_vi_all
--enter
--endenter
   fcset 0x1000000
   fcand vi01, 1
   isw.x vi01, 0(vi00)
--exit
--endexit
