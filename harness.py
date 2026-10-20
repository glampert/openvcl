#!/usr/bin/env python3
"""Corpus harness for openvcl changes. It lives on the branch test/local-corpus, which is never
opened as a pull request or merged into main; README.md says how to set it up.

  harness.py prepare            preprocess every corpus program into inputs/, and record
                                where each came from in inputs/manifest.json
  harness.py run BASE CAND OUT  compile every input with both binaries (plain and -c),
                                compare, and run vu-checker on both sets of outputs
  harness.py sanitize SRC OUT   build the openvcl tree SRC with ASan and UBSan, run its
                                tests, and compile every input with it and a release build

Byte-identical rule: a program that compiles with BASE must compile to the same plain
.vsm with CAND. -c output may differ, but with comments stripped it must equal CAND's
plain output. vu-checker's 'branches' check is ignored (it reads LOI's float immediate
as a branch).
"""
import hashlib, json, os, re, shutil, subprocess, sys
from concurrent.futures import ThreadPoolExecutor

HERE    = os.path.dirname(os.path.abspath(__file__))
REPOS   = os.environ.get('CORPUS_REPOS', os.path.expanduser('~/Repos/GitHub'))   # the source checkouts
OPENVCL = os.environ.get('OPENVCL_REPO', os.path.join(REPOS, 'openvcl'))
PS2DEV  = os.environ.get('PS2DEV', os.path.expanduser('~/ps2dev'))
VCLPP   = os.path.join(REPOS, 'quake2-ps2/build/tools/vclpp')   # pinned vclpp 2
MASP    = os.path.join(PS2DEV, 'bin/masp')
DVPAS   = os.path.join(PS2DEV, 'dvp/bin/dvp-as')
CHECK   = os.path.join(REPOS, 'vu-checker/check_vu_code.py')
INPUTS  = os.path.join(HERE, 'inputs')
FJTRUJY = os.path.join(HERE, 'fjtrujy-openvcl')   # clone of fjtrujy/openvcl, made by prepare
GASP    = ['-g', '--gasp', '$MASP']               # $MASP becomes MASP when compiling

def sh(cmd, cwd=None):
    return subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)

# ---------------------------------------------------------------------------
#  prepare
# ---------------------------------------------------------------------------

def add(entries, group, name, text, flags=(), source=None):
    entries.append({'group': group, 'name': name, 'text': text, 'flags': list(flags), 'source': source or {}})

def source_of(top, path, via, watch=None):
    """Where a program comes from: the checkout's origin and HEAD, the file, and how it became an
    input; 'modified' when the checkout has local changes under watch (default: the file)."""
    url = sh(['git', 'remote', 'get-url', 'origin'], cwd=top).stdout.strip()
    source = {'repo': re.sub(r'\.git$', '', url), 'commit': sh(['git', 'rev-parse', 'HEAD'], cwd=top).stdout.strip(),
              'path': path, 'via': via}
    if sh(['git', 'status', '--porcelain', '--', watch or path], cwd=top).stdout.strip():
        source['modified'] = True
    return source

def vclpp(args, cwd=None):
    with_tmp = os.path.join(HERE, '.tmp.vcl')
    r = sh([VCLPP] + args + [with_tmp], cwd=cwd)
    if r.returncode != 0:
        raise RuntimeError(f'vclpp {" ".join(args)} failed:\n{r.stderr}')
    text = open(with_tmp).read()
    os.remove(with_tmp)
    return text

def prepare():
    entries = []

    # vclpp reads includes as well, so any local change in those checkouts marks a program modified
    for proj in ('quake2-ps2', 'quake-ps2'):
        top = os.path.join(REPOS, proj)
        d = os.path.join(top, 'src/ps2/renderer/vu1progs')
        for prog in ('textured_triangles', 'particles'):
            source = source_of(top, f'src/ps2/renderer/vu1progs/{prog}.vcl', 'vclpp -I src/ps2/renderer/vu1progs -Wundef -Werror -j', '.')
            add(entries, proj, prog, vclpp(['-I', d, '-Wundef', '-Werror', '-j', os.path.join(d, prog + '.vcl')]), source=source)

    tyra = os.path.join(REPOS, 'tyra')
    engine = os.path.join(tyra, 'engine')
    for root, _, files in sorted(os.walk(os.path.join(engine, 'src'))):
        for f in sorted(files):
            if f.endswith('.vclpp'):
                rel = os.path.relpath(os.path.join(root, f), engine)
                source = source_of(tyra, 'engine/' + rel, 'vclpp, run from engine/', '.')
                add(entries, 'tyra', f[:-len('.vclpp')], vclpp([rel], cwd=engine), source=source)

    ps2gl = os.path.join(REPOS, 'ps2gl')
    for rel, incs in (('vu1/scei.vcl', ['vu1', 'include']),
                      ('examples/tricked_out/billboard_renderer.vcl', ['examples/tricked_out', 'vu1', 'include'])):
        args = ['-m', '-x', '-f']
        for i in incs:
            args += ['-I', i]
        try:
            source = source_of(ps2gl, rel, 'vclpp ' + ' '.join(args), '.')
            add(entries, 'ps2gl-extra', os.path.basename(rel)[:-4], vclpp(args + [rel], cwd=ps2gl), source=source)
        except RuntimeError as e:
            print(f'skipping {rel}: {e}', file=sys.stderr)

    # Every .vcl in openvcl's test/ and examples/ trees (test/fixtures are the ps2gl programs).
    for top in ('test', 'examples'):
        for root, dirs, files in os.walk(os.path.join(OPENVCL, top)):
            dirs.sort()
            for f in sorted(files):
                if f.endswith('.vcl'):
                    rel = os.path.relpath(os.path.join(root, f), OPENVCL)
                    if rel.startswith('test/fixtures/'):
                        group, name = 'ps2gl-fixtures', f[:-4]
                    elif top == 'examples':
                        group, name = 'examples', rel.split('/')[1]
                    else:
                        group, name = 'repo', rel.replace('/', '_')[:-4]
                    flags = GASP if '/triangle-gasp/' in rel else []
                    add(entries, group, name, open(os.path.join(root, f)).read(), flags, source_of(OPENVCL, rel, 'copy'))

    if not os.path.isdir(FJTRUJY):
        r = sh(['git', 'clone', '--quiet', '--no-checkout', '--depth', '1', '--no-single-branch',
                'https://github.com/fjtrujy/openvcl.git', FJTRUJY])
        if r.returncode != 0:
            raise RuntimeError(f'cloning fjtrujy/openvcl failed:\n{r.stderr}')
    for branch in ('origin/master', 'origin/ps2gl'):
        commit = sh(['git', 'rev-parse', branch], cwd=FJTRUJY).stdout.strip()
        files = sh(['git', 'ls-tree', '-r', '--name-only', branch], cwd=FJTRUJY).stdout.split()
        for path in files:
            if path.endswith('.vcl'):
                text = sh(['git', 'show', f'{branch}:{path}'], cwd=FJTRUJY).stdout
                flags = GASP if 'triangle-gasp' in path else []
                source = {'repo': 'https://github.com/fjtrujy/openvcl', 'commit': commit, 'path': path, 'via': 'copy'}
                add(entries, 'fjtrujy-' + branch.split('/')[1], path.replace('/', '_')[:-4], text, flags, source)

    repros = os.path.join(HERE, 'repros')
    if os.path.isdir(repros):
        for f in sorted(os.listdir(repros)):
            if f.endswith('.vcl'):
                add(entries, 'repro', f[:-4], open(os.path.join(repros, f)).read(), source={'path': 'repros/' + f, 'via': 'copy'})

    # Dedupe by content and flags: the first name wins, and every source is recorded.
    shutil.rmtree(INPUTS, ignore_errors=True)
    os.makedirs(INPUTS)
    manifest, seen = [], {}
    for e in entries:
        key = hashlib.sha256((e['text'] + '\0' + ' '.join(e['flags'])).encode()).hexdigest()
        source = dict(name=f"{e['group']}/{e['name']}", **e['source'])
        if key in seen:
            seen[key]['sources'].append(source)
            continue
        ident = f"{e['group']}__{e['name']}"
        open(os.path.join(INPUTS, ident + '.vcl'), 'w').write(e['text'])
        item = {'id': ident, 'flags': e['flags'], 'sha256': key, 'sources': [source]}
        seen[key] = item
        manifest.append(item)
    open(os.path.join(INPUTS, 'manifest.json'), 'w').write(json.dumps(manifest, indent=1) + '\n')
    print(f'{len(entries)} programs, {len(manifest)} unique inputs in {INPUTS}')
    for m in manifest:
        if len(m['sources']) > 1:
            print(f"  {m['id']} also covers {', '.join(s['name'] for s in m['sources'][1:])}")
        for s in m['sources']:
            if s.get('modified'):
                print(f"  {s['name']}: the {s['repo']} checkout has local changes", file=sys.stderr)

# ---------------------------------------------------------------------------
#  run
# ---------------------------------------------------------------------------

def compile_one(binary, out, m):
    src = os.path.join(INPUTS, m['id'] + '.vcl')
    flags = [MASP if f == '$MASP' else f for f in m['flags']]
    res = {}
    for mode, extra in (('plain', []), ('c', ['-c'])):
        dst = os.path.join(out, m['id'] + ('.c.vsm' if mode == 'c' else '.vsm'))
        if os.path.exists(dst):
            os.remove(dst)
        r = sh([binary] + flags + extra + ['-o', dst, src])
        text = open(dst).read() if (r.returncode == 0 and os.path.exists(dst)) else None
        res[mode] = {'rc': r.returncode, 'stderr': r.stderr.strip(), 'text': text}
    if res['plain']['text'] is not None:
        shutil.copy(src, os.path.join(out, m['id'] + '.pp.vcl'))
        sh([DVPAS, os.path.join(out, m['id'] + '.vsm'), '-o', os.path.join(out, m['id'] + '.o')])
    return res

def compile_all(binary, out, manifest):
    # one program per core: each writes only its own files (a sanitized openvcl is slow)
    os.makedirs(out, exist_ok=True)
    with ThreadPoolExecutor(os.cpu_count()) as pool:
        results = pool.map(lambda m: compile_one(binary, out, m), manifest)
        return {m['id']: res for m, res in zip(manifest, results)}

# vu-checker's regalloc check reads the -c output (.c.vsm) beside each .o, so its findings can
# change when only the -c output does, even with an identical .vsm.
def vu_check(out, ids):
    objs = [os.path.join(out, i + '.o') for i in ids if os.path.exists(os.path.join(out, i + '.o'))]
    fails = {}
    if not objs:
        return fails
    r = sh([sys.executable, '-I', CHECK] + objs)
    for line in r.stdout.splitlines():
        m = re.match(r'^FAIL (\S+) \((\w+)\): (.*)$', line)
        if m and m.group(2) != 'branches':
            fails.setdefault(m.group(1), []).append(f'{m.group(2)}: {m.group(3)}')
    return fails

def strip_comments(text):
    return '\n'.join(l for l in text.split('\n') if not re.match(r'^\s*;', l))

def run(base, cand, out, checker=True):
    manifest = json.load(open(os.path.join(INPUTS, 'manifest.json')))
    rb = compile_all(base, os.path.join(out, 'base'), manifest)
    rc = compile_all(cand, os.path.join(out, 'cand'), manifest)
    ids = [m['id'] for m in manifest]
    fb = vu_check(os.path.join(out, 'base'), ids) if checker else {}
    fc = vu_check(os.path.join(out, 'cand'), ids) if checker else {}

    summary = {'identical': 0, 'changed': 0, 'new': 0, 'failing': 0, 'broken': 0}
    rows = []
    for i in ids:
        b, c = rb[i], rc[i]
        bp, cp = b['plain']['text'], c['plain']['text']
        notes = []
        if bp is not None and cp is None:
            status = 'BROKEN'; summary['broken'] += 1
            notes.append('candidate fails: ' + c['plain']['stderr'][:200])
        elif bp is None and cp is None:
            status = 'failing'; summary['failing'] += 1
            notes.append(c['plain']['stderr'].split('\n')[0][:160])
        elif bp is None:
            status = 'NEW'; summary['new'] += 1
        elif bp == cp:
            status = 'identical'; summary['identical'] += 1
        else:
            status = 'CHANGED'; summary['changed'] += 1
            notes.append(f"{sum(1 for _ in diff_lines(bp, cp))} diff lines")
        if cp is not None and c['c']['text'] is not None and strip_comments(c['c']['text']) != cp:
            notes.append('-c CHANGES CODE')
        if b['c']['text'] is not None and c['c']['text'] is not None and b['c']['text'] != c['c']['text']:
            notes.append('-c output differs')
        gone = sorted(set(fb.get(i, [])) - set(fc.get(i, [])))
        new = sorted(set(fc.get(i, [])) - set(fb.get(i, [])))
        if gone:
            notes.append(f'vu-checker fixed {len(gone)}')
        if new:
            notes.append(f'vu-checker NEW: {new[0][:140]}' + (f' (+{len(new) - 1})' if len(new) > 1 else ''))
        elif fc.get(i):
            notes.append(f'vu-checker still: {fc[i][0][:120]}' + (f' (+{len(fc[i]) - 1})' if len(fc[i]) > 1 else ''))
        rows.append((i, status, notes))
    print('\n'.join(f'{status:9s} {i:58s} {"; ".join(notes)}' for i, status, notes in rows))
    print(' '.join(f'{k}={v}' for k, v in summary.items()))
    return summary, rows

def diff_lines(a, b):
    import difflib
    for l in difflib.unified_diff(a.split('\n'), b.split('\n'), lineterm='', n=0):
        if l.startswith(('+', '-')) and not l.startswith(('+++', '---')):
            yield l

# ---------------------------------------------------------------------------
#  sanitize
# ---------------------------------------------------------------------------

SANITIZERS = '-fsanitize=address,undefined -fno-omit-frame-pointer'

def sanitizer_reports(logs, tree):
    """The distinct reports in the sanitizer logs: {first line: (count, a log that has it)}."""
    found = {}
    for f in sorted(os.listdir(logs)):
        for line in open(os.path.join(logs, f), errors='replace'):
            m = re.search(r'(runtime error|ERROR: \w+Sanitizer): ', line)
            if m:
                # pids, addresses and the values in UBSan's messages change from run to run
                key = line[:m.end()].split('==')[-1].replace(tree + '/', '') + re.sub(r'0x[0-9a-f]+|\d+', '?', line[m.end():].strip())
                count, example = found.get(key, (0, os.path.join(logs, f)))
                found[key] = (count + 1, example)
    return found

def sanitize(src, out):
    """Build the openvcl tree src twice, Release and Debug with ASan and UBSan, and run its tests on
    the sanitized build. Then compile every input with both binaries: the outputs must match, and
    the sanitizers must report nothing."""
    shutil.rmtree(out, ignore_errors=True)
    tree, bins, logs = (os.path.join(out, d) for d in ('src', 'bin', 'logs'))
    # The build writes openvcl to the source root and the integration tests run it from there,
    # so build in a copy: the tracked and untracked files, uncommitted changes included.
    listed = sh(['git', 'ls-files', '-z', '--cached', '--others', '--exclude-standard'], cwd=src)
    if listed.returncode != 0:
        print(f'{src} is not a git checkout:\n{listed.stderr}')
        return 2
    for f in listed.stdout.split('\0'):
        if f and os.path.isfile(os.path.join(src, f)):
            os.makedirs(os.path.dirname(os.path.join(tree, f)), exist_ok=True)
            shutil.copy2(os.path.join(src, f), os.path.join(tree, f))
    os.makedirs(bins)
    os.makedirs(logs)
    jobs = str(os.cpu_count() or 4)
    for name, config in (('release', ['-DCMAKE_BUILD_TYPE=Release', '-DBUILD_TESTING=OFF']),
                         ('asan', ['-DCMAKE_BUILD_TYPE=Debug', '-DCMAKE_CXX_FLAGS=' + SANITIZERS])):
        build = os.path.join(out, 'build-' + name)
        for cmd in (['cmake', '-S', tree, '-B', build, '-DBUILD_EXAMPLES=OFF'] + config,
                    ['cmake', '--build', build, '--parallel', jobs]):
            r = sh(cmd)
            if r.returncode != 0:
                print(f'{" ".join(cmd)} failed:\n{r.stdout[-4000:]}{r.stderr[-4000:]}')
                return 1
        shutil.copy2(os.path.join(tree, 'openvcl'), os.path.join(bins, 'openvcl-' + name))

    # Reports go to files: the integration tests keep openvcl's stderr to themselves, and UBSan
    # carries on after a report. LeakSanitizer (Linux only) would just list what is live at exit.
    os.environ['ASAN_OPTIONS'] = f'log_path={logs}/asan:abort_on_error=0' + ('' if sys.platform == 'darwin' else ':detect_leaks=0')
    os.environ['UBSAN_OPTIONS'] = f'log_path={logs}/ubsan:print_stacktrace=1'
    tests = sh(['ctest', '--test-dir', os.path.join(out, 'build-asan'), '--verbose'])
    totals = [l.split(': ', 1)[-1] for l in tests.stdout.split('\n') if '[==========]' in l]
    if tests.returncode != 0:
        print('\n'.join(tests.stdout.strip().split('\n')[-60:]))
    print('tests:', totals[-1] if totals else 'no result')

    summary, rows = run(os.path.join(bins, 'openvcl-release'), os.path.join(bins, 'openvcl-asan'),
                        os.path.join(out, 'corpus'), checker=False)
    differ = [i for i, status, notes in rows if status in ('CHANGED', 'NEW', 'BROKEN') or '-c output differs' in notes]

    found = sanitizer_reports(logs, tree)
    for key, (count, example) in sorted(found.items()):
        print(f'{count:5d}x {key}\n        in {example}')
    print(f"sanitize: tests {'passed' if tests.returncode == 0 else 'FAILED'}; "
          f"{len(differ)} program(s) compile differently from the release build; "
          f"{len(found)} distinct sanitizer report(s)")
    return 0 if tests.returncode == 0 and not differ and not found else 1

if __name__ == '__main__':
    args = sys.argv[1:]
    if args == ['prepare']:
        prepare()
    elif len(args) == 4 and args[0] == 'run':
        summary, _ = run(*(os.path.abspath(a) for a in args[1:]))
        sys.exit(1 if summary['broken'] else 0)
    elif len(args) == 3 and args[0] == 'sanitize':
        sys.exit(sanitize(*(os.path.abspath(a) for a in args[1:])))
    else:
        print(__doc__)
        sys.exit(2)
