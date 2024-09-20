//-----------------------------------------------------------------------------
// File: FSWindow.H
//
// Desc: Header file for full-screen non-GDI window update functions
//
// Copyright (c) 1998-1999 Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------

#ifndef __FSWINDOW_H__
#define __FSWINDOW_H__

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

#include <ddraw.h>

#define USE_SURF4

#ifdef __cplusplus
extern "C" {
#endif

// Exported function prototypes
extern void FSWindow_Init(HWND hwndApp, IDirectDraw7* dd,
	IDirectDrawSurface7* FrontBuffer, IDirectDrawSurface7* BackBuffer);
extern HWND FSWindow_Begin(HWND hwnd, BOOL StaticContent);
extern void FSWindow_End(void);
extern void FSWindow_Update(void);
extern BOOL FSWindow_IsActive(void);
extern BOOL FSWindow_IsStatic(void);

#ifdef __cplusplus
}
#endif

#endif
