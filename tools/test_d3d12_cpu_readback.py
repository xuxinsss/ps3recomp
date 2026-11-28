"""Portable selection and byte-layout regression; D3D12 execution is separate."""
import os, subprocess, tempfile
from pathlib import Path
from msvc_env import environ
root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="d3d12-readback-") as tmp:
    directory = Path(tmp)
    binary = directory / ("test.exe" if os.name == "nt" else "test")
    source = root / "libs/video/tests/test_d3d12_cpu_readback.c"
    if os.name == "nt":
        command = [os.environ.get("CC", "clang-cl"), "/nologo", "/W4", "/UNDEBUG",
            "/D_CRT_SECURE_NO_WARNINGS", str(source), "/Fe:" + str(binary)]
    else:
        command = [os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra",
            "-O1", "-UNDEBUG", str(source), "-o", str(binary)]
    env = environ()
    subprocess.run(command, cwd=directory, env=env, check=True)
    subprocess.run([str(binary)], cwd=directory, env=env, check=True, timeout=30)
