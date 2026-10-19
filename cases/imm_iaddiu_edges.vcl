; show: iaddiu|isubiu
.name ImmCase_iaddiu_edges
.syntax new
.vu
.init_vf_all
.init_vi_all
--enter
--endenter
   iaddiu iA, vi00, 32767
   iaddiu iB, vi00, 0x7fff
   isubiu iC, vi00, 0
   isw.x iA, 0(vi00)
   isw.x iB, 1(vi00)
   isw.x iC, 2(vi00)
--exit
--endexit
