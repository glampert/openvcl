.name ImmCase_iaddi_expr
.syntax new
.vu
.init_vf_all
.init_vi_all
--enter
--endenter
   iaddi iA, vi00, (0 - 18)
   isw.x iA, 0(vi00)
--exit
--endexit
