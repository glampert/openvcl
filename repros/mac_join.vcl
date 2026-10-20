.name Repro_mac_join
.init_vi_all
.init_vf_all
.syntax new
.vu
--enter
--endenter
    ilw.x iFmt, 0(vi00)
    lq fA, 1(vi00)
    ibeq iFmt, vi00, lFmtB
    sub.xyz fB, fA, vf00
    b lJoin
lFmtB:
    add.xyz fB, fA, vf00
lJoin:
    fmand iM, iFmt
    isw.x iM, 2(vi00)
    sq.xyz fB, 3(vi00)
--exit
--endexit
