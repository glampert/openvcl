.name Repro_masked_write_branch
.init_vi_all
.init_vf_all
.syntax new
.vu
--enter
--endenter
    lq fX, 1(vi00)
    ilw.x iC, 0(vi00)
    move vCol, vf00
    ibeq iC, vi00, lSkip
    move.xyz vCol, vf00
    move.w vCol, fX
lSkip:
    sq vCol, 2(vi00)
--exit
--endexit
