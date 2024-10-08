/*==========================================================================
 *
 *  Copyright (C) 1998-1999 Microsoft Corporation. All Rights Reserved.
 *
 *  File:       ddutil.cpp
 *  Content:    Routines for loading bitmap and palettes from resources
 *
 ***************************************************************************/

#ifndef __DDUTIL_H__
#define __DDUTIL_H__

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

#include <ddraw.h>

/* Assume C declarations for C++ */
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

extern IDirectDrawSurface7* DDLoadBitmap(
	IDirectDraw7* pdd, LPCSTR szBitmap, int dx, int dy);
extern HRESULT DDReLoadBitmap(IDirectDrawSurface7* pdds, LPCSTR szBitmap);
extern HRESULT DDCopyBitmap(
	IDirectDrawSurface7* pdds, HBITMAP hbm, int x, int y, int dx, int dy);
extern IDirectDrawPalette* DDLoadPalette(IDirectDraw7* pdd, LPCSTR szBitmap);
extern DWORD DDColorMatch(IDirectDrawSurface7* pdds, COLORREF rgb);
extern HRESULT DDSetColorKey(IDirectDrawSurface7* pdds, COLORREF rgb);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
