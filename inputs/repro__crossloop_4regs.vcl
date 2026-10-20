.name Repro_crossloop_4regs
.init_vi VI01, VI02, VI03, VI04
.init_vf_all
.syntax new
.vu
--enter
--endenter
    iaddiu iCount, vi00, 3
lA:
    iaddiu iVal, iCount, 10
    iaddi iCount, iCount, -1
    ibne iCount, vi00, lA
    iaddiu iCount2, vi00, 3
lB:
    ilw.x iCond, 0(iCount2)
    ibeq iCond, vi00, lBSkip
    iaddiu iTmp, vi00, 1
    isw.x iTmp, 0(vi00)
lBSkip:
    isw.x iVal, 1(vi00)
    iaddiu iTmp2, vi00, 5
    iaddiu iTmp3, iTmp2, 5
    isw.x iTmp3, 2(vi00)
    iaddi iCount2, iCount2, -1
    ibne iCount2, vi00, lB
--exit
--endexit
