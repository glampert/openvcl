.name ImmCase_fsset_big
.syntax new
.vu
.init_vf_all
.init_vi_all
--enter
--endenter
   lq vA, 0(vi00)
   div q, vf00[w], vA[x]
   waitq
   mulq.x vB, vA, q
   sq.x vB, 1(vi00)
   fsset 4096
   fsand vi01, 1
   isw.x vi01, 0(vi00)
--exit
--endexit
