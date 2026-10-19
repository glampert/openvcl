.name ImmCase_fsand_big
.syntax new
.vu
.init_vf_all
.init_vi_all
--enter
--endenter
   lq vA, 0(vi00)
   div q, vf00[w], vA[x]
   mulq.x vB, vA, q
   sq.x vB, 1(vi00)
   fsand vi01, 0x1000
   isw.x vi01, 0(vi00)
--exit
--endexit
