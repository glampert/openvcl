; show: f[cs][a-z]+
.name ImmCase_flag_edges
.syntax new
.vu
.init_vf_all
.init_vi_all
--enter
--endenter
   fcset 0xffffff
   fcand vi01, 0xffffff
   fsset 0xfff
   fsand vi02, 0xfff
   isw.x vi01, 0(vi00)
   isw.x vi02, 1(vi00)
--exit
--endexit
