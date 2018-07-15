#include <windows.h>
#include <Shlwapi.h>

#include "../plugin/globals.h"

BYTE ParseArgument(WCHAR* cmd) {
  if (!StrCmpIW(cmd, L"goto")) return FRC_GOTO;
  if (!StrCmpIW(cmd, L"copy")) return FRC_COPY;
  if (!StrCmpIW(cmd, L"qcpy")) return FRC_QCPY;
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
    "The FRC client executable supports the following commands as its first parameter, "
    "followed by an optional argument in some cases. All commands are case-insensitive.\n\n"
    "GOTO\nNavigate to the file or folder given as the second argument.\n\n"
    "COPY\nInsert the second argument as a string into the FAR command line\n\n"
    "QCPY\nInsert the second argument into the FAR command line, but wrap it in double "
    "quotes if it contains a space character (used to insert path names)\n\n"
    "QUIT\nIssue a request for FRC server termination. The second argument is ignored.",
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