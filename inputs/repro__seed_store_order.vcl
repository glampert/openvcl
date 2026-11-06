.name Repro_seed_store_order
.init_vi_all
.init_vf_all
.syntax new
.vu
--enter
--endenter
    lq fP0, 1(vi00)
    lq fP1, 2(vi00)
    iaddiu iWalk, vi00, 960
    sq fP0, 0(iWalk)
    sq fP1, 1(iWalk)
    sq fP0, 2(iWalk)
    add.yz fCut, vf00, fP1
    sq.yz fCut, 960 + 1(vi00)
    iaddiu iCount, vi00, 3
    isw.x iCount, 1009(vi00)
--exit
--endexit
