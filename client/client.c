#include <windows.h>
#include <Shlwapi.h>

#include "../plugin/globals.h"

BYTE ParseArgument(WCHAR* cmd) {
  if (!StrCmpIW(cmd, L"goto")) return FRC_GOTO;
  if (!StrCmpIW(cmd, L"copy")) return FRC_COPY;
  if (!StrCmpIW(cmd, L"quit")) return FRC_QUIT;
  return FRC_NONE;
}

__declspec(noreturn) void ExitNoSlot() {
  MessageBoxA(NULL,
    "Unable to open Mailslot handle.",
    "FRC Client Error", MB_OK | MB_ICONERROR);
  ExitProcess(1);
}

__declspec(noreturn) void ExitUsage() {
  MessageBoxA(NULL,
    "client [goto | copy | quit] [argument]\n\n"
    "  goto - navigate to file or folder given as argument\n"
    "  copy - insert the argument into the FAR command line\n"
    "  quit - request frc server termination\n",
    "FRC Client Usage", MB_OK | MB_ICONWARNING);
  ExitProcess(1);
}

__declspec(noreturn) void main() {
  int argc;
  WCHAR **argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  if (argc >= 2) {
    BYTE type = ParseArgument(argv[1]);
    if (type != FRC_NONE) {
      HANDLE slot = CreateFileW(
        FRC_MSLOT,
        GENERIC_WRITE,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
      if (slot && slot != INVALID_HANDLE_VALUE) {
        WCHAR cmdMask = ((WCHAR)type) << 8;
        WCHAR *cmd = &cmdMask;
        DWORD size = 2;
        if (argc >= 3) {
          cmd = argv[2];
          size = (lstrlenW(cmd)+1) * sizeof(WCHAR);
          cmd[0] = cmdMask | LOBYTE(cmd[0]);
        }
        WriteFile(slot, cmd, size, &size, NULL);
        CloseHandle(slot);
        ExitProcess(0);
      } else {
        ExitNoSlot();
      }
    }
  }
  ExitUsage();
}