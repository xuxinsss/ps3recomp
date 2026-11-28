"""Windows regression for empty slots after a successful preview read."""
import os, subprocess, tempfile
from pathlib import Path
from msvc_env import environ
root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='savedata-missing-') as tmp:
    directory = Path(tmp)
    binary = directory/'test.exe'
    env = environ()
    env.pop('PS3_GAMEDATA_ROOT', None)
    subprocess.run([os.environ.get('CC', 'clang-cl'), '/nologo', '/W0', '/UNDEBUG',
        '/D_CRT_SECURE_NO_WARNINGS', '/I'+str(root/'include'),
        str(root/'libs/system/tests/test_savedata_autoload_missing.c'),
        '/Fe:'+str(binary)], cwd=directory, env=env, check=True)
    saves = directory/'saves'
    saves.mkdir()
    subprocess.run([str(binary), str(saves)], cwd=directory, env=env, check=True, timeout=30)
