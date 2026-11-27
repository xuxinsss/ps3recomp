#!/usr/bin/env python3
"""Run Windows legacy command, GPU point-sprite, cache and vertex regressions."""
import os, subprocess, tempfile
from pathlib import Path
from msvc_env import environ
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='d3d12-sampler-') as tmp:
    binary=Path(tmp)/'test.exe'
    subprocess.run([os.environ.get('CC','clang-cl'),'/nologo','/W0','/UNDEBUG',
                    '/I'+str(root/'libs/video'),str(root/'libs/video/tests/test_d3d12_sampler.c'),
                    '/Fe:'+str(binary)],cwd=tmp,env=environ(),check=True)
    subprocess.run([str(binary)],cwd=tmp,check=True,timeout=15)

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

# Use the real Windows HLSL compiler for the point-expanded vertex shader.
with tempfile.TemporaryDirectory(prefix='d3d12-point-quad-') as tmp:
    binary=Path(tmp)/'test.exe'
    command=[os.environ.get('CC','clang-cl'),'/nologo','/W0','/UNDEBUG',
             '/D_CRT_SECURE_NO_WARNINGS','/I'+str(root/'libs/video'),
             str(root/'libs/video/tests/test_d3d12_point_quad.c'),'/Fe:'+str(binary)]
    subprocess.run(command,cwd=tmp,env=environ(),check=True)
    subprocess.run([str(binary)],cwd=tmp,check=True,timeout=15)

# Render/read back pixels with D3D12 WARP across nonzero draw offsets.
with tempfile.TemporaryDirectory(prefix='d3d12-point-gpu-') as tmp:
    binary=Path(tmp)/'test.exe'
    command=[os.environ.get('CC','clang-cl'),'/nologo','/W0','/UNDEBUG',
             '/EHsc','/I'+str(root/'libs/video'),
             str(root/'libs/video/tests/test_d3d12_point_quad_gpu.cpp'),'/Fe:'+str(binary)]
    subprocess.run(command,cwd=tmp,env=environ(),check=True)
    subprocess.run([str(binary)],cwd=tmp,check=True,timeout=30)

with tempfile.TemporaryDirectory(prefix='d3d12-shader-cache-') as tmp:
    binary=Path(tmp)/'test.exe'
    subprocess.run([os.environ.get('CC','clang-cl'),'/nologo','/W0','/UNDEBUG',
                    '/I'+str(root/'libs/video'),str(root/'libs/video/tests/test_d3d12_shader_cache.c'),
                    '/Fe:'+str(binary)],cwd=tmp,env=environ(),check=True)
    subprocess.run([str(binary)],cwd=tmp,check=True,timeout=30)

with tempfile.TemporaryDirectory(prefix='vertex-float-plan-') as tmp:
    binary=Path(tmp)/'test.exe'
    subprocess.run([os.environ.get('CC','clang-cl'),'/nologo','/W0','/UNDEBUG',
                    '/I'+str(root/'include'),'/I'+str(root/'libs/video'),
                    str(root/'libs/video/tests/test_vertex_float_plan.c'),str(root/'libs/video/rsx_vertex_fetch.c'),
                    '/Fe:'+str(binary)],cwd=tmp,env=environ(),check=True)
    subprocess.run([str(binary)],cwd=tmp,check=True,timeout=30)
