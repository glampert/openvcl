"""compare.py OURS_DIR SCE_DIR [SCE_DIR...]: per-program code size and loop cycles, openvcl vs SCE vcl.

The SCE directories are searched in order for each program's .vsm (and .status)."""
import glob, json, os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vucycles

ours_dir, sce_dirs = sys.argv[1], sys.argv[2:]


def sce_file(name):
    for d in sce_dirs:
        f = os.path.join(d, name + '.vsm')
        st = os.path.join(d, name + '.status')
        if os.path.exists(st) and 'exit=0 ' in open(st).read() and os.path.getsize(f) > 0:
            return f, open(st).read()
    return None, None


rows = []
for f in sorted(glob.glob(os.path.join(ours_dir, '*.vsm'))):
    name = os.path.basename(f)[:-4]
    s, status = sce_file(name)
    if not s:
        continue
    o = vucycles.analyse(f)
    c = vucycles.analyse(s)
    fired = re.search(r'alarms_fired=(\d+)', status)
    row = {'program': name, 'ours_pairs': o['pairs'], 'sce_pairs': c['pairs'],
           'ours_path': o['fallthrough_cycles'], 'sce_path': c['fallthrough_cycles'],
           'alarms_fired': int(fired.group(1)) if fired else None, 'loops': []}
    ol = {l['label']: l for l in o['loops']}
    cl = {l['label']: l for l in c['loops']}
    # SCE renames the loops it software-pipelines (--LoopCS) to
    # EXPL_<file>_<stream>__MAIN_LOOP; pair them, in order, with the
    # source's --LoopCS loops it did not keep by name. openvcl's known-loop
    # kernels are <label>__MAIN_LOOP.
    src = open(os.path.join(os.path.expanduser(os.environ.get('CORPUS_INPUTS', '~/Repos/GitHub/openvcl-corpus/inputs')), name + '.vcl')).read().splitlines()
    loopcs = []
    for k, line in enumerate(src):
        m = re.match(r'^\s*([A-Za-z_][\w]*):\s*(--LoopCS)?', line)
        if m and (m.group(2) or (k + 1 < len(src) and re.match(r'^\s*--LoopCS', src[k + 1]))):
            loopcs.append(m.group(1))
    kernels = [l['label'] for l in sorted(c['loops'], key=lambda l: l['head']) if l['label'].endswith('__MAIN_LOOP')]
    for k in list(kernels):
        for label in sorted(loopcs, key=len, reverse=True):
            if k.endswith('_' + label + '__MAIN_LOOP') and label not in cl:
                cl[label] = cl.pop(k)
                kernels.remove(k)
                break
    for label in loopcs:
        if label in cl or not kernels:
            continue
        k = kernels.pop(0)
        cl[label] = cl.pop(k)
    for label in list(ol):
        if label.endswith('__MAIN_LOOP') and label[:-len('__MAIN_LOOP')] in loopcs:
            ol[label[:-len('__MAIN_LOOP')]] = ol.pop(label)
    for label in sorted(set(ol) | set(cl)):
        row['loops'].append({'label': label,
                             'ours': ol[label]['cycles_per_iteration'] if label in ol else None,
                             'ours_pairs': ol[label]['body_pairs'] if label in ol else None,
                             'sce': cl[label]['cycles_per_iteration'] if label in cl else None,
                             'sce_pairs': cl[label]['body_pairs'] if label in cl else None})
    # model vs SCE's own per-stream estimate
    row['sce_streams'] = len(c['streams'])
    row['sce_streams_matching_model'] = sum(1 for st in c['streams'] if st['cycles'] == st['model_cycles'])
    rows.append(row)

if '--json' in os.environ.get('COMPARE_FLAGS', ''):
    print(json.dumps(rows, indent=1))
    sys.exit(0)

print('%-42s %7s %7s %8s %8s  %s' % ('program', 'pairs', 'pairs', 'path', 'path', 'loops: label ours/sce cycles per iteration'))
print('%-42s %7s %7s %8s %8s' % ('', 'ours', 'sce', 'ours', 'sce'))
for r in rows:
    loops = '; '.join('%s %s/%s' % (l['label'], l['ours'] if l['ours'] is not None else '-', l['sce'] if l['sce'] is not None else '-')
                      for l in r['loops'])
    print('%-42s %7d %7d %8d %8d  %s%s' % (r['program'][:42], r['ours_pairs'], r['sce_pairs'], r['ours_path'], r['sce_path'], loops,
                                         '' if not r['alarms_fired'] else '  [sce alarms fired: %d]' % r['alarms_fired']))
total_streams = sum(r['sce_streams'] for r in rows)
matching = sum(r['sce_streams_matching_model'] for r in rows)
print('\nmodel vs SCE\'s own stream estimates: %d of %d streams agree' % (matching, total_streams))
