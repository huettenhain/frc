#include <Windows.h>
#include <Shlwapi.h>

#include "plugin.hpp"
#include "globals.h"

#define TIMEOUT  500
#define PATIENCE 300

static struct PluginStartupInfo api = { 0 };

HANDLE stopEvent = NULL;
HANDLE doneEvent = NULL;
HANDLE frcThread = NULL;
HANDLE mailbox = NULL;

typedef struct {
  FRC_COMMAND_TYPE type;
  WCHAR *arg;
} FRC_COMMAND;

VOID NullHandle(HANDLE *h) {
  if (*h && *h != INVALID_HANDLE_VALUE)
    CloseHandle(*h);
  *h = NULL;
}

intptr_t FrcMessage(LPCWSTR *messages, size_t count, FARMESSAGEFLAGS flags) {
  return api.Message(
    &FRC_GUID_PLUG,
    &FRC_GUID_FMSG,
    flags, NULL,
    messages, count+1, 0);
}

DWORD WINAPI Receiver(PVOID reserved) {
  HANDLE heap = GetProcessHeap();
  DWORD size = MAX_PATH, bytesRead = 0;
  BYTE* data = HeapAlloc(heap, HEAP_ZERO_MEMORY, size);
  FRC_COMMAND cmd = {0};
  ResetEvent(doneEvent);
  if (data) {
    OVERLAPPED ov;
    ov.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    while (WaitForSingleObject(stopEvent, 0) != WAIT_OBJECT_0) {
      ov.Offset = ov.OffsetHigh = 0;
      ResetEvent(ov.hEvent);
      BOOL success = ReadFile(mailbox, data, size, &bytesRead, &ov);
      if (!success && GetLastError() == ERROR_IO_PENDING)
        success = GetOverlappedResult(mailbox, &ov, &bytesRead, TRUE);
      if (!success) {
        DWORD err = GetLastError();
        if (err == ERROR_SEM_TIMEOUT)
          continue;
        if (err == ERROR_INSUFFICIENT_BUFFER) {
          DWORD required = 0, sizeNew = size;
          PVOID dataNew;
          success = GetMailslotInfo(mailbox, NULL, &required, NULL, NULL);
          if (success && required != MAILSLOT_NO_MESSAGE && required > size) {
            while (sizeNew < required)
              sizeNew *= 2;
            dataNew = HeapReAlloc(heap, HEAP_ZERO_MEMORY, data, sizeNew);
            if (dataNew != NULL) {
              data = dataNew;
              size = sizeNew;
              continue;
            }
          }
        }
        break;
      } else if (bytesRead > 1) {
        if (data[1] == FRC_QUIT) 
          break;
        *(WCHAR*)(data + bytesRead - sizeof(WCHAR)) = L'\0';
        if (IS_VALID_CMD(data[1])) {
          cmd.type = data[1];
          cmd.arg = (WCHAR*) data;
          data[1] = 0;
          api.AdvControl(&FRC_GUID_PLUG, ACTL_SYNCHRO, 0, &cmd);
        }
      }
    }
    CloseHandle(ov.hEvent);
    HeapFree(heap, 0, data);
  }
  NullHandle(&mailbox);
  SetEvent(doneEvent);
  return 0;
}


VOID ReceiverTerminate() {
  DWORD retVal = 0;
  if (frcThread) {
    if ((!GetExitCodeThread(frcThread, &retVal) || retVal == STILL_ACTIVE) &&
        (WaitForSingleObject(frcThread, PATIENCE) != WAIT_OBJECT_0) &&
        (!TerminateThread(frcThread, 1)))
      RaiseException(0, EXCEPTION_NONCONTINUABLE, 0, NULL);
  }
  NullHandle(&frcThread);
  NullHandle(&mailbox);
}

BOOL isReceiverStarted() {
  if (WaitForSingleObject(doneEvent, 0) == WAIT_OBJECT_0) {
    ReceiverTerminate();
    return FALSE;
  } else
    return TRUE;
}

BOOL TerminateRemoteFRC() {
  BOOL success = FALSE;
  HANDLE slot = CreateFileW(FRC_MSLOT, GENERIC_WRITE, 
    FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (slot != INVALID_HANDLE_VALUE) {
    DWORD written;
    WCHAR stop = (WCHAR)(FRC_QUIT<<8);
    WriteFile(slot, (LPCVOID) &stop, sizeof(stop), &written, NULL);
    CloseHandle(slot);
    #define SLEEP_STEP 10
    for (DWORD slept = 0; slept < TIMEOUT + PATIENCE; 
      Sleep(SLEEP_STEP), slept += SLEEP_STEP)
    # undef SLEEP_STEP
    {
      mailbox = CreateMailslotW(FRC_MSLOT, 0, TIMEOUT, NULL);
      if (mailbox != INVALID_HANDLE_VALUE)
        return TRUE;
    }
    mailbox = NULL;
  }
  return FALSE;
}


BOOL ReceiverStart(BOOL quietSuccess, BOOL quietFailure, BOOL terminateOther) {
  BOOL didKill = FALSE;
  if (!isReceiverStarted()) {
    mailbox = CreateMailslotW(FRC_MSLOT, 0, TIMEOUT, NULL);
    if (mailbox == INVALID_HANDLE_VALUE) {
      mailbox = NULL;
      if (!terminateOther && !quietFailure) {
        LPCWSTR msg[] = { FRC_TITLE, 
          L"Unable to create mailslot, could not start FRC.",
          L"Try to shutdown the other FRC instance blocking the slot?"
        };
        terminateOther = !FrcMessage(msg, 2, FMSG_WARNING | FMSG_MB_YESNO);
      }
      if (!terminateOther || !(didKill = TerminateRemoteFRC()))
        return FALSE;
    }
    ResetEvent(doneEvent);
    ResetEvent(stopEvent);
    frcThread = CreateThread(NULL, 0, 
      (LPTHREAD_START_ROUTINE) &Receiver, NULL, 0, NULL);
    if (frcThread != NULL) {
      if (!quietSuccess) {
        LPCWSTR msg[] = { 
          FRC_TITLE, 
          L"FRC was successfull started.",
          L"A remote FRC was terminated."
        };
        FrcMessage(msg, didKill ? 2 : 1, FMSG_MB_OK);
      }
      return TRUE;
    } else {
      if (!quietFailure) {
        LPCWSTR msg[] = { FRC_TITLE, L"FRC main thread could not be started." };
        FrcMessage(msg, 1, FMSG_WARNING | FMSG_MB_OK | FMSG_ERRORTYPE);
      }
      return FALSE;
    }
  } else {
    return TRUE;
  }
}

BOOL ReceiverStop() {
  SetEvent(stopEvent);
  if (WaitForSingleObject(doneEvent, TIMEOUT + PATIENCE) != WAIT_OBJECT_0)
    ReceiverTerminate();
  return !isReceiverStarted();
}

BOOL FrcGoto(WCHAR* path, BOOL navinto) {
  LPWSTR file = NULL;
  WCHAR fileFirst = L'\0';
  while (!*PathRemoveBackslashW(path));
  BOOL success = PathFileExistsW(path);
  if (success) {
    struct FarPanelDirectory dir = { sizeof(dir) };
    dir.Name = path;
    if (!navinto || (GetFileAttributesW(path) & FILE_ATTRIBUTE_DIRECTORY) == 0) {
      file = PathFindFileNameW(path);
      if (file == path) 
        file = NULL;
      else {
        fileFirst = file[0];
        file[0] = L'\0';
      }
    }
    success = (BOOL) api.PanelControl(PANEL_ACTIVE, FCTL_SETPANELDIRECTORY, 0, &dir);
    if (success && file) {
      HANDLE heap = GetProcessHeap();
      struct PanelInfo PanelInfo = { sizeof(PanelInfo) };
      file[0] = fileFirst;
      success = (BOOL) api.PanelControl(PANEL_ACTIVE, FCTL_GETPANELINFO, 0, &PanelInfo);
      if (success) {
        struct FarGetPluginPanelItem PanelItem = { sizeof(PanelItem) };
        unsigned int i;
        success = FALSE;
        PanelItem.Size = 2 * MAX_PATH * sizeof(WCHAR) + sizeof(*PanelItem.Item);
        PanelItem.Item = (struct PluginPanelItem*) HeapAlloc(heap, HEAP_ZERO_MEMORY, PanelItem.Size);
        struct PanelRedrawInfo PanelDrawInfo = { sizeof(PanelDrawInfo) };
        for (i = 0; PanelItem.Item && i < PanelInfo.ItemsNumber; i++) {
          size_t size = api.PanelControl(PANEL_ACTIVE, FCTL_GETPANELITEM, i, NULL);
          if (size > PanelItem.Size) {
            PVOID item = HeapReAlloc(heap, HEAP_ZERO_MEMORY, PanelItem.Item, size);
            if (!item) continue;
            PanelItem.Item = (struct PluginPanelItem*) item;
            PanelItem.Size = size;
          }
          if (api.PanelControl(PANEL_ACTIVE, FCTL_GETPANELITEM, i, &PanelItem)) {
            if (!StrCmpIW((LPWSTR)PanelItem.Item->FileName, file)) {
              success = TRUE;
              PanelDrawInfo.CurrentItem = i;
              break;
            }
          }
        }
        if (PanelItem.Item)
          HeapFree(heap, 0, (PVOID) PanelItem.Item);
        if (success)
          success = (BOOL) api.PanelControl(PANEL_ACTIVE, FCTL_REDRAWPANEL, 0, &PanelDrawInfo);
      } 
    }
  }
  return success;
}

BOOL FrcCopy(WCHAR* arg, BOOL escape) {
  if (escape && StrChrW(arg, L' ')) {
    int size = lstrlenW(arg)+1;
    HANDLE heap = GetProcessHeap();
    WCHAR* quot = HeapAlloc(heap, HEAP_ZERO_MEMORY, sizeof(WCHAR)*(size+2));
    if (quot) {
      lstrcpynW(quot+1, arg, size);
      quot[size] = quot[0] = L'"';
      quot[size+1] = L'\0';
      api.PanelControl(PANEL_ACTIVE, FCTL_INSERTCMDLINE, 0, quot);
      HeapFree(heap, 0, quot);
    } else {
      return FALSE;
    }
  } else {
    api.PanelControl(PANEL_ACTIVE, FCTL_INSERTCMDLINE, 0, arg);
  }
  return TRUE;
}

intptr_t WINAPI ProcessSynchroEventW(const struct ProcessSynchroEventInfo * Info) {
  if (Info->StructSize >= sizeof(*Info) && Info->Event == SE_COMMONSYNCHRO) {
    FRC_COMMAND *cmd = (FRC_COMMAND*) Info->Param;
    BOOL success = FALSE;
    BOOL escaped = FALSE;
    BOOL navinto = FALSE;
    HWND console = GetConsoleWindow();
    if (IsIconic(console))
        ShowWindow(console, SW_SHOW);
    BringWindowToTop(console) && SetForegroundWindow(console);
    if (cmd && cmd->arg) {
      switch (cmd->type) {
        case FRC_INTO:
          navinto = TRUE;
        case FRC_GOTO:
          success = FrcGoto(cmd->arg, navinto);
          break;
        case FRC_QCPY:
          escaped = TRUE;
        case FRC_COPY:
          success = FrcCopy(cmd->arg, escaped);
          break;
      }
    }
    if (!success) {
      LPCWSTR msg[] = { FRC_TITLE, L"An unexpected error occurred." };
      FrcMessage(msg, 1, FMSG_WARNING | FMSG_MB_OK | FMSG_ERRORTYPE);
    }
    return 1;
  }
  return 0;
}

void WINAPI SetStartupInfoW(const struct PluginStartupInfo * Info) {
  struct OpenInfo oi = { sizeof(oi) };
  api = *Info;
  if (!stopEvent)
    stopEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
  if (!doneEvent)
    doneEvent = CreateEvent(NULL, TRUE, TRUE, NULL);
  oi.OpenFrom = OPEN_VIEWER;
  OpenW(&oi);
}


HANDLE WINAPI OpenW(const struct OpenInfo *Info) {
  if (Info->OpenFrom == OPEN_FROMMACRO || Info->OpenFrom == OPEN_LUAMACRO) {
    struct OpenMacroInfo *Args = (struct OpenMacroInfo *) Info->Data;
    if ((Args != NULL) && (Args->Count > 0) && (Args->Values[0].Type == FMVT_STRING)) {
      if (ReceiverStart(TRUE, FALSE, TRUE)) {
        LPCWSTR command = Args->Values[0].Value.String;
        LPCWSTR parameters = NULL;
        if (Args->Count > 1 && Args->Values[1].Type == FMVT_STRING)
          parameters = Args->Values[1].Value.String;
        return ShellExecuteW(NULL, L"open", command, parameters, NULL, SW_SHOWNORMAL);
      } else
        return NULL;
    }
  }
  if (isReceiverStarted()) {
    LPCWSTR messages[] = { FRC_TITLE, L"FRC is currently running. Do you want to stop it?" };
    if (FrcMessage(messages, 1, FMSG_MB_YESNO) == 0) {
      ReceiverStop();
      return (HANDLE)1;
    } else {
      return frcThread;
    }
  } else {
    BOOL q = (Info->OpenFrom == OPEN_VIEWER);
    ReceiverStart(q, q, FALSE);
    return frcThread;
  }
}

void WINAPI GetGlobalInfoW(struct GlobalInfo *Info) {
  Info->StructSize = sizeof(struct GlobalInfo);
  Info->MinFarVersion = MAKEFARVERSION(3, 0, 0, 2927, VS_RELEASE);
  Info->Version = MAKEFARVERSION(2, 3, 0, 0, VS_RELEASE);
  Info->Guid = FRC_GUID_PLUG;
  Info->Title = L"FRC";
  Info->Description = L"FAR Remote Control";
  Info->Author = L"Jesko Hüttenhain";
}

EXTERN_C BOOL WINAPI _DllMainCRTStartup(HINSTANCE instance, DWORD reason, LPVOID reserved) {
  switch (reason) {
    case DLL_PROCESS_ATTACH:
      break;
    case DLL_PROCESS_DETACH:
      ExitFARW(NULL);
      break;
  }
  return TRUE;
}

void WINAPI ExitFARW(const struct ExitInfo * Info) {
  ReceiverStop();
  NullHandle(&stopEvent);
  NullHandle(&doneEvent);
}

void WINAPI GetPluginInfoW(struct PluginInfo *Info) {
  Info->StructSize = sizeof(struct PluginInfo);
  Info->Flags = PF_DIALOG | PF_PRELOAD;
  Info->PluginMenu.Guids = &FRC_GUID_PLUG;
  Info->PluginMenu.Strings = &FRC_TITLE;
  Info->PluginMenu.Count = 1;
}
