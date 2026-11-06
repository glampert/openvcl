.name Repro_immediates_out_of_range
.init_vi_all
.init_vf_all
.syntax new
.vu
--enter
--endenter
    ilw.x iA, 0(vi00)
    iaddi iB, iA, -18
    iaddiu iC, iA, 40000
    isubiu iD, iA, -1
    isw.x iB, 2000(vi00)
    isw.x iC, 3(vi00)
    isw.x iD, 4(vi00)
--exit
--endexit
