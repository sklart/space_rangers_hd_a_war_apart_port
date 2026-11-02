"""Capture MSVC environment in memory; never log unrelated environment values."""
import os
import subprocess
from pathlib import Path
VS=Path('C:/Program Files/Microsoft Visual Studio/2022/Enterprise')
CMAKE=VS/'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
NINJA=VS/'Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe'
def environment(directory):
 directory.mkdir(parents=True,exist_ok=True);script=directory/'capture-msvc-env.cmd'
 content='@echo off\ncall "'+str(VS/'VC/Auxiliary/Build/vcvars64.bat')+'" >nul\nif errorlevel 1 exit /b %errorlevel%\nset\n'
 if not script.exists():script.write_text(content,encoding='utf-8')
 else:assert script.read_text(encoding='utf-8')==content
 result=subprocess.run(['cmd.exe','/d','/c',str(script)],capture_output=True,text=True,errors='replace');assert result.returncode==0,'MSVC environment setup failed'
 env={key.upper():value for key,value in os.environ.items()}
 for line in result.stdout.splitlines():
  if '=' in line and not line.startswith('='):
   key,value=line.split('=',1);env[key.upper()]=value
 return env
def compiler(env):return VS/'VC/Tools/MSVC'/env['VCTOOLSVERSION'].strip('\\/')/'bin/Hostx64/x64/cl.exe'
