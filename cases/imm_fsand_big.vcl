; show: fs[a-z]+
.name ImmCase_fsand_big
.syntax new
.vu
.init_vf_all
.init_vi_all
--enter
--endenter
   fsand vi01, 0x1000
   isw.x vi01, 0(vi00)
--exit
--endexit
