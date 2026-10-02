#!/usr/bin/env python3
"""Build and run portable tests. Requires Python 3 and g++ on PATH."""
import os, pathlib, subprocess, sys
root = pathlib.Path(__file__).resolve().parents[1]
os.chdir(root)
build = root / 'build' / 'host'
build.mkdir(parents=True, exist_ok=True)
sources = sorted(pathlib.Path('src/core').glob('*.cpp')) + sorted(pathlib.Path('src/ui').glob('*.cpp'))
sources += [pathlib.Path(x) for x in ['src/touch_sample.cpp', 'src/platform/desktop_file_storage.cpp', 'src/platform/esp32/esp32_clock.cpp', 'src/platform/esp32/littlefs_byte_storage.cpp', 'src/platform/esp32/power_hardware.cpp', 'src/platform/esp32/touch_hardware.cpp']]
objects=[]
for source in sources:
    obj=build / (str(source).replace('/', '_') + '.o')
    subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Isrc','-c',str(source),'-o',str(obj)], check=True)
    objects.append(str(obj))
tests=sorted(pathlib.Path('tests').glob('test_*.cpp'))
if len(sys.argv)>1:
    tests=[t for t in tests if any(x in t.stem for x in sys.argv[1:])]
for test in tests:
    exe=build / (test.stem + ('.exe' if os.name=='nt' else ''))
    subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Isrc',str(test),*objects,'-o',str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print(f'PASS: {len(tests)} test executables')
