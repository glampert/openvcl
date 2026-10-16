.name ClipCase_bare
.syntax new
.vu
.init_vf_all
.init_vi_all
--enter
--endenter
   lq          vA,   1(vi00)
   clipw.xyz   vA, vA
   fcand       VI01, 0x3FFFF
   iaddiu      adc,  VI01, 0x7FFF
   isw.w       adc,  2(vi00)
--exit
--endexit
