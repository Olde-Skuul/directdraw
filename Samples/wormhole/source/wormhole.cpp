//-----------------------------------------------------------------------------
// File: WormHole.CPP
//
// Desc:
//
// Copyright (c) 1995-1999 Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------

#define INITGUID

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

//-----------------------------------------------------------------------------
// Include files
//-----------------------------------------------------------------------------
#include "resource.h"
#include <ddraw.h>
#include <stdarg.h>
#include <stdio.h>

//-----------------------------------------------------------------------------
// Local definitions
//-----------------------------------------------------------------------------
#define NAME "WormHole"
#define TITLE "Direct Draw WormHole Example"

//-----------------------------------------------------------------------------
// Private functions
//-----------------------------------------------------------------------------

HWND g_hWnd;
PALETTEENTRY pe[256];
BOOL g_bActive;
BOOL g_bIsInitialized = FALSE;

LPDIRECTDRAW7 g_pDD = NULL;
LPDIRECTDRAWSURFACE7 g_pDDSPrimary = NULL;
LPDIRECTDRAWSURFACE7 g_pDDSOne = NULL;
LPDIRECTDRAWCLIPPER g_pClipper = NULL;
LPDIRECTDRAWPALETTE g_pDDPal = NULL;

static BOOL ReadBMPIntoSurfaces()
{
	HRESULT hRet;
	HRSRC hBMP;
	RGBQUAD Palette[256];
	DDSURFACEDESC2 ddsd;
	LPSTR pBits;
	LPSTR pSrc;
	BYTE* pBMP;
	int i;

	hBMP = FindResourceA(NULL, "wormhole_bmp", RT_BITMAP);
	if (hBMP == NULL) {
		return FALSE;
	}

	pBMP = (BYTE*)LockResource(LoadResource(NULL, hBMP));

	memcpy(Palette, &pBMP[sizeof(BITMAPINFOHEADER)], sizeof(Palette));

	FreeResource(hBMP);

	for (i = 0; i < 256; i++) {
		pe[i].peRed = Palette[i].rgbRed;
		pe[i].peGreen = Palette[i].rgbGreen;
		pe[i].peBlue = Palette[i].rgbBlue;
	}

	if (NULL == g_pDDPal) {
		hRet = g_pDD->CreatePalette(DDPCAPS_8BIT, pe, &g_pDDPal, NULL);

		if (hRet != DD_OK) {
			return FALSE;
		}
	}

	g_pDDSPrimary->SetPalette(g_pDDPal);

	ddsd.dwSize = sizeof(ddsd);
	hRet = g_pDDSOne->Lock(NULL, &ddsd, DDLOCK_WAIT, NULL);
	if (hRet != DD_OK) {
		return FALSE;
	}

	pBits = (LPSTR)ddsd.lpSurface;
	pSrc = (LPSTR)(&pBMP[sizeof(BITMAPINFOHEADER) + sizeof(Palette) +
		(640 * 479)]);

	for (i = 0; i < 480; i++) {
		memcpy(pBits, pSrc, 640);
		pBits += ddsd.lPitch;
		pSrc -= 640;
	}
	g_pDDSOne->Unlock(NULL);

	return TRUE;
}

//-----------------------------------------------------------------------------
// Name: ReleaseAllObjects()
// Desc: Finished with all objects we use; release them
//-----------------------------------------------------------------------------
static void ReleaseAllObjects()
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
// Name: RestoreAll()
// Desc: Restore all lost objects
//-----------------------------------------------------------------------------
static BOOL RestoreAll()
{
	BOOL bResult;

	bResult =
		g_pDDSPrimary->Restore() == DD_OK && g_pDDSOne->Restore() == DD_OK;

	ReadBMPIntoSurfaces();

	return bResult;
}

//-----------------------------------------------------------------------------
// Name: UpdateFrame()
// Desc: Decide what needs to be blitted next
//-----------------------------------------------------------------------------
static void UpdateFrame()
{
	RECT rcRect;
	RECT destRect;
	HRESULT hRet;
	POINT pt;

	rcRect.left = 0;
	rcRect.top = 0;
	rcRect.right = 640;
	rcRect.bottom = 480;

	GetClientRect(g_hWnd, &destRect);

	pt.x = pt.y = 0;
	ClientToScreen(g_hWnd, &pt);
	OffsetRect(&destRect, pt.x, pt.y);

	for (;;) {
		hRet = g_pDDSPrimary->Blt(&destRect, g_pDDSOne, &rcRect, 0, NULL);

		if (hRet == DD_OK) {
			break;
		}
		if (hRet == DDERR_SURFACELOST) {
			if (!RestoreAll()) {
				return;
			}
			continue;
		}
		if (hRet != DDERR_WASSTILLDRAWING) {
			return;
		}
	}
}

//-----------------------------------------------------------------------------
// Name: WindowProc()
// Desc: The Main Window Procedure
//-----------------------------------------------------------------------------
static LRESULT FAR PASCAL WindowProc(
	HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message) {
	case WM_ACTIVATE:
		g_bActive = static_cast<BOOL>(wParam);
		break;

	case WM_CREATE:
		break;

	case WM_SETCURSOR:
		SetCursor(NULL);
		if (g_bIsInitialized) {
			UpdateFrame();
			g_pDDPal->GetEntries(0, 0, 256, pe);
		}
		break;

	case WM_KEYDOWN:
		switch (wParam) {
		case VK_ESCAPE:
		case VK_F12:
			PostMessage(hWnd, WM_CLOSE, 0, 0);
			break;
		}
		break;

	case WM_DESTROY:
		ReleaseAllObjects();
		PostQuitMessage(0);
		break;
	}
	return DefWindowProcA(hWnd, message, wParam, lParam);
}

//-----------------------------------------------------------------------------
// Name: InitApp()
// Desc: Do work required for every instance of the application:
//          Create the window, initialize data
//-----------------------------------------------------------------------------
static BOOL InitApp(HINSTANCE hInstance, int nCmdShow)
{
	WNDCLASSA wc;
	DDSURFACEDESC2 ddsd;
	HRESULT hRet;

	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WindowProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIconA(hInstance, MAKEINTRESOURCE(IDI_MAIN_ICON));
	wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
	wc.hbrBackground = NULL;
	wc.lpszMenuName = NAME;
	wc.lpszClassName = NAME;
	RegisterClassA(&wc);

	g_hWnd = CreateWindowEx(0, NAME, TITLE, WS_POPUP, 0, 0,
		GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), NULL,
		NULL, hInstance, NULL);

	if (!g_hWnd) {
		return FALSE;
	}
	ShowWindow(g_hWnd, nCmdShow);
	UpdateWindow(g_hWnd);

	///////////////////////////////////////////////////////////////////////////
	// Create the main DirectDraw object
	///////////////////////////////////////////////////////////////////////////
	hRet = DirectDrawCreateEx(NULL, (VOID**)&g_pDD, IID_IDirectDraw7, NULL);
	if (hRet != DD_OK) {
		return InitFail(g_hWnd, hRet, "DirectDrawCreateEx FAILED");
	}

	// Get exclusive mode
	hRet =
		g_pDD->SetCooperativeLevel(g_hWnd, DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN);
	if (hRet != DD_OK) {
		return InitFail(g_hWnd, hRet, "SetCooperativeLevel FAILED");
	}

	// Set the video mode to 640x480x8
	hRet = g_pDD->SetDisplayMode(640, 480, 8, 0, 0);
	if (hRet != DD_OK) {
		return InitFail(g_hWnd, hRet, "SetDisplayMode FAILED");
	}

	// Create the primary surface
	ZeroMemory(&ddsd, sizeof(ddsd));
	ddsd.dwSize = sizeof(ddsd);
	ddsd.dwFlags = DDSD_CAPS;
	ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
	hRet = g_pDD->CreateSurface(&ddsd, &g_pDDSPrimary, NULL);
	if (hRet != DD_OK) {
		return InitFail(g_hWnd, hRet, "CreateSurface FAILED");
	}

	ddsd.dwSize = sizeof(ddsd);
	ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
	ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;
	ddsd.dwWidth = 640;
	ddsd.dwHeight = 480;
	g_pDD->CreateSurface(&ddsd, &g_pDDSOne, NULL);
	if (g_pDDSOne == NULL) {
		return InitFail(g_hWnd, hRet, "CreateSurface FAILED");
	}

	g_bIsInitialized = TRUE;
	return TRUE;
}

//-----------------------------------------------------------------------------
// Name: CyclePalette()
// Desc:
//-----------------------------------------------------------------------------
static void CyclePalette()
{
	int reg[15];
	int k;
	PALETTEENTRY Temp[15];

	// Rotate the whole palette by 15 colors
	memcpy(Temp, &pe[30], sizeof(PALETTEENTRY) * 15);
	memcpy(&pe[30], &pe[45], sizeof(PALETTEENTRY) * 210);
	memcpy(&pe[240], Temp, sizeof(PALETTEENTRY) * 15);

	for (k = 2; k < 17; k++) {
		reg[k - 2] = pe[15 * k + 14].peRed;
		pe[15 * k + 14].peRed = pe[15 * k + 13].peRed;
		pe[15 * k + 13].peRed = pe[15 * k + 12].peRed;
		pe[15 * k + 12].peRed = pe[15 * k + 11].peRed;
		pe[15 * k + 11].peRed = pe[15 * k + 10].peRed;
		pe[15 * k + 10].peRed = pe[15 * k + 9].peRed;
		pe[15 * k + 9].peRed = pe[15 * k + 8].peRed;
		pe[15 * k + 8].peRed = pe[15 * k + 7].peRed;
		pe[15 * k + 7].peRed = pe[15 * k + 6].peRed;
		pe[15 * k + 6].peRed = pe[15 * k + 5].peRed;
		pe[15 * k + 5].peRed = pe[15 * k + 4].peRed;
		pe[15 * k + 4].peRed = pe[15 * k + 3].peRed;
		pe[15 * k + 3].peRed = pe[15 * k + 2].peRed;
		pe[15 * k + 2].peRed = pe[15 * k + 1].peRed;
		pe[15 * k + 1].peRed = pe[15 * k].peRed;
		pe[15 * k].peRed = static_cast<BYTE>(reg[k - 2]);

		reg[k - 2] = pe[15 * k + 14].peGreen;
		pe[15 * k + 14].peGreen = pe[15 * k + 13].peGreen;
		pe[15 * k + 13].peGreen = pe[15 * k + 12].peGreen;
		pe[15 * k + 12].peGreen = pe[15 * k + 11].peGreen;
		pe[15 * k + 11].peGreen = pe[15 * k + 10].peGreen;
		pe[15 * k + 10].peGreen = pe[15 * k + 9].peGreen;
		pe[15 * k + 9].peGreen = pe[15 * k + 8].peGreen;
		pe[15 * k + 8].peGreen = pe[15 * k + 7].peGreen;
		pe[15 * k + 7].peGreen = pe[15 * k + 6].peGreen;
		pe[15 * k + 6].peGreen = pe[15 * k + 5].peGreen;
		pe[15 * k + 5].peGreen = pe[15 * k + 4].peGreen;
		pe[15 * k + 4].peGreen = pe[15 * k + 3].peGreen;
		pe[15 * k + 3].peGreen = pe[15 * k + 2].peGreen;
		pe[15 * k + 2].peGreen = pe[15 * k + 1].peGreen;
		pe[15 * k + 1].peGreen = pe[15 * k].peGreen;
		pe[15 * k].peGreen = static_cast<BYTE>(reg[k - 2]);

		reg[k - 2] = pe[15 * k + 14].peBlue;
		pe[15 * k + 14].peBlue = pe[15 * k + 13].peBlue;
		pe[15 * k + 13].peBlue = pe[15 * k + 12].peBlue;
		pe[15 * k + 12].peBlue = pe[15 * k + 11].peBlue;
		pe[15 * k + 11].peBlue = pe[15 * k + 10].peBlue;
		pe[15 * k + 10].peBlue = pe[15 * k + 9].peBlue;
		pe[15 * k + 9].peBlue = pe[15 * k + 8].peBlue;
		pe[15 * k + 8].peBlue = pe[15 * k + 7].peBlue;
		pe[15 * k + 7].peBlue = pe[15 * k + 6].peBlue;
		pe[15 * k + 6].peBlue = pe[15 * k + 5].peBlue;
		pe[15 * k + 5].peBlue = pe[15 * k + 4].peBlue;
		pe[15 * k + 4].peBlue = pe[15 * k + 3].peBlue;
		pe[15 * k + 3].peBlue = pe[15 * k + 2].peBlue;
		pe[15 * k + 2].peBlue = pe[15 * k + 1].peBlue;
		pe[15 * k + 1].peBlue = pe[15 * k].peBlue;
		pe[15 * k].peBlue = static_cast<BYTE>(reg[k - 2]);
	}

	g_pDD->WaitForVerticalBlank(DDWAITVB_BLOCKBEGIN, NULL);

	if (g_pDDPal->SetEntries(0, 0, 256, pe) != DD_OK) {
		return;
	}
}

//-----------------------------------------------------------------------------
// Name: WinMain()
// Desc: Initialization, message loop
//-----------------------------------------------------------------------------
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE /* hPrevInstance */,
	LPSTR /* lpCmdLine */, int nCmdShow)
{
	MSG msg;

	if (!InitApp(hInstance, nCmdShow)) {
		return FALSE;
	}

	ReadBMPIntoSurfaces();
	UpdateFrame();

	for (;;) {
		if (PeekMessageA(&msg, NULL, 0, 0, PM_NOREMOVE)) {
			if (!GetMessageA(&msg, NULL, 0, 0)) {
				return static_cast<int>(msg.wParam);
			}
			TranslateMessage(&msg);
			DispatchMessageA(&msg);
		} else if (g_bActive) {
			CyclePalette();
		} else {
			WaitMessage();
		}
	}
}
