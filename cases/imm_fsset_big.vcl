; show: fs[a-z]+
.name ImmCase_fsset_big
.syntax new
.vu
.init_vf_all
.init_vi_all
--enter
--endenter
   fsset 4096
   fsand vi01, 1
   isw.x vi01, 0(vi00)
--exit
--endexit
