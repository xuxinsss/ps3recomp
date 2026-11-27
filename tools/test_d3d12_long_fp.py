"""Long-shader regression, including the offline Cg cache preparation path."""
import os,struct,subprocess,tempfile
from pathlib import Path
from msvc_env import environ
root=Path(__file__).resolve().parents[1]
env=environ()
with tempfile.TemporaryDirectory(prefix='d3d12-long-fp-') as tmp:
    d=Path(tmp)
    common=[os.environ.get('CC','clang-cl'),'/nologo','/W0','/UNDEBUG','/D_CRT_SECURE_NO_WARNINGS',
            '/I'+str(root/'include'),'/I'+str(root/'libs/video')]
    decoder=str(root/'libs/video/rsx_fp_decompiler.c')
    subprocess.run(common+[str(root/'libs/video/tests/test_d3d12_long_fp.c'),decoder,'/Fe:'+str(d/'test.exe')],cwd=d,env=env,check=True)
    subprocess.run([str(d/'test.exe'),str(d/'long.bin')],cwd=d,check=True,timeout=30)
    subprocess.run(common+[str(root/'tools/d3d12_precompile_cg.c'),decoder,str(root/'libs/video/rsx_vp_decompiler.c'),'/Fe:'+str(d/'precompile.exe')],cwd=d,env=env,check=True)
    code=(d/'long.bin').read_bytes()
    meta=struct.pack('>8I',7004,6,len(code)+64,0,32,32,len(code),64)+bytes(32)
    fixture=d/'long.cg';fixture.write_bytes(meta+code)
    cache=d/'cache'
    subprocess.run([str(d/'precompile.exe'),str(fixture),str(cache)],cwd=d,check=True,timeout=30)
    entries=list(cache.glob('*.bin'));assert len(entries)==1
    data=entries[0].read_bytes()
    assert b'rsx_tex[9].Sample' in data and b'fp_constants[0]' in data
    subprocess.run([str(d/'precompile.exe'),str(fixture),str(cache)],cwd=d,check=True,timeout=30)
    assert entries[0].read_bytes()==data
    print('PASS precompile includes the tail beyond 4 KiB and reuses the exact-source cache')
