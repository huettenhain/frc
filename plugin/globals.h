#pragma once

const static GUID FRC_GUID_PLUG = {
  0x5846B0A6, 0xE130, 0x4B20, 0x8F, 0xDD, 0x5C, 0xCD, 0x70, 0xC8, 0x60, 0xBD
};

const static GUID FRC_GUID_FMSG = {
  0x5846B0A6, 0xE130, 0x4B20, 0x8F, 0xDD, 0x5C, 0xCD, 0x70, 0xC8, 0x60, 0xBE
};

const static wchar_t *FRC_MSLOT = L"\\\\.\\mailslot\\FRC";
const static wchar_t *FRC_TITLE = L"Far Remote Control";

typedef enum {
  FRC_NONE = 0,
  FRC_INTO = 1,
  FRC_COPY = 2,
  FRC_QCPY = 3,
  FRC_GOTO = 4,
  FRC_QUIT = 0xFF,
} FRC_COMMAND_TYPE;

#define FRC_LAST FRC_GOTO
#define IS_VALID_CMD(_x) ((_x)>=1 && (_x)<=FRC_LAST)
