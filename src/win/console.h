#ifndef TT_WINDOWS_H
#define TT_WINDOWS_H

// Definitions of needed Windows console APIs, taken from the Windows SDK
// documentation.
//
// Copyright (c) Microsoft Corp. All rights reserved.

#include <windows.h>

HRESULT WINAPI
CreatePseudoConsole (
  _In_ COORD size,
  _In_ HANDLE hInput,
  _In_ HANDLE hOutput,
  _In_ DWORD dwFlags,
  _Out_ HPCON *phPC
);

void WINAPI
ClosePseudoConsole (
  _In_ HPCON hPC
);

HRESULT WINAPI
ResizePseudoConsole (
  _In_ HPCON hPC,
  _In_ COORD size
);

#endif // TT_WINDOWS_H