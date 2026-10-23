import os, sys, time
from qiling import Qiling
from qiling.const import QL_VERBOSE
from qiling.extensions import pipe
from unicorn import UC_HOOK_BLOCK
rootfs, name, mode = sys.argv[1], sys.argv[2], sys.argv[3]
ql = Qiling([os.path.join(rootfs, 'bin', 'vcl'), '/work/' + name], rootfs, verbose=QL_VERBOSE.DISABLED, console=False)
ql.os.stdout = pipe.SimpleOutStream(1); ql.os.stderr = pipe.SimpleOutStream(2)
stats = [0, 0]
if mode == 'count':
    def blk(uc, addr, size, user):
        stats[0] += 1; stats[1] += size
    ql.uc.hook_add(UC_HOOK_BLOCK, blk)
t = time.time(); ql.run(); dt = time.time() - t
print('%s mode=%s seconds=%.2f blocks=%d bytes=%d approx_instr=%.0f' % (name, mode, dt, stats[0], stats[1], stats[1] / 3.5))
