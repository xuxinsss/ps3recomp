"""CPU-only point-sprite text and production glslang/SPIR-V regressions."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="rsx-point-sprite-") as directory:
    d=Path(directory)
    (d/"CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.20)
project(point_sprite_tests LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
add_compile_options(-UNDEBUG)
find_package(Threads REQUIRED)
find_package(glslang CONFIG REQUIRED)
add_executable(point_sprite "{root}/libs/video/tests/test_rsx_point_sprite.c")
add_executable(point_spirv "{root}/libs/video/tests/test_point_sprite_spirv.c" "{root}/libs/video/rsx_shader_spirv.cpp")
target_compile_definitions(point_spirv PRIVATE PS3RECOMP_HAVE_SPIRV_TRANSLATION=1)
target_include_directories(point_spirv PRIVATE "{root}/include" "{root}/libs/video")
target_link_libraries(point_spirv PRIVATE glslang::glslang glslang::SPIRV glslang::glslang-default-resource-limits)
add_executable(shader_cache "{root}/libs/video/tests/test_shader_spirv.c" "{root}/libs/video/rsx_shader_spirv.cpp")
target_compile_definitions(shader_cache PRIVATE PS3RECOMP_HAVE_SPIRV_TRANSLATION=1)
target_include_directories(shader_cache PRIVATE "{root}/include" "{root}/libs/video")
target_link_libraries(shader_cache PRIVATE glslang::glslang glslang::SPIRV glslang::glslang-default-resource-limits)
''')
    subprocess.run(["cmake","-S",str(d),"-B",str(d/"build"),"-DCMAKE_BUILD_TYPE=Release"],check=True)
    subprocess.run(["cmake","--build",str(d/"build"),"--parallel","2"],check=True)
    for name in ["point_sprite","point_spirv","shader_cache"]:
        subprocess.run([str(d/"build"/name)],cwd=d,check=True,timeout=60)
    validator=shutil.which("spirv-val")
    if validator:
        for p in sorted(d.glob("*.spv")):
            subprocess.run([validator,"--target-env","vulkan1.0",str(p)],check=True)
        print("PASS: generated SPIR-V validates for Vulkan 1.0")
    else:
        print("SKIP: spirv-val unavailable; translation assertions passed")
