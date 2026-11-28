"""Selection and byte layout for opt-in D3D12 CPU surface readback (Windows)."""
import os, subprocess, tempfile
from pathlib import Path
from msvc_env import environ
root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="d3d12-readback-") as tmp:
    directory = Path(tmp)
    binary = directory / "test.exe"
    env = environ()
    subprocess.run([os.environ.get("CC", "clang-cl"), "/nologo", "/W4", "/UNDEBUG",
        "/D_CRT_SECURE_NO_WARNINGS", str(root / "libs/video/tests/test_d3d12_cpu_readback.c"),
        "/Fe:" + str(binary)], cwd=directory, env=env, check=True)
    subprocess.run([str(binary)], cwd=directory, env=env, check=True, timeout=30)
