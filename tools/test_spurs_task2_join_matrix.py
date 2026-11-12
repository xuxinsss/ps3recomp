#!/usr/bin/env python3
"""Characterize Task2 Join/ID reuse; results do not establish firmware semantics."""
import os, platform, subprocess, tempfile, json
from pathlib import Path
if os.name == 'nt':
    print('SKIP: POSIX fixture'); raise SystemExit(0)
r=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='task2-hle-') as tmp:
    out=Path(tmp)/'test'
    units=['spu_channels','spu_drain','spu_lockstep','spu_coherency','spu_workload',
           'spurs_policy','spurs_job','spu_tsp_weak','spu_vm_pagemap',
           'spurs_policy_blob_weak','spu_interp']
    subprocess.run([os.environ.get('CC','cc'),'-std=gnu17','-O1','-UNDEBUG',
        '-ffunction-sections','-fdata-sections',
        '-DSPU_TASKSET_TAIL_OBSERVER=fixture_task2_tail',
        '-DSPU_TASKSET_LS_ALLOC=fixture_task2_ls_alloc',
        '-DSPU_TASKSET_LS_FREE=fixture_task2_ls_free',
        '-DSPU_TASKSET_THREAD_CREATE=fixture_task2_thread_create',
        '-Wl,-dead_strip' if platform.system()=='Darwin' else '-Wl,--gc-sections',
        '-I'+str(r/'include'),'-I'+str(r/'runtime/spu'),'-I'+str(r/'runtime/platform'),
        str(r/'tests/test_spurs_task2_join_matrix.c'),
        *[str(r/'runtime/spu'/f'{u}.c') for u in units],
        str(r/'libs/spurs/spurs_taskset.c'),
        str(r/'runtime/syscalls/sys_event.c'),
        str(r/'runtime/platform/win32_compat.c'),'-pthread','-lm','-o',str(out)],check=True)
    env=dict(os.environ,LBP_PERSIST_LS='1',TASK2_JOIN_RESULTS=str(Path(tmp)/'observations.jsonl'))
    subprocess.run([str(out)],check=True,timeout=45,env=env)

    rows=[json.loads(line) for line in (Path(tmp)/'observations.jsonl').read_text().splitlines()]
    assert len(rows)==5 and all(row['oracle']=='host-characterization' for row in rows)
    for row in rows: print('TASK2_OBSERVATION '+json.dumps(row))
    if os.environ.get('TASK2_JOIN_EXPORT'):
        Path(os.environ['TASK2_JOIN_EXPORT']).write_text(json.dumps(rows,indent=2)+'\n')
