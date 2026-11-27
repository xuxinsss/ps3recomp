"""Build the optional Windows Cg/DXBC cache preparation utility."""
import os, subprocess, sys
from pathlib import Path
from msvc_env import environ

root = Path(__file__).resolve().parents[1]
output = Path(sys.argv[1]).resolve()
output.mkdir(parents=True, exist_ok=True)
subprocess.run([
    os.environ.get('CC', 'clang-cl'), '/nologo', '/W3', '/O2',
    '/D_CRT_SECURE_NO_WARNINGS', '/I' + str(root/'include'),
    '/I' + str(root/'libs/video'),
    str(root/'tools/d3d12_precompile_cg.c'),
    str(root/'libs/video/rsx_vp_decompiler.c'),
    str(root/'libs/video/rsx_fp_decompiler.c'),
    '/Fe:' + str(output/'d3d12-precompile-cg.exe')
], cwd=output, env=environ(), check=True)
