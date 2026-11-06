.name Repro_clipw_implicit_w
.init_vi_all
.init_vf_all
.syntax new
.vu
--enter
--endenter
    lq fA, 1(vi00)
    clipw.xyz fA, fA
    fcand vi01, 0x3f
    isw.x vi01, 2(vi00)
--exit
--endexit
