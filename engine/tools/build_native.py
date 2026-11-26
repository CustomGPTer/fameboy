#!/usr/bin/env python3
from pathlib import Path
import argparse,os,subprocess,shutil
p=argparse.ArgumentParser();p.add_argument('--ndk',default=os.environ.get('ANDROID_NDK_ROOT'));p.add_argument('--jobs',default='4');a=p.parse_args()
if not a.ndk:raise SystemExit('Pass --ndk or set ANDROID_NDK_ROOT')
root=Path(__file__).resolve().parents[1]
for abi in ('arm64-v8a','armeabi-v7a','x86_64'):
 build=root/'build'/abi
 subprocess.run(['cmake','-S',str(root),'-B',str(build),'-G','Ninja','-DCMAKE_TOOLCHAIN_FILE='+str(Path(a.ndk)/'build/cmake/android.toolchain.cmake'),'-DANDROID_ABI='+abi,'-DANDROID_PLATFORM=android-29','-DCMAKE_POLICY_VERSION_MINIMUM=3.5'],check=True)
 subprocess.run(['cmake','--build',str(build),'-j',a.jobs],check=True)
 dest=root/'prebuilt'/abi;dest.mkdir(parents=True,exist_ok=True);shutil.copyfile(build/'libfameboy.so',dest/'libfameboy.so')
