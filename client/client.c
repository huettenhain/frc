#include <windows.h>
#include "../plugin/globals.h"

void main() {
  int argc;
  WCHAR **argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  if (argc == 2) {
    HANDLE slot = CreateFileW(
      FRC_MSLOT,
      GENERIC_WRITE,
      FILE_SHARE_READ,
      NULL,
      OPEN_EXISTING,
      FILE_ATTRIBUTE_NORMAL,
      NULL);
    if (slot != INVALID_HANDLE_VALUE) {
      DWORD written;
      WriteFile(slot, argv[1], (1+lstrlenW(argv[1]))*sizeof(**argv), 
        &written, NULL);
      CloseHandle(slot);
      ExitProcess(0);
    }
  }
  ExitProcess(1);
}