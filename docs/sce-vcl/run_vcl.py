"""Run SCE vcl 1.4beta7 (i386 Linux) under Qiling.

    run_vcl.py ROOTFS INPUT OUT_VSM OUT_ERR [vcl args...]

INPUT is copied into ROOTFS/work; vcl writes the program to stdout.

vcl bounds each optimiser search with alarm(): its SIGALRM handler (at
HANDLER) only stores 1 through the pointer at FLAG_POINTER, which the search
polls. Qiling's alarm() does nothing, so the searches would never be cut
short. Here alarm(n) arms a deadline of n * VCL_TIME_SCALE seconds of this
process's CPU time, and a watcher thread does what the handler does when it
passes. VCL_TIME_SCALE is how much slower the emulator runs vcl than the
native machine being imitated; 0 disables the alarms."""
import math, os, shutil, sys, threading, time
from qiling import Qiling
from qiling.const import QL_VERBOSE
from qiling.extensions import pipe

HANDLER = 0x804ac30
FLAG_POINTER = 0x80bbd90
SIGALRM = 14

rootfs, src, out_vsm, out_err = sys.argv[1:5]
extra = sys.argv[5:]
scale = float(os.environ.get('VCL_TIME_SCALE', '25'))
name = os.path.basename(src)
shutil.copy(src, os.path.join(rootfs, 'work', name))

debug = bool(os.environ.get('QLDEBUG'))
stdout = pipe.SimpleOutStream(1)
stderr = pipe.SimpleOutStream(2)
ql = Qiling([os.path.join(rootfs, 'bin', 'vcl')] + extra + ['/work/' + name], rootfs,
            env={'PATH': '/bin:/usr/bin'},
            verbose=QL_VERBOSE.DEBUG if debug else QL_VERBOSE.DISABLED, console=debug)
ql.os.stdout = stdout
ql.os.stderr = stderr

lock = threading.Lock()
state = {'deadline': None, 'armed': 0, 'fired': 0, 'other': 0, 'done': False}

def sys_alarm(ql, seconds):
    with lock:
        now = time.process_time()
        previous = state['deadline']
        remaining = 0 if previous is None else max(1, math.ceil((previous - now) / scale))
        if seconds and scale > 0:
            state['deadline'] = now + seconds * scale
            state['armed'] += 1
        else:
            state['deadline'] = None
    return remaining

def fire():
    act = ql.os.sigaction_act[SIGALRM] if SIGALRM < len(ql.os.sigaction_act) else None
    handler = act[0] if act else 0
    if handler == HANDLER:
        flag = ql.mem.read_ptr(FLAG_POINTER, 4)
        if flag:
            ql.mem.write(flag, b'\x01')
        state['fired'] += 1
    else:
        state['other'] += 1

def watch():
    while not state['done']:
        time.sleep(0.01)
        with lock:
            deadline = state['deadline']
            if deadline is not None and time.process_time() >= deadline:
                state['deadline'] = None
                fire()

ql.os.set_syscall('alarm', sys_alarm)
watcher = threading.Thread(target=watch, daemon=True)
watcher.start()
start, cpu = time.time(), time.process_time()
ql.run()
state['done'] = True
elapsed, cpu = time.time() - start, time.process_time() - cpu
open(out_vsm, 'wb').write(stdout.read())
open(out_err, 'wb').write(stderr.read())
code = getattr(ql.os, 'exit_code', None)
print('%s exit=%s seconds=%.1f cpu=%.1f scale=%g alarms_armed=%d alarms_fired=%d alarms_unhandled=%d native_estimate=%.2f'
      % (name, code, elapsed, cpu, scale, state['armed'], state['fired'], state['other'],
         cpu / scale if scale else 0))
