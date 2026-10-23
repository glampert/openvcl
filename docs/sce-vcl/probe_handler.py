import os, sys
from qiling import Qiling
from qiling.const import QL_VERBOSE, QL_INTERCEPT
from qiling.extensions import pipe
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
rootfs = sys.argv[1]
seen = {}
def on_sigaction(ql, signum, act, oldact, *rest):
    if act and signum == 14:
        h = ql.mem.read_ptr(act, 4); flags = ql.mem.read_ptr(act + 4, 4); rest_ = ql.mem.read_ptr(act + 8, 4)
        if h and h not in seen:
            seen[h] = (flags, rest_)
ql = Qiling([os.path.join(rootfs, 'bin', 'vcl'), '/work/' + sys.argv[2]], rootfs, verbose=QL_VERBOSE.DISABLED, console=False)
ql.os.stdout = pipe.SimpleOutStream(1); ql.os.stderr = pipe.SimpleOutStream(2)
ql.os.set_syscall('rt_sigaction', on_sigaction, QL_INTERCEPT.ENTER)
ql.run()
md = Cs(CS_ARCH_X86, CS_MODE_32)
for h, (flags, r) in seen.items():
    print('handler %#x flags %#x restorer %#x' % (h, flags, r))
    code = bytes(ql.mem.read(h, 96))
    for i in md.disasm(code, h):
        print('  %#x: %s %s' % (i.address, i.mnemonic, i.op_str))
        if i.mnemonic == 'ret': break
