"""Cycle model for VU1 micro programs (.vsm), for openvcl's and SCE vcl's output alike.

Timing follows the VU User's Manual (7.4, 3.4): every VF result (FMAC, LQ,
MOVE, MR32, MFIR, MFP, ITOF/FTOI, RGET/RNEXT) and VI load (ILW/ILWR) can be
read 4 cycles after issue, and a read before that stalls the whole pair, field
by field; IALU results are ready the next cycle. Q and P are not interlocked:
only WAITQ/WAITP wait for them, and a DIV/SQRT/RSQRT (or EFU op) waits for the
unit to be free. ACC and the flags never stall. XGKICK's wait for the GIF is
not modelled.

    vucycles.py report FILE.vsm        per-loop and per-stream figures
    vucycles.py json FILE.vsm          the same as JSON
"""
import json, os, re, sys
from collections import defaultdict

# openvcl --dump-instruction-info-json, written there by setup.sh
INFO_PATH = os.environ.get('VUCYCLES_INFO') or os.path.join(
    os.path.expanduser(os.environ.get('SCEVCL_HOME', '~/.cache/openvcl-scevcl')), 'info.json')
TABLE = defaultdict(list)
for _it in json.load(open(INFO_PATH))['instructions']:
    TABLE[_it['mnemonic']].append(_it)

FIELDS = 'xyzw'
VF_LATENCY = 4
VI_LOAD_LATENCY = 4
ROTATE = {'x': 'y', 'y': 'z', 'z': 'w', 'w': 'x'}
CONDITIONAL = {'ibeq', 'ibne', 'ibgez', 'ibgtz', 'iblez', 'ibltz'}


def resolve(token):
    """(base mnemonic, broadcast field, dest fields, flags) or None if token is no mnemonic."""
    flags = set(re.findall(r'\[([A-Za-z])\]', token))
    name = re.sub(r'\[[A-Za-z]\]', '', token).lower()
    dest = None
    if '.' in name:
        name, dest = name.split('.', 1)
        if not dest or any(c not in FIELDS for c in dest):
            return None
    if not re.match(r'^[a-z][a-z0-9]*$', name):
        return None
    if name in TABLE:
        return name, None, dest, flags
    if name[-1] in FIELDS and name[:-1] in TABLE and any('vf:bc' in e['operand']['pattern'] for e in TABLE[name[:-1]]):
        return name[:-1], name[-1], dest, flags
    return None


class Instr(object):
    def __init__(self, text, base, bc, dest, flags, operands):
        self.text, self.base, self.bc, self.flags = text, base, bc, flags
        self.dest = dest or 'xyzw'
        self.operands = operands
        entries = TABLE[base]
        if bc:
            entries = [e for e in entries if 'vf:bc' in e['operand']['pattern']] or entries
        else:
            entries = [e for e in entries if 'vf:bc' not in e['operand']['pattern']] or entries
        self.entry = entries[0]
        self.unit = self.entry['unit']
        self.reads, self.writes = [], []        # (kind, register, fields)
        self.target = None
        self.decode()

    def decode(self):
        pattern = self.entry['operand']['pattern']
        pieces = pattern.split(',') if pattern else []
        for piece, op in zip(pieces, self.operands):
            roles = piece.split(':')
            kind = roles[0]
            if kind.startswith('(vi)') or '(vi)' in piece:
                m = re.match(r'^(.*)\(\s*(--)?\s*(vi\d\d)\s*(\+\+)?\s*\)$', op, re.I)
                if m:
                    reg = m.group(3).upper()
                    self.reads.append(('vi', reg, None))
                    if m.group(2) or m.group(4):
                        self.writes.append(('vi', reg, 1))
                continue
            if kind == 'vf':
                m = re.match(r'^(vf\d\d)([xyzw]*)$', op, re.I)
                if not m:
                    continue
                reg, suffix = m.group(1).upper(), m.group(2).lower()
                if 'write' in roles:
                    fields = self.dest
                elif 'bc' in roles:
                    fields = self.bc or suffix[-1:] or 'x'
                elif 'flag' in roles:
                    fields = suffix or 'x'
                elif 'wcomp' in roles:
                    fields = 'w'
                elif 'rotate' in roles:
                    fields = ''.join(ROTATE[f] for f in self.dest)
                elif 'dest' in roles:
                    fields = self.dest
                else:
                    fields = suffix or 'xyzw'
                if 'write' in roles:
                    self.writes.append(('vf', reg, fields))
                else:
                    self.reads.append(('vf', reg, fields))
            elif kind == 'vi':
                m = re.match(r'^(vi\d\d)$', op, re.I)
                if not m:
                    continue
                reg = m.group(1).upper()
                if 'write' in roles:
                    self.writes.append(('vi', reg, VI_LOAD_LATENCY if self.base in ('ilw', 'ilwr') else 1))
                else:
                    self.reads.append(('vi', reg, None))
                if 'branch' in roles:
                    self.target = None
            elif kind == 'imm' and 'branch' in roles:
                self.target = op

    @property
    def is_branch(self):
        return self.entry['branch_delay_slots'] > 0

    def __repr__(self):
        return self.text


class Pair(object):
    def __init__(self, upper, lower, line):
        self.upper, self.lower, self.line = upper, lower, line
        self.flags = (upper.flags if upper else set()) | (lower.flags if lower else set())
        self.labels = []

    @property
    def branch(self):
        return self.lower if self.lower is not None and self.lower.is_branch else None

    def instrs(self):
        return [i for i in (self.upper, self.lower) if i is not None]

    def text(self):
        return '%-34s %s%s' % (self.upper.text if self.upper else '?', self.lower.text if self.lower else '?',
                               ''.join('[%s]' % f for f in sorted(self.flags)) if self.flags - {'I'} else '')


def parse_instrs(code):
    tokens = code.split()
    out = []
    i = 0
    while i < len(tokens):
        r = resolve(tokens[i])
        if r is None:
            raise ValueError('expected a mnemonic at %r in %r' % (tokens[i], code))
        start = i
        i += 1
        ops = []
        while i < len(tokens):
            if ops and not ops[-1].endswith(',') and not tokens[i].startswith(',') and resolve(tokens[i]) is not None:
                break
            if not ops and resolve(tokens[i]) is not None and len(out) == 0 and r[0] == 'nop':
                break
            if not ops and resolve(tokens[i]) is not None and TABLE[r[0]][0]['operand']['arguments'] == 0:
                break
            ops.append(tokens[i])
            i += 1
        operands = [o for o in ''.join(ops).split(',') if o != ''] if ops else []
        text = (tokens[start] + ' ' + ', '.join(operands)).strip()
        out.append(Instr(text, r[0], r[1], r[2], r[3], operands))
    return out


def parse_vsm(path):
    pairs, pending, streams = [], [], []
    for n, raw in enumerate(open(path, errors='replace'), 1):
        m = re.match(r'^;\s*_L[NP]OPT_w=\[\s*([^\]]*)\]\s*(\d+)\s*\[(\d+)\s+(\d+)\]\s*(\d+)\s*\[([^\]]*)\]', raw)
        if m:
            streams.append({'kind': m.group(1).strip(), 'instructions': int(m.group(2)), 'a': int(m.group(3)),
                            'b': int(m.group(4)), 'cycles': int(m.group(5)), 'label': m.group(6).strip(),
                            'start': len(pairs)})
        line = raw.split(';', 1)[0].rstrip()
        if not line.strip() or line.strip().startswith('.'):
            continue
        m = re.match(r'^\s*([A-Za-z_$.][\w$.]*)\s*:\s*(.*)$', line)
        if m:
            pending.append(m.group(1))
            line = m.group(2)
            if not line.strip():
                continue
        instrs = parse_instrs(line)
        upper = instrs[0] if instrs else None
        lower = instrs[1] if len(instrs) > 1 else None
        p = Pair(upper, lower, n)
        p.labels, pending = pending, []
        pairs.append(p)
    labels = {}
    for i, p in enumerate(pairs):
        for l in p.labels:
            labels[l] = i
    for l in pending:
        labels[l] = len(pairs)
    return pairs, labels, streams


class State(object):
    def __init__(self):
        self.vf = {}
        self.vi = {}
        self.q_ready = self.p_ready = self.fdiv_free = self.efu_free = -100
        self.cycle = -1

    def copy(self):
        s = State()
        s.vf, s.vi = dict(self.vf), dict(self.vi)
        s.q_ready, s.p_ready, s.fdiv_free, s.efu_free, s.cycle = self.q_ready, self.p_ready, self.fdiv_free, self.efu_free, self.cycle
        return s


def issue(state, pair):
    """Issue pair after state; returns (cycle, stall cycles)."""
    t = state.cycle + 1
    need = t
    for ins in pair.instrs():
        for kind, reg, fields in ins.reads:
            if kind == 'vf' and reg != 'VF00':
                for f in fields:
                    need = max(need, state.vf.get((reg, f), -100))
            elif kind == 'vi' and reg != 'VI00':
                need = max(need, state.vi.get(reg, -100))
        if ins.unit == 'fdiv':
            need = max(need, state.fdiv_free)
        if ins.unit == 'efu' and ins.base != 'mfp':
            need = max(need, state.efu_free)
        if ins.base == 'waitq':
            need = max(need, state.q_ready)
        if ins.base == 'waitp':
            need = max(need, state.p_ready)
    t = need
    for ins in pair.instrs():
        for kind, reg, extra in ins.writes:
            if kind == 'vf' and reg != 'VF00':
                for f in extra:
                    state.vf[(reg, f)] = t + VF_LATENCY
            elif kind == 'vi' and reg != 'VI00':
                state.vi[reg] = t + extra
        if ins.base in ('bal', 'jalr'):
            for kind, reg, extra in ins.writes:
                state.vi[reg] = t + 1
        if ins.unit == 'fdiv':
            state.q_ready = t + ins.entry['latency']
            state.fdiv_free = t + ins.entry['throughput']
        if ins.unit == 'efu' and ins.base != 'mfp':
            state.p_ready = t + ins.entry['latency']
            state.efu_free = t + ins.entry['throughput']
    stall = t - (state.cycle + 1)
    state.cycle = t
    return t, stall


def loops(pairs, labels):
    """Back edges: (head index, branch index, head label)."""
    out = []
    for k, p in enumerate(pairs):
        b = p.branch
        if b is not None and b.target in labels and labels[b.target] <= k:
            out.append((labels[b.target], k, b.target))
    return out


def walk(pairs, labels, start, state, stop, take_back_edge_to=None, max_pairs=100000):
    """Simulate from start, following fall-through for conditional branches (except a
    branch back to take_back_edge_to, which is taken) and taking unconditional ones,
    until stop(index) or the end of the program. Returns the list of (index, cycle)."""
    trace = []
    i = start
    steps = 0
    while 0 <= i < len(pairs) and steps < max_pairs:
        if trace and stop(i):
            break
        p = pairs[i]
        t, _ = issue(state, p)
        trace.append((i, t))
        steps += 1
        if 'E' in p.flags:
            if i + 1 < len(pairs):
                issue(state, pairs[i + 1])
                trace.append((i + 1, state.cycle))
            return trace
        b = p.branch
        if b is not None:
            # the delay slot
            if i + 1 < len(pairs):
                issue(state, pairs[i + 1])
                trace.append((i + 1, state.cycle))
                steps += 1
            if b.base in CONDITIONAL:
                if take_back_edge_to is not None and b.target == take_back_edge_to:
                    i = labels[b.target]
                    continue
                i += 2
                continue
            if b.base in ('b', 'bal') and b.target in labels:
                i = labels[b.target]
                continue
            return trace          # jr/jalr: leaves the program
        i += 1
    return trace


def loop_cycles(pairs, labels, head, branch, label, iterations=8):
    """Steady-state cycles per iteration of the loop head..branch (inner forward
    branches not taken, inner loops run once), and the static body length."""
    state = State()
    arrivals = []
    i = head
    for _ in range(iterations):
        arrivals.append(state.cycle + 1)
        trace = walk(pairs, labels, head, state, stop=lambda j: j == head, take_back_edge_to=None if False else label)
        # walk stops when it comes back to head
        last = trace[-1][0] if trace else head
        if not trace or last not in (branch, branch + 1):
            # left the loop: did not return to the head
            return None
    per_iter = arrivals[-1] - arrivals[-2]
    stalls = per_iter - (branch + 2 - head)
    return {'label': label, 'head': head, 'branch': branch, 'body_pairs': branch + 2 - head,
            'cycles_per_iteration': per_iter, 'stall_cycles_per_iteration': stalls}


def analyse(path):
    pairs, labels, streams = parse_vsm(path)
    result = {'file': path, 'pairs': len(pairs), 'loops': [], 'streams': []}
    seen = set()
    for head, branch, label in loops(pairs, labels):
        key = (head, branch)
        if key in seen:
            continue
        seen.add(key)
        lc = loop_cycles(pairs, labels, head, branch, label)
        if lc:
            result['loops'].append(lc)
    # straight-line streams (SCE reports its own estimate for each)
    for s in streams:
        end = len(pairs)
        for t in streams:
            if t['start'] > s['start']:
                end = min(end, t['start'])
        state = State()
        first = None
        for i in range(s['start'], end):
            t, _ = issue(state, pairs[i])
            first = t if first is None else first
        model = (state.cycle + 1 - first) if first is not None else 0
        s2 = dict(s)
        s2['model_cycles'] = model
        s2['pairs'] = end - s['start']
        result['streams'].append(s2)
    # the whole program along the fall-through path, loops once
    state = State()
    trace = walk(pairs, labels, 0, state, stop=lambda j: False)
    result['fallthrough_cycles'] = state.cycle + 1
    result['fallthrough_pairs'] = len(trace)
    return result


def trace_loop(path, label, iterations=6):
    """Print the last iteration of loop `label`: each pair with its issue cycle and stall."""
    pairs, labels, _ = parse_vsm(path)
    head = labels[label]
    state = State()
    last = []
    for it in range(iterations):
        rows = []
        i = head
        first = True
        while 0 <= i < len(pairs):
            if not first and i == head:
                break
            first = False
            p = pairs[i]
            before = state.cycle
            t, stall = issue(state, p)
            rows.append((i, t, stall, p))
            b = p.branch
            if 'E' in p.flags:
                break
            if b is not None:
                if i + 1 < len(pairs):
                    t2, s2 = issue(state, pairs[i + 1])
                    rows.append((i + 1, t2, s2, pairs[i + 1]))
                if b.base in CONDITIONAL and b.target != label:
                    i += 2
                    continue
                if b.target in labels:
                    i = labels[b.target]
                    continue
                break
            i += 1
        last = rows
    base = last[0][1]
    for i, t, stall, p in last:
        print('%4d %+4d %s %s' % (i, t - base, ('stall %d' % stall).ljust(8) if stall else ' ' * 8, p.text()))
    print('iteration: %d cycles' % (state.cycle + 1 - base))


def main():
    cmd, path = sys.argv[1], sys.argv[2]
    if cmd == 'trace':
        trace_loop(path, sys.argv[3])
        return
    r = analyse(path)
    if cmd == 'json':
        print(json.dumps(r, indent=1))
        return
    print('%s: %d pairs; fall-through path %d cycles over %d pairs' % (path, r['pairs'], r['fallthrough_cycles'], r['fallthrough_pairs']))
    for l in r['loops']:
        print('  loop %-28s body %4d pairs, %4d cycles/iteration (%d stall)' % (l['label'], l['body_pairs'], l['cycles_per_iteration'], l['stall_cycles_per_iteration']))
    for s in r['streams']:
        print('  stream %-40s sce=%4d model=%4d pairs=%d' % (s['label'][:40], s['cycles'], s['model_cycles'], s['pairs']))


if __name__ == '__main__':
    main()
