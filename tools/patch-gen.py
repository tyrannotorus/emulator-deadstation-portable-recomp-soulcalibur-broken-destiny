#!/usr/bin/env python3
# The module's changes to the translated game: prepare-game.sh's C (SRC) copied to DST with each change applied. A change names
# the translated instruction it replaces by its exact text; one that doesn't match exactly once stops the build. Files are
# rewritten only when they differ.
#   python3 -I tools/patch-gen.py WORK/gen build-brick/gen
import os, shutil, sys
src, dst = sys.argv[1], sys.argv[2]
DECL = 'uint32_t sc_frame_interval(uint32_t wanted);   /* src/module.c */\nuint32_t sc_fighter_control(uint32_t records, uint32_t cpu);\nuint32_t sc_random_seed(uint32_t seed);\nuint32_t sc_options_rows(uint32_t rows);\n'
CHANGES = [
    # logic at 60: the frame interval the game sets at boot (src/module.c sc_frame_interval)
    ('logic at 60',
     'MEM_W16_PC(s->r[16] + 0x000006c8u, s->r[17], 0x08a55174u)',
     'MEM_W16_PC(s->r[16] + 0x000006c8u, sc_frame_interval(s->r[17]), 0x08a55174u)'),
    # who controls fighter 1 (sc_fighter_control: the benchmark's CPU vs CPU)
    ('fighter control',
     'MEM_W32_PC(s->r[5] + 0x0000001cu, s->r[3], 0x089de4ccu)',
     'MEM_W32_PC(s->r[5] + 0x0000001cu, sc_fighter_control(s->r[5], s->r[3]), 0x089de4ccu)'),
    # OPTIONS without INSTALL: the row count of the list's two constructors (sc_options_rows)
    ('options without install',
     'MEM_W32_PC(s->r[18] + 0x000000c4u, s->r[2], 0x08958f14u)',
     'MEM_W32_PC(s->r[18] + 0x000000c4u, sc_options_rows(s->r[2]), 0x08958f14u)'),
    ('options without install (2)',
     'MEM_W32_PC(s->r[18] + 0x000000c4u, s->r[2], 0x08959060u)',
     'MEM_W32_PC(s->r[18] + 0x000000c4u, sc_options_rows(s->r[2]), 0x08959060u)'),
    # the seed of the game's random numbers (sc_random_seed: the benchmark's fixed seed)
    ('random seed',
     'MEM_W32_PC(s->r[4] + 0x00000004u, s->r[5], 0x08b11210u)',
     'MEM_W32_PC(s->r[4] + 0x00000004u, sc_random_seed(s->r[5]), 0x08b11210u)'),
]
os.makedirs(dst, exist_ok=True)
texts = {}
for name in sorted(os.listdir(src)):
    path = os.path.join(src, name)
    if name.endswith('.c'):
        texts[name] = open(path).read()
    elif os.path.isfile(path):
        out = os.path.join(dst, name)
        if not os.path.exists(out) or open(path, 'rb').read() != open(out, 'rb').read():
            shutil.copyfile(path, out)
for what, old, new in CHANGES:
    hits = [n for n, t in texts.items() if old in t]
    count = sum(texts[n].count(old) for n in hits)
    if count != 1:
        sys.exit(f'patch-gen: "{what}": the instruction is found {count} times, not once: {old}')
    n = hits[0]
    texts[n] = texts[n].replace(old, new)
    if DECL not in texts[n]:
        i = texts[n].index('\n', texts[n].index('#include')) + 1
        texts[n] = texts[n][:i] + DECL + texts[n][i:]
    print(f'patch-gen: {what}: {n}')
# the SDK's register promotion (sdk/tools/promote_registers.py): each translated function's guest registers in C locals
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'sdk', 'tools'))
import promote_registers
import multiprocessing
chunks = sorted(n for n in texts if n.startswith('game_') and n[5:-2].isdigit())
if os.environ.get('DSP_NO_PROMOTE') == '1':   # the code as generated (a check)
    chunks = []
with multiprocessing.get_context('fork').Pool() as pool:   # fork: the workers don't run this script again
    results = pool.map(promote_registers.promote_job, [texts[n] for n in chunks])
total, reasons = {}, {}
for n, (text, stats, why) in zip(chunks, results):
    texts[n] = text
    for k, v in stats.items():
        total[k] = total.get(k, 0) + v
    for k, v in why.items():
        reasons[k] = reasons.get(k, 0) + v
if chunks: print(f"promote: {total['promoted']} of {total['functions']} functions; {total['syncs']} calls, {total['yields']} yields, "
      f"{total['returns']} returns; kept as they were: " + (", ".join(f"{k} {v}" for k, v in sorted(reasons.items(), key=lambda kv: -kv[1])) or "none"))
for n, t in texts.items():
    out = os.path.join(dst, n)
    if not os.path.exists(out) or open(out).read() != t:
        open(out, 'w').write(t)
