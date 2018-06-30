@echo off
for %%d in (Debug,Release,x64) do (
 2>NUL rmdir /q /s .\plugin\%%d
 2>NUL rmdir /q /s .\client\%%d
 2>NUL rmdir /q /s .\%%d
)
2>NUL del .\plugin\*.aps
2>NUL del .\client\*.aps