#!/usr/bin/env python3
"""Build and run the legacy D3D12 command bridge regression on Windows."""
import os, subprocess, tempfile
from pathlib import Path
from msvc_env import environ
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='legacy-d3d12-') as tmp:
    binary=Path(tmp)/'test.exe'
    sources=['libs/video/tests/test_d3d12_legacy_commands.c',
             'libs/video/rsx_commands.c','libs/video/rsx_vertex_fetch.c']
    command=[os.environ.get('CC','clang-cl'),'/nologo','/W0','/UNDEBUG',
             '/D_CRT_SECURE_NO_WARNINGS','/I'+str(root/'include'),
             '/I'+str(root/'libs/video'),*[str(root/s) for s in sources],
             '/Fe:'+str(binary)]
    subprocess.run(command,cwd=tmp,env=environ(),check=True)
    subprocess.run([str(binary)],cwd=tmp,check=True,timeout=15)
