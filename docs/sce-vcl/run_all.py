"""run_all.py OUTDIR PATTERN [vcl args...]: SCE vcl on the corpus inputs matching PATTERN, 8 at a time.

VCL_TIME_SCALE is passed through to run_vcl.py. SCEVCL_HOME is where setup.sh
put the venv and the rootfs; CORPUS_INPUTS is the corpus's inputs/ folder."""
import concurrent.futures, fnmatch, glob, os, subprocess, sys, time
S = os.path.expanduser(os.environ.get('SCEVCL_HOME', '~/.cache/openvcl-scevcl'))
TOOLS = os.path.dirname(os.path.abspath(__file__))
INPUTS = os.path.expanduser(os.environ.get('CORPUS_INPUTS', '~/Repos/GitHub/openvcl-corpus/inputs'))
out, pattern = sys.argv[1], sys.argv[2]; extra = sys.argv[3:]
os.makedirs(out, exist_ok=True)
inputs = sorted(f for f in glob.glob(os.path.join(INPUTS, '*.vcl'))
                if fnmatch.fnmatch(os.path.basename(f)[:-4], pattern))
def one(src):
    n = os.path.basename(src)[:-4]
    r = subprocess.run([os.path.join(S, 'venv/bin/python'), os.path.join(TOOLS, 'run_vcl.py'), os.path.join(S, 'rootfs'),
                        src, os.path.join(out, n + '.vsm'), os.path.join(out, n + '.err')] + extra,
                       capture_output=True, text=True)
    err = '\n'.join(l for l in r.stderr.splitlines() if 'deprecated' not in l)
    open(os.path.join(out, n + '.status'), 'w').write(r.stdout + err[-2000:])
    return n, r.stdout.strip() or ('PYERR ' + err.strip().splitlines()[-1] if err.strip() else '?')
start = time.time()
with concurrent.futures.ThreadPoolExecutor(int(os.environ.get('JOBS', '8'))) as pool:
    for n, line in pool.map(one, inputs):
        print(line, flush=True)
print('total %.1f s' % (time.time() - start))
