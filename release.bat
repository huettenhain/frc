@echo off
setlocal enabledelayedexpansion

set FRC7Z=%1

if "%FRC7Z%" == "" (
  for /f %%c in ('git tag -l') do set FRC7Z=%%c
)

echo releasing as !FRC7Z!

set FRC7Z=frc-!FRC7Z:V=!

2>NUL del   !FRC7Z!.x64.7z
2>NUL del   !FRC7Z!.x86.7z

1>NUL 7za a !FRC7Z!.x64.7z ./x64/Release/client.exe ./x64/Release/plugin.dll 
1>NUL 7za a !FRC7Z!.x86.7z     ./Release/client.exe     ./Release/plugin.dll 