//-----------------------------------------------------------------------------
// File: DDEx5.CPP
//
// Desc: Direct Draw example program 5.  Adds functionality to
//       example program 4.  Uses GetEntries() to read a palette,
//       modifies the entries, and then uses SetEntries() to update
//       the palette.  This program requires 1.2 Meg of video ram.
//
// Copyright (c) 1995-1999 Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------

#define INITGUID

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

//-----------------------------------------------------------------------------
// Include files
//-----------------------------------------------------------------------------
#include <windows.h>

#include "ddutil.h"
#include "resource.h"
#include <ddraw.h>
#include <stdarg.h>
#include <stdio.h>

//-----------------------------------------------------------------------------
// Local definitions
//-----------------------------------------------------------------------------
#define NAME "DDExample5"
#define TITLE "Direct Draw Example 5"

//-----------------------------------------------------------------------------
// Global data
//-----------------------------------------------------------------------------
LPDIRECTDRAW7 g_pDD = NULL;                // DirectDraw object
LPDIRECTDRAWSURFACE7 g_pDDSPrimary = NULL; // DirectDraw primary surface
LPDIRECTDRAWSURFACE7 g_pDDSBack = NULL;    // DirectDraw back surface
LPDIRECTDRAWSURFACE7 g_pDDSOne = NULL;     // Offscreen surface 1
LPDIRECTDRAWPALETTE g_pDDPal = NULL;       // The primary surface palette
BOOL g_bActive = FALSE;                    // Is application active?

//-----------------------------------------------------------------------------
// Local data
//-----------------------------------------------------------------------------
// Name of our bitmap resource.
static char szBitmap[] = "ALL";
// Marks the colors used in the torus
static BYTE torusColors[256];

//-----------------------------------------------------------------------------
// Name: ReleaseAllObjects()
// Desc: Finished with all objects we use; release them
//-----------------------------------------------------------------------------
static void ReleaseAllObjects(void)
{
	if (g_pDD != NULL) {
		if (g_pDDSPrimary != NULL) {
			g_pDDSPrimary->Release();
			g_pDDSPrimary = NULL;
		}
		if (g_pDDSOne != NULL) {
			g_pDDSOne->Release();
			g_pDDSOne = NULL;
		}
		if (g_pDDPal != NULL) {
			g_pDDPal->Release();
			g_pDDPal = NULL;
		}
		g_pDD->Release();
		g_pDD = NULL;
	}
}

//-----------------------------------------------------------------------------
// Name: InitFail()
// Desc: This function is called if an initialization function fails
//-----------------------------------------------------------------------------
static HRESULT InitFail(HWND hWnd, HRESULT hRet, LPCTSTR szError, ...)
{
	char szBuff[128];
	va_list vl;

	va_start(vl, szError);
	vsprintf(szBuff, szError, vl);
	ReleaseAllObjects();
	MessageBoxA(hWnd, szBuff, TITLE, MB_OK);
	DestroyWindow(hWnd);
	va_end(vl);
	return hRet;
}

//-----------------------------------------------------------------------------
// Name: MarkColors()
// Desc: Mark the colors used in the torus frames
//-----------------------------------------------------------------------------
static void MarkColors(void)
{
	DDSURFACEDESC2 ddsd;
	int i, x, y;

	// First, set all colors as unused
	for (i = 0; i < 256; i++) {
		torusColors[i] = 0;
	}

	// Lock the surface and scan the lower part (the torus area)
	// and remember all the indecies we find.
	ZeroMemory(&ddsd, sizeof(ddsd));
	ddsd.dwSize = sizeof(ddsd);
	while (g_pDDSOne->Lock(NULL, &ddsd, 0, NULL) == DDERR_WASSTILLDRAWING) {
	}

	// Now search through the torus frames and mark used colors
	for (y = 480; y < 480 + 384; y++) {
		for (x = 0; x < 640; x++) {
			torusColors[((BYTE*)ddsd.lpSurface)[y * ddsd.lPitch + x]] = 1;
		}
	}
	g_pDDSOne->Unlock(NULL);
}

//-----------------------------------------------------------------------------
// Name: RestoreAll()
// Desc: Restore all lost objects
//-----------------------------------------------------------------------------
static HRESULT RestoreAll(void)
{
	HRESULT hRet;

	hRet = g_pDDSPrimary->Restore();
	if (hRet == DD_OK) {
		hRet = g_pDDSOne->Restore();
		if (hRet == DD_OK) {
			DDReLoadBitmap(g_pDDSOne, szBitmap);
		}
	}

	// Loose the old palette
	g_pDDPal->Release();
	// Create and set the palette (restart cycling from the same place)
	g_pDDPal = DDLoadPalette(g_pDD, szBitmap);

	if (g_pDDPal) {
		g_pDDSPrimary->SetPalette(g_pDDPal);
	}
	MarkColors();

	return hRet;
}

//-----------------------------------------------------------------------------
// Name: UpdateFrame()
// Desc: Decide what needs to be blitted next, wait for flip to complete,
//       then flip the buffers.
//-----------------------------------------------------------------------------
static void UpdateFrame(void)
{
	static DWORD lastTickCount[4] = {0, 0, 0, 0};
	static int currentFrame[3] = {0, 0, 0};
	DWORD delay[4] = {50, 78, 13, 93};
	int xpos[3] = {288, 190, 416};
	int ypos[3] = {128, 300, 256};
	int i;
	DWORD thisTickCount;
	RECT rcRect;
	HRESULT hRet;
	PALETTEENTRY pe[256];

	// Decide which frame will be blitted next
	thisTickCount = GetTickCount();
	for (i = 0; i < 3; i++) {
		if ((thisTickCount - lastTickCount[i]) > delay[i]) {
			// Move to next frame;
			lastTickCount[i] = thisTickCount;
			currentFrame[i]++;
			if (currentFrame[i] > 59) {
				currentFrame[i] = 0;
			}
		}
	}

	// Blit the stuff for the next frame
	rcRect.left = 0;
	rcRect.top = 0;
	rcRect.right = 640;
	rcRect.bottom = 480;
	for (;;) {
		hRet =
			g_pDDSBack->BltFast(0, 0, g_pDDSOne, &rcRect, DDBLTFAST_NOCOLORKEY);
		if (hRet == DD_OK) {
			break;
		}
		if (hRet == DDERR_SURFACELOST) {
			hRet = RestoreAll();
			if (hRet != DD_OK) {
				return;
			}
		}
		if (hRet != DDERR_WASSTILLDRAWING) {
			return;
		}
	}
	if (hRet != DD_OK) {
		return;
	}

	for (i = 0; i < 3; i++) {
		rcRect.left = currentFrame[i] % 10 * 64;
		rcRect.top = currentFrame[i] / 10 * 64 + 480;
		rcRect.right = currentFrame[i] % 10 * 64 + 64;
		rcRect.bottom = currentFrame[i] / 10 * 64 + 64 + 480;

		for (;;) {
			hRet = g_pDDSBack->BltFast(static_cast<DWORD>(xpos[i]),
				static_cast<DWORD>(ypos[i]), g_pDDSOne, &rcRect,
				DDBLTFAST_SRCCOLORKEY);
			if (hRet == DD_OK) {
				break;
			}
			if (hRet == DDERR_SURFACELOST) {
				hRet = RestoreAll();
				if (hRet != DD_OK) {
					return;
				}
			}
			if (hRet != DDERR_WASSTILLDRAWING) {
				return;
			}
		}
	}

	if ((thisTickCount - lastTickCount[3]) > delay[3]) {
		// Change the palette
		if (g_pDDPal->GetEntries(0, 0, 256, pe) != DD_OK) {
			return;
		}

		for (i = 1; i < 256; i++) {
			if (!torusColors[i]) {
				continue;
			}
			pe[i].peRed = static_cast<BYTE>((pe[i].peRed + 2) % 256);
			pe[i].peGreen = static_cast<BYTE>((pe[i].peGreen + 1) % 256);
			pe[i].peBlue = static_cast<BYTE>((pe[i].peBlue + 3) % 256);
		}
		if (g_pDDPal->SetEntries(0, 0, 256, pe) != DD_OK) {
			return;
		}

		lastTickCount[3] = thisTickCount;
	}

	// Flip the surfaces
	for (;;) {
		hRet = g_pDDSPrimary->Flip(NULL, 0);
		if (hRet == DD_OK) {
			break;
		}
		if (hRet == DDERR_SURFACELOST) {
			hRet = RestoreAll();
			if (hRet != DD_OK) {
				break;
			}
		}
		if (hRet != DDERR_WASSTILLDRAWING) {
			break;
		}
	}
}

//-----------------------------------------------------------------------------
// Name: WindowProc()
// Desc: The Main Window Procedure
//-----------------------------------------------------------------------------
static LRESULT CALLBACK WindowProc(
	HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message) {
	case WM_ACTIVATE:
		// Pause if minimized
		g_bActive = !((BOOL)HIWORD(wParam));
		return 0L;

	case WM_DESTROY:
		// Clean up and close the app
		ReleaseAllObjects();
		PostQuitMessage(0);
		return 0L;

	case WM_KEYDOWN:
		// Handle any non-accelerated key commands
		switch (wParam) {
		case VK_ESCAPE:
		case VK_F12:
			PostMessageA(hWnd, WM_CLOSE, 0, 0);
			return 0L;
		}
		break;

	case WM_SETCURSOR:
		// Turn off the cursor since this is a full-screen app
		SetCursor(NULL);
		return TRUE;
	}
	return DefWindowProcA(hWnd, message, wParam, lParam);
}

//-----------------------------------------------------------------------------
// Name: InitApp()
// Desc: Do work required for every instance of the application:
//          Create the window, initialize data
//-----------------------------------------------------------------------------
static HRESULT InitApp(HINSTANCE hInstance, int nCmdShow)
{
	HWND hWnd;
	WNDCLASSA wc;
	DDSURFACEDESC2 ddsd;
	DDSCAPS2 ddscaps;
	HRESULT hRet;

	// Set up and register window class
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WindowProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIconA(hInstance, MAKEINTRESOURCE(IDI_MAIN_ICON));
	wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wc.lpszMenuName = NAME;
	wc.lpszClassName = NAME;
	RegisterClassA(&wc);

	// Create a window
	hWnd = CreateWindowExA(WS_EX_TOPMOST, NAME, TITLE, WS_POPUP, 0, 0,
		GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), NULL,
		NULL, hInstance, NULL);

	if (!hWnd) {
		return FALSE;
	}
	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);
	SetFocus(hWnd);

	///////////////////////////////////////////////////////////////////////////
	// Create the main DirectDraw object
	///////////////////////////////////////////////////////////////////////////
	hRet = DirectDrawCreateEx(NULL, (VOID**)&g_pDD, IID_IDirectDraw7, NULL);
	if (hRet != DD_OK) {
		return InitFail(hWnd, hRet, "DirectDrawCreateEx FAILED");
	}

	// Get exclusive mode
	hRet = g_pDD->SetCooperativeLevel(hWnd, DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN);
	if (hRet != DD_OK) {
		return InitFail(hWnd, hRet, "SetCooperativeLevel FAILED");
	}

	// Set the video mode to 640x480x8
	hRet = g_pDD->SetDisplayMode(640, 480, 8, 0, 0);
	if (hRet != DD_OK) {
		return InitFail(hWnd, hRet, "SetDisplayMode FAILED");
	}

	// Create the primary surface with 1 back buffer
	ZeroMemory(&ddsd, sizeof(ddsd));
	ddsd.dwSize = sizeof(ddsd);
	ddsd.dwFlags = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
	ddsd.ddsCaps.dwCaps =
		DDSCAPS_PRIMARYSURFACE | DDSCAPS_FLIP | DDSCAPS_COMPLEX;
	ddsd.dwBackBufferCount = 1;
	hRet = g_pDD->CreateSurface(&ddsd, &g_pDDSPrimary, NULL);
	if (hRet != DD_OK) {
		return InitFail(hWnd, hRet, "CreateSurface FAILED");
	}

	// Get a pointer to the back buffer
	ZeroMemory(&ddscaps, sizeof(ddscaps));
	ddscaps.dwCaps = DDSCAPS_BACKBUFFER;
	hRet = g_pDDSPrimary->GetAttachedSurface(&ddscaps, &g_pDDSBack);
	if (hRet != DD_OK) {
		return InitFail(hWnd, hRet, "GetAttachedSurface FAILED");
	}

	// Create and set the palette
	g_pDDPal = DDLoadPalette(g_pDD, szBitmap);
	if (g_pDDPal) {
		g_pDDSPrimary->SetPalette(g_pDDPal);
	}

	// Create the offscreen surface, by loading our bitmap.
	g_pDDSOne = DDLoadBitmap(g_pDD, szBitmap, 0, 0);
	if (g_pDDSOne == NULL) {
		return InitFail(hWnd, hRet, "DDLoadBitmap FAILED");
	}

	// Set the color key for this bitmap (black)
	DDSetColorKey(g_pDDSOne, RGB(0, 0, 0));

	MarkColors();

	return DD_OK;
}

//-----------------------------------------------------------------------------
// Name: WinMain()
// Desc: Initialization, message loop
//-----------------------------------------------------------------------------
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE /* hPrevInstance */,
	LPSTR /* lpCmdLine */, int nCmdShow)
{
	MSG msg;

	if (InitApp(hInstance, nCmdShow) != DD_OK) {
		return FALSE;
	}
	for (;;) {
		if (PeekMessageA(&msg, NULL, 0, 0, PM_NOREMOVE)) {
			if (!GetMessageA(&msg, NULL, 0, 0)) {
				return static_cast<int>(msg.wParam);
			}
			TranslateMessage(&msg);
			DispatchMessageA(&msg);
		} else if (g_bActive) {
			UpdateFrame();
		} else {
			// Make sure we go to sleep if we have nothing else to do
			WaitMessage();
		}
	}
}
