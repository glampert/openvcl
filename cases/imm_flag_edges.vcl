.name ImmCase_flag_edges
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
   fcset 0xffffff
   fcand vi01, 0xffffff
   fsset 0xfff
   fsand vi02, 0xfff
   isw.x vi01, 0(vi00)
   isw.x vi02, 1(vi00)
--exit
--endexit
