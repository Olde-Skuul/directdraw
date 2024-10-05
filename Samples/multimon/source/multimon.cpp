//-----------------------------------------------------------------------------
// File: Multimon.cpp
//
// Desc: This sample demonstrates the following programming concepts:
//   Writing code for a multi-monitor program that works on both Windows 95
//     (which does not support multiple monitors) and later Windows versions
//     (which do support it).
//   Using DirectDrawEnumerate and DirectDrawEnumerateEx to enumerate displays.
//   Working with separate device and focus windows.
//   Creating a video-memory sprite that spans multiple screens using multiple
//     copies of the image data.
//   Creating a system-memory sprite that spans multiple screens using a shared
//     copy of the image data.
//
// Copyright (c) 1998-1999 Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------

#define INITGUID

#include <Windows.h>

// Multimon.h implements support of the multimon APIs for when your program
// runs on Windows 95 systems.  You can #include multimon.h in all your source
// files that use multimon APIs, and you should #define COMPILE_MULTIMON_STUBS
// in only one of your source files.
#define COMPILE_MULTIMON_STUBS
#include <multimon.h>

#include "resource.h"
#include <ddraw.h>
#include <stdio.h>

// Constants for this sample:
#define NAME "MultiMon DirectDraw Sample"
#define TITLE "MultiMon DirectDraw Sample"

// Structures for this sample:

// An EnumInfo contains extra enumeration information that is passed
// into the the DDEnumCallbackEx function.
struct EnumInfo {
	BOOL bMultimonSupported;
	HRESULT hr;
};

// A Screen represents one display that images can be drawn to.
struct Screen {
	GUID guid;
	char szDesc[200];
	HMONITOR hmon;
	LPDIRECTDRAW7 pdd;
	LPDIRECTDRAWSURFACE7 pddsFront;
	LPDIRECTDRAWSURFACE7 pddsBack;
	Screen* pScreenNext; // For linked list
};

// A ScreenSurface holds a DirectDrawSurface that can be used on a
// particular screen.
struct ScreenSurface {
	Screen* pScreen;
	LPDIRECTDRAWSURFACE7 pdds;
	ScreenSurface* pScreenSurfaceNext; // For linked list
	// Could add a "last used time" field, which could be used to
	// determine whether this ScreenSurface should be
	// removed to free up video memory for another surface
};

// A Sprite holds generic information about a drawable image, and
// a linked list of SpriteSurfaces (one per screen).
struct Sprite {
	char szName[64];     // Name of this Sprite
	BOOL bForceSystem;   // If TRUE, don't try to create video memory surfaces
	RECT rcSrc;          // Dimensions of the image
	RECT rcDest;         // Destination of the rectangle to draw image into
	LONG xVel;           // X-Velocity for animation
	LONG yVel;           // Y-Velocity for animation
	HBITMAP hbmImage;    // Loaded bitmap image
	BYTE* pImageData;    // Sharable pointer to DD surface data
	DDSURFACEDESC2 ddsd; // Holds pitch and pixel format of pImageData
	ScreenSurface* pScreenSurfaceFirst; // Linked list of ScreenSurfaces
};

// Global variables for this sample:

// Linked list of Screens
static Screen* s_pScreenFirst = NULL;

// A sprite that uses video memory where possible
static Sprite* s_pSprite1 = NULL;

// A sprite that always uses system memory
static Sprite* s_pSprite2 = NULL;

// Main app focus HWND
static HWND s_hwnd = NULL;

// Whether app is actively drawing
static BOOL s_bActive = TRUE;

//-----------------------------------------------------------------------------
// Name: DestroyScreenSurfaces()
// Desc: Destroys all ScreenSurfaces attached to the given Sprite.  This is
//       called after restoring all surfaces (since image data may be lost) and
//       when preparing to exit the program.
//-----------------------------------------------------------------------------
static void DestroyScreenSurfaces(Sprite* pSprite)
{
	ScreenSurface* pScreenSurface = pSprite->pScreenSurfaceFirst;
	pSprite->pScreenSurfaceFirst = NULL;
	while (pScreenSurface != NULL) {

		ScreenSurface* pScreenSurfaceNext = pScreenSurface->pScreenSurfaceNext;
		pScreenSurface->pdds->Release();
		delete pScreenSurface;
		pScreenSurface = pScreenSurfaceNext;
	}

	if (pSprite->pImageData != NULL) {
		delete pSprite->pImageData;
		pSprite->pImageData = NULL;
	}
	ZeroMemory(&pSprite->ddsd, sizeof(pSprite->ddsd));
}

//-----------------------------------------------------------------------------
// Name: Cleanup()
// Desc: Releases all resources allocated during this program.
//-----------------------------------------------------------------------------
static void Cleanup(VOID)
{
	DestroyScreenSurfaces(s_pSprite1);
	DestroyScreenSurfaces(s_pSprite2);

	Screen* pScreen = s_pScreenFirst;
	s_pScreenFirst = NULL;
	while (pScreen != NULL) {
		Screen* pScreenNext = pScreen->pScreenNext;

		pScreen->pddsBack->Release();
		pScreen->pddsFront->Release();
		pScreen->pdd->RestoreDisplayMode();
		pScreen->pdd->SetCooperativeLevel(s_hwnd, DDSCL_NORMAL);
		pScreen->pdd->Release();

		delete pScreen;
		pScreen = pScreenNext;
	}
}

//-----------------------------------------------------------------------------
// Name: WindowProc()
// Desc: Handler for window messages.
//-----------------------------------------------------------------------------
static LRESULT CALLBACK WindowProc(
	HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
	case WM_SIZE:
		if (SIZE_MAXHIDE == wParam || SIZE_MINIMIZED == wParam) {
			s_bActive = FALSE;
			// Give window an icon and system menu on the taskbar when minimized
			SetWindowLongA(hwnd, GWL_STYLE, WS_SYSMENU);
		} else {
			s_bActive = TRUE;
			// Remove any window "decoration" when fullscreen
			SetWindowLongA(hwnd, GWL_STYLE, WS_POPUP);
		}
		return DefWindowProcA(hwnd, uMsg, wParam, lParam);

	case WM_CLOSE:
		return DefWindowProcA(hwnd, uMsg, wParam, lParam);

	case WM_CHAR:
		DestroyWindow(hwnd);
		return 0;

	case WM_DESTROY:
		Cleanup();
		PostQuitMessage(0);
		return 0;

	default:
		return DefWindowProcA(hwnd, uMsg, wParam, lParam);
	}
}

//-----------------------------------------------------------------------------
// Name: CreateFocusWindow()
// Desc: Creates the focus window, which is the window which will receive user
//       input.
//-----------------------------------------------------------------------------
static HRESULT CreateFocusWindow(HINSTANCE hInstance)
{
	WNDCLASSA wc;

	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WindowProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIconA(hInstance, MAKEINTRESOURCE(IDI_ICON1));
	wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = NAME;
	RegisterClassA(&wc);

	// Window style and dimensions are not important,
	// since they will be adjusted elsewhere.
	s_hwnd = CreateWindowExA(
		0, NAME, TITLE, 0, 0, 0, 0, 0, NULL, NULL, hInstance, NULL);

	// It is important to call ShowWindow on this window to ensure
	// that any activity from other windows is obscured.
	ShowWindow(s_hwnd, SW_SHOW);

	if (s_hwnd == NULL) {
		return E_FAIL;
	}
	return S_OK;
}

//-----------------------------------------------------------------------------
// Name: DDEnumCallbackEx()
// Desc: This callback function is called by DirectDraw once for each
//       available DirectDraw device.  In this implementation, it saves the
//       GUID, device description, and hmon in a Screen structure for later use.
//-----------------------------------------------------------------------------
static BOOL WINAPI DDEnumCallbackEx(GUID* pGuid, LPTSTR pszDesc,
	LPTSTR /* pszDriverName */, VOID* pContext, HMONITOR hmon)
{
	EnumInfo* pEnumInfo = (EnumInfo*)pContext;
	GUID guidNull;
	ZeroMemory(&guidNull, sizeof(GUID));

	if (s_pScreenFirst != NULL && s_pScreenFirst->guid == guidNull) {
		// We must be running on a multimon system, so get rid of the
		// guidNull Screen -- we want Screens with specific GUIDs.
		delete s_pScreenFirst;
		s_pScreenFirst = NULL;
	}

	// Store all the info in a Screen structure
	Screen* pScreenNew = new Screen;
	if (pScreenNew == NULL) {
		pEnumInfo->hr = E_OUTOFMEMORY;
		return FALSE; // fatal error, stop enumerating
	}
	ZeroMemory(pScreenNew, sizeof(Screen));
	if (pGuid == NULL) {
		pScreenNew->guid = guidNull;
	} else {
		pScreenNew->guid = *pGuid;
	}
	lstrcpy(pScreenNew->szDesc, pszDesc);
	pScreenNew->hmon = hmon;

	// Insert Screen into global linked list
	if (s_pScreenFirst == NULL) {
		s_pScreenFirst = pScreenNew;
	} else {
		// Insert at end of list
		Screen* pScreen = s_pScreenFirst;
		while (pScreen->pScreenNext != NULL) {
			pScreen = pScreen->pScreenNext;
		}
		pScreen->pScreenNext = pScreenNew;
	}
	// Keep enumerating
	return TRUE;
}

//-----------------------------------------------------------------------------
// Name: DDEnumCallback()
// Desc: This callback function is called by DirectDrawEnumerate when
//       DirectDrawEnumerateEx is not available.  It simply calls
//       DDEnumCallbackEx.
//-----------------------------------------------------------------------------
static BOOL WINAPI DDEnumCallback(
	GUID* pGuid, LPTSTR pszDesc, LPTSTR pszDriverName, VOID* pContext)
{
	DDEnumCallbackEx(pGuid, pszDesc, pszDriverName, pContext, NULL);
	// Stop enumerating -- For this sample, we don't want any
	// non-display devices
	return FALSE;
}

//-----------------------------------------------------------------------------
// Name: EnumerateScreens()
// Desc: Creates a Screen structure for every appropriate screen in the user's
//       computer.  This function is written in such a way that it will work on
//       systems that do not necessarily have DX6 (and thus
//       DirectDrawEnumerateEx) installed.  If your application has already
//       ensured that DX6 is present, you won't need to handle calling
//       DirectDrawEnumerate and providing a DDEnumCallback function.  If your
//       application has already ensured that DX7 is present, you don't even
//       need to use LoadLibrary and GetProcAddress.  Just call
//       DirectDrawEnumerateEx directly.
//
//-----------------------------------------------------------------------------
static HRESULT EnumerateScreens(VOID)
{
	HRESULT hr;
	LPDIRECTDRAWENUMERATEEX pDDEnumEx = NULL;
	EnumInfo enumInfo;

	ZeroMemory(&enumInfo, sizeof(enumInfo));

	HMODULE hModule = LoadLibraryA("ddraw.dll");
	// If ddraw.dll doesn't exist in the search path,
	// then DirectX probably isn't installed, so fail.
	if (hModule == NULL) {
		return E_FAIL;
	}

	pDDEnumEx = (LPDIRECTDRAWENUMERATEEX)GetProcAddress(
		hModule, "DirectDrawEnumerateExA");

	if (pDDEnumEx == NULL) {
		// We must be running on an old version of DirectDraw.
		// Therefore MultiMon isn't supported. Fall back on
		// DirectDrawEnumerate to enumerate standard devices on a
		// single-monitor system.
		enumInfo.bMultimonSupported = FALSE;
		hr = DirectDrawEnumerate(DDEnumCallback, &enumInfo);
	} else {
		enumInfo.bMultimonSupported = TRUE;
		hr = pDDEnumEx(
			DDEnumCallbackEx, &enumInfo, DDENUM_ATTACHEDSECONDARYDEVICES);
	}

	// If something failed inside the enumeration, be sure to return that
	// HRESULT
	if (SUCCEEDED(hr) && FAILED(enumInfo.hr)) {
		hr = enumInfo.hr;
	}

	FreeLibrary(hModule);
	return hr;
}

//-----------------------------------------------------------------------------
// Name: InitScreens()
// Desc: For each Screen, this function initializes DirectDraw and sets up the
//       front buffer, back buffer, and a clipper.  Many fullscreen DirectDraw
//       programs don't need to create clippers, but this one does so to allow
//       the sprites to be automatically clipped to each display.
//-----------------------------------------------------------------------------
static HRESULT InitScreens(VOID)
{
	HRESULT hr;

	DDSURFACEDESC2 ddsd;
	DDSCAPS2 ddsCaps;
	LPDIRECTDRAWCLIPPER pclip;
	RECT rc;
	BYTE rgnDataBuffer[1024];

	GUID guidNull;
	ZeroMemory(&guidNull, sizeof(GUID));

	Screen* pScreen;
	for (pScreen = s_pScreenFirst; pScreen != NULL;
		 pScreen = pScreen->pScreenNext) {
		GUID* pGuid;
		if (pScreen->guid == guidNull) {
			pGuid = NULL;
		} else {
			pGuid = &pScreen->guid;
		}
		if (FAILED(hr = DirectDrawCreateEx(pGuid, (VOID**)&(pScreen->pdd),
					   IID_IDirectDraw7, NULL))) {
			return hr;
		}

		if (pScreen == s_pScreenFirst) {
			DWORD dwFlags = DDSCL_SETFOCUSWINDOW;
			if (FAILED(
					hr = pScreen->pdd->SetCooperativeLevel(s_hwnd, dwFlags))) {
				return hr;
			}

			dwFlags = DDSCL_ALLOWREBOOT | DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN;
			if (FAILED(
					hr = pScreen->pdd->SetCooperativeLevel(s_hwnd, dwFlags))) {
				return hr;
			}
		} else {
			DWORD dwFlags2 = DDSCL_SETFOCUSWINDOW | DDSCL_CREATEDEVICEWINDOW |
				DDSCL_ALLOWREBOOT | DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN;
			if (FAILED(
					hr = pScreen->pdd->SetCooperativeLevel(s_hwnd, dwFlags2))) {
				return hr;
			}
		}

		if (FAILED(hr = pScreen->pdd->SetDisplayMode(640, 480, 16, 0, 0))) {
			if (FAILED(hr = pScreen->pdd->SetDisplayMode(640, 480, 32, 0, 0))) {
				return hr;
			}
		}
	}

	// Note: It is recommended that programs call SetDisplayMode on all screens
	// before creating/acquiring any DirectDrawSurfaces.
	for (pScreen = s_pScreenFirst; pScreen != NULL;
		 pScreen = pScreen->pScreenNext) {

		ZeroMemory(&ddsd, sizeof(ddsd));
		ddsd.dwSize = sizeof(ddsd);
		ddsd.dwFlags = DDSD_BACKBUFFERCOUNT | DDSD_CAPS;
		ddsd.dwBackBufferCount = 1;
		ddsd.ddsCaps.dwCaps =
			DDSCAPS_COMPLEX | DDSCAPS_FLIP | DDSCAPS_PRIMARYSURFACE;
		if (FAILED(hr = pScreen->pdd->CreateSurface(
					   &ddsd, &pScreen->pddsFront, NULL))) {
			return hr;
		}

		ZeroMemory(&ddsCaps, sizeof(ddsCaps));
		ddsCaps.dwCaps = DDSCAPS_BACKBUFFER;
		if (FAILED(hr = pScreen->pddsFront->GetAttachedSurface(
					   &ddsCaps, &pScreen->pddsBack))) {
			return hr;
		}

		ZeroMemory(&ddsd, sizeof(ddsd));
		ddsd.dwSize = sizeof(ddsd);
		if (FAILED(hr = pScreen->pddsFront->GetSurfaceDesc(&ddsd))) {
			return hr;
		}
		SetRect(&rc, 0, 0, static_cast<int>(ddsd.dwWidth),
			static_cast<int>(ddsd.dwHeight));
		HRGN hrgn = CreateRectRgn(0, 0, static_cast<int>(ddsd.dwWidth),
			static_cast<int>(ddsd.dwHeight));
		GetRegionData(hrgn, sizeof(rgnDataBuffer), (RGNDATA*)rgnDataBuffer);
		if (FAILED(hr = pScreen->pdd->CreateClipper(0, &pclip, 0))) {
			return hr;
		}
		if (FAILED(hr = pclip->SetClipList((RGNDATA*)rgnDataBuffer, 0))) {
			pclip->Release();
			return hr;
		}
		if (FAILED(hr = pScreen->pddsFront->SetClipper(pclip))) {
			pclip->Release();
			return hr;
		}
		if (FAILED(hr = pScreen->pddsBack->SetClipper(pclip))) {
			pclip->Release();
			return hr;
		}
		pclip->Release();
	}

	return S_OK;
}

//-----------------------------------------------------------------------------
// Name: InitSprites()
// Desc: Initializes the objects that will be drawn and animated.  Note that
//       the ScreenSurfaces are created when they are first needed, not here.
//-----------------------------------------------------------------------------
static HRESULT InitSprites(VOID)
{
	// Initialize the first Sprite.  This sprite will try to use video memory
	// on each Screen.
	s_pSprite1 = new Sprite;
	if (s_pSprite1 == NULL) {
		return E_OUTOFMEMORY;
	}
	ZeroMemory(s_pSprite1, sizeof(Sprite));
	lstrcpy(s_pSprite1->szName, "Sprite 1");
	s_pSprite1->bForceSystem =
		FALSE; // This sprite will try to use video memory
	s_pSprite1->hbmImage = (HBITMAP)LoadImage(GetModuleHandle(NULL),
		MAKEINTRESOURCE(IDB_BITMAP1), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
	if (s_pSprite1->hbmImage == NULL) {
		return E_FAIL;
	}

	BITMAP bm;
	GetObject(s_pSprite1->hbmImage, sizeof(bm), &bm); // get size of bitmap
	SetRect(&s_pSprite1->rcSrc, 0, 0, bm.bmWidth, bm.bmHeight);
	s_pSprite1->rcDest = s_pSprite1->rcSrc;
	s_pSprite1->xVel = 2; // Animation velocity
	s_pSprite1->yVel = 1;

	// Initialize the first Sprite.  This sprite will use system memory (and
	// share that memory between ScreenSurfaces whenever possible).
	s_pSprite2 = new Sprite;
	if (s_pSprite2 == NULL) {
		return E_OUTOFMEMORY;
	}
	ZeroMemory(s_pSprite2, sizeof(Sprite));
	lstrcpy(s_pSprite2->szName, "Sprite 2");
	// This sprite will always use system memory
	s_pSprite2->bForceSystem = TRUE;
	s_pSprite2->hbmImage = (HBITMAP)LoadImage(GetModuleHandle(NULL),
		MAKEINTRESOURCE(IDB_BITMAP2), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
	if (s_pSprite2->hbmImage == NULL) {
		return E_FAIL;
	}
	GetObject(s_pSprite2->hbmImage, sizeof(bm), &bm); // get size of bitmap
	SetRect(&s_pSprite2->rcSrc, 0, 0, bm.bmWidth, bm.bmHeight);
	s_pSprite2->rcDest = s_pSprite1->rcSrc;
	s_pSprite2->xVel = -1; // Animation velocity
	s_pSprite2->yVel = -2;

	return S_OK;
}

//-----------------------------------------------------------------------------
// Name: RectIntersectsMonitor()
// Desc: Returns TRUE if prcSrc intersects the monitor hmon, and uses prcScreen
//       to store prcSrc in that monitor's local coordinate system.
//-----------------------------------------------------------------------------
static BOOL RectIntersectsMonitor(RECT* prcSrc, HMONITOR hmon, RECT* prcScreen)
{
	MONITORINFO mi;
	RECT rcIntersection;
	BOOL bIntersects;
	ZeroMemory(&mi, sizeof(mi));
	mi.cbSize = sizeof(mi);
	if (hmon == NULL) {
		SetRect(&mi.rcMonitor, 0, 0, GetSystemMetrics(SM_CXSCREEN),
			GetSystemMetrics(SM_CYSCREEN));
	} else {
		GetMonitorInfo(hmon, &mi);
	}
	bIntersects = IntersectRect(&rcIntersection, prcSrc, &mi.rcMonitor);
	if (!bIntersects) {
		return FALSE;
	}
	*prcScreen = *prcSrc;
	OffsetRect(prcScreen, -mi.rcMonitor.left, -mi.rcMonitor.top);
	return TRUE;
}

//-----------------------------------------------------------------------------
// Name: SetupScreenSurfaceDDS()
// Desc: Generates the DirectDrawSurface for a new ScreenSurface, and draws
//       the appropriate image into it.
//-----------------------------------------------------------------------------
static HRESULT SetupScreenSurfaceDDS(
	Sprite* pSprite, ScreenSurface* pScreenSurface)
{
	HRESULT hr;
	DDSURFACEDESC2 ddsd;
	char sz[200];
	Screen* pScreen = pScreenSurface->pScreen;

	ZeroMemory(&ddsd, sizeof(ddsd));
	ddsd.dwSize = sizeof(ddsd);
	ddsd.dwFlags = DDSD_WIDTH | DDSD_HEIGHT | DDSD_CAPS;
	ddsd.dwWidth =
		static_cast<DWORD>(pSprite->rcSrc.right - pSprite->rcSrc.left);
	ddsd.dwHeight =
		static_cast<DWORD>(pSprite->rcSrc.bottom - pSprite->rcSrc.top);
	ddsd.ddsCaps.dwCaps = DDSCAPS_VIDEOMEMORY;

	// Try to create the surface in video memory, unless the bForceSystem flag
	// is set on the Sprite.
	if (pSprite->bForceSystem ||
		FAILED(hr = pScreen->pdd->CreateSurface(
				   &ddsd, &pScreenSurface->pdds, NULL))) {
		// Either this sprite has the bForceSystem flag, or creation in video
		// memory failed, so try to create the surface in system memory.
		ddsd.ddsCaps.dwCaps = DDSCAPS_SYSTEMMEMORY;
		if (FAILED(hr = pScreen->pdd->CreateSurface(
					   &ddsd, &pScreenSurface->pdds, NULL))) {
			return hr;
		}
	}

	if (ddsd.ddsCaps.dwCaps == DDSCAPS_SYSTEMMEMORY &&
		pSprite->pImageData != NULL) {
		// See if we can reuse the image data that is stored in the Sprite.
		// As long as the pixel formats match, the image is reusable.
		ZeroMemory(&ddsd, sizeof(ddsd));
		ddsd.dwSize = sizeof(ddsd);
		if (FAILED(hr = pScreenSurface->pdds->GetSurfaceDesc(&ddsd))) {
			return hr;
		}
		if (ddsd.ddpfPixelFormat.dwRGBBitCount ==
				pSprite->ddsd.ddpfPixelFormat.dwRGBBitCount &&
			ddsd.ddpfPixelFormat.dwRBitMask ==
				pSprite->ddsd.ddpfPixelFormat.dwRBitMask &&
			ddsd.ddpfPixelFormat.dwGBitMask ==
				pSprite->ddsd.ddpfPixelFormat.dwGBitMask &&
			ddsd.ddpfPixelFormat.dwBBitMask ==
				pSprite->ddsd.ddpfPixelFormat.dwBBitMask) {
			// Make the DDS use the Sprite's pImageData for its surface contents
			if (FAILED(hr = pScreenSurface->pdds->SetSurfaceDesc(
						   &pSprite->ddsd, 0))) {
				return hr;
			}
			return S_OK; // All done!  This DDS is ready to use.
		}
		// Otherwise, we can't share image data, and this system memory surface
		// will be for this Screen only.
	}

	// Copy image data from the Sprite to this ScreenSurface:
	HDC hdc;
	if (FAILED(hr = pScreenSurface->pdds->GetDC(&hdc))) {
		return hr;
	}

	DWORD dwWidth =
		static_cast<DWORD>(pSprite->rcSrc.right - pSprite->rcSrc.left);
	DWORD dwHeight =
		static_cast<DWORD>(pSprite->rcSrc.bottom - pSprite->rcSrc.top);
	HDC hdcImage = CreateCompatibleDC(NULL);
	HGDIOBJ hgdiobjOld = SelectObject(hdcImage, pSprite->hbmImage);
	StretchBlt(hdc, 0, 0, static_cast<int>(dwWidth), static_cast<int>(dwHeight),
		hdcImage, 0, 0, static_cast<int>(dwWidth), static_cast<int>(dwHeight),
		SRCCOPY);
	SelectObject(hdcImage, hgdiobjOld); // restore previously selected object
	DeleteDC(hdcImage);
	TextOutA(hdc, 0, 0, pSprite->szName, lstrlen(pSprite->szName));
	pScreenSurface->pdds->ReleaseDC(hdc);

	if (ddsd.ddsCaps.dwCaps == DDSCAPS_VIDEOMEMORY) {
		if (FAILED(hr = pScreenSurface->pdds->GetDC(&hdc))) {
			return hr;
		}
		sprintf(sz, "Video memory copy");
		TextOutA(hdc, 0, 20, sz, lstrlen(sz));
		sprintf(sz, "for %s", pScreen->szDesc);
		TextOutA(hdc, 0, 40, sz, lstrlen(sz));
		pScreenSurface->pdds->ReleaseDC(hdc);
	} else if (pSprite->pImageData == NULL) {
		// No shared copy exists yet, so create one using data in this
		// system memory surface.
		if (FAILED(hr = pScreenSurface->pdds->GetDC(&hdc))) {
			return hr;
		}
		sprintf(sz, "Shared System memory copy");
		TextOutA(hdc, 0, 20, sz, lstrlen(sz));
		pScreenSurface->pdds->ReleaseDC(hdc);

		// Copy image to pImageData so it can be shared among ScreenSurfaces:
		if (SUCCEEDED(hr = pScreenSurface->pdds->Lock(
						  NULL, &ddsd, DDLOCK_READONLY | DDLOCK_WAIT, NULL))) {
			pSprite->pImageData = new BYTE[ddsd.lPitch * ddsd.dwHeight];
			if (pSprite->pImageData != NULL) {
				// Store size, pitch, pixel format, and surface pointer info in
				// Sprite
				pSprite->ddsd = ddsd;
				pSprite->ddsd.lpSurface = pSprite->pImageData;
				pSprite->ddsd.dwFlags = DDSD_WIDTH | DDSD_HEIGHT | DDSD_PITCH |
					DDSD_PIXELFORMAT | DDSD_LPSURFACE;
				// Copy image data from DDS's surface memory to Sprite's buffer
				CopyMemory(pSprite->pImageData, ddsd.lpSurface,
					ddsd.lPitch * ddsd.dwHeight);
			}
			pScreenSurface->pdds->Unlock(NULL);

			if (pSprite->pImageData != NULL) {
				// May as well make this ScreenSurface use the sharable copy
				// too:
				if (FAILED(hr = pScreenSurface->pdds->SetSurfaceDesc(
							   &pSprite->ddsd, 0))) {
					return hr;
				}
			}
		}
	} else {
		// Shared copy exists, but attempt to use it failed (probably due to
		// mismatched pixel format), so indicate that this as a non-shared
		// sysmem copy:
		if (FAILED(hr = pScreenSurface->pdds->GetDC(&hdc))) {
			return hr;
		}
		sprintf(sz, "System memory copy");
		TextOutA(hdc, 0, 20, sz, lstrlen(sz));
		sprintf(sz, "for %s", pScreen->szDesc);
		TextOutA(hdc, 0, 40, sz, lstrlen(sz));
		pScreenSurface->pdds->ReleaseDC(hdc);
	}
	return S_OK;
}

//-----------------------------------------------------------------------------
// Name: FindOrBuildScreenSurface()
// Desc: This is called when UpdateFrame needs to draw the image of a Sprite
//       onto a particular Screen.  If a ScreenSurface already exists for this
//       Screen and Sprite, a pointer to it is returned.  Otherwise, a new
//       ScreenSurface is created (and stored for future reuse).
//-----------------------------------------------------------------------------
static HRESULT FindOrBuildScreenSurface(
	Sprite* pSprite, Screen* pScreen, ScreenSurface** ppScreenSurface)
{
	HRESULT hr;
	ScreenSurface* pScreenSurface;

	for (pScreenSurface = pSprite->pScreenSurfaceFirst; pScreenSurface != NULL;
		 pScreenSurface = pScreenSurface->pScreenSurfaceNext) {
		if (pScreenSurface->pScreen == pScreen) {
			// ScreenSurface exists for this Screen, so return a pointer to it
			*ppScreenSurface = pScreenSurface;
			return S_OK;
		}
	}

	// No ScreenSurface for this Screen exists yet, so build one.
	pScreenSurface = new ScreenSurface;
	if (pScreenSurface == NULL) {
		return E_OUTOFMEMORY;
	}
	ZeroMemory(pScreenSurface, sizeof(ScreenSurface));
	pScreenSurface->pScreen = pScreen;
	if (FAILED(hr = SetupScreenSurfaceDDS(pSprite, pScreenSurface))) {
		delete pScreenSurface;
		return hr;
	}

	// Insert this new ScreenSurface in the Sprite's list:
	pScreenSurface->pScreenSurfaceNext = pSprite->pScreenSurfaceFirst;
	pSprite->pScreenSurfaceFirst = pScreenSurface;
	*ppScreenSurface = pScreenSurface;

	return S_OK;
}

//-----------------------------------------------------------------------------
// Name: UpdateFrame()
// Desc: Renders one frame of animation, then updates the sprites for the next
//       frame.
//-----------------------------------------------------------------------------
static void UpdateFrame(VOID)
{
	HRESULT hr;
	Screen* pScreen;
	RECT rcScreen;
	DDBLTFX ddbltfx;

	for (pScreen = s_pScreenFirst; pScreen != NULL;
		 pScreen = pScreen->pScreenNext) {
		// Handle lost surfaces
		if (pScreen->pddsFront->IsLost() == DDERR_SURFACELOST) {
			// Though surface memory can be reaquired via RestoreAllSurfaces,
			// the image contents will be undefined.  So we destroy all the
			// ScreenSurfaces and let them be recreated as needed.  Calling
			// RestoreAllSurfaces takes care of the remaining surfaces (the
			// front and back buffers).
			DestroyScreenSurfaces(s_pSprite1);
			DestroyScreenSurfaces(s_pSprite2);
			hr = pScreen->pdd->RestoreAllSurfaces();
		}

		// Clear the back buffer for this Screen
		ZeroMemory(&ddbltfx, sizeof(ddbltfx));
		ddbltfx.dwSize = sizeof(ddbltfx);
		ddbltfx.dwFillColor = 0; // Black
		hr = pScreen->pddsBack->Blt(
			NULL, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAIT, &ddbltfx);

		// Draw first sprite
		if (RectIntersectsMonitor(
				&s_pSprite1->rcDest, pScreen->hmon, &rcScreen)) {
			ScreenSurface* pScreenSurface2;
			hr =
				FindOrBuildScreenSurface(s_pSprite1, pScreen, &pScreenSurface2);
			if (SUCCEEDED(hr)) {
				hr = pScreen->pddsBack->Blt(&rcScreen, pScreenSurface2->pdds,
					&s_pSprite1->rcSrc, DDBLT_WAIT, NULL);
			}
		}

		// Draw second sprite
		if (RectIntersectsMonitor(
				&s_pSprite2->rcDest, pScreen->hmon, &rcScreen)) {
			ScreenSurface* pScreenSurface;
			hr = FindOrBuildScreenSurface(s_pSprite2, pScreen, &pScreenSurface);
			if (SUCCEEDED(hr)) {
				hr = pScreen->pddsBack->Blt(&rcScreen, pScreenSurface->pdds,
					&s_pSprite2->rcSrc, DDBLT_WAIT, NULL);
			}
		}
	}

	// Flip all screens.  This is done in a separate loop to make the flips
	// happen as close together in time as possible
	for (pScreen = s_pScreenFirst; pScreen != NULL;
		 pScreen = pScreen->pScreenNext) {
		hr = pScreen->pddsFront->Flip(NULL, DDFLIP_WAIT);
	}

	// Animate Sprites for the next frame.  The sprites are bounced against the
	// virtual desktop, which may cause them to partially or totally disappear
	// if your screens are set up in a way that is non-rectangular.  Since the
	// animation is not the purpose of this demo, this simplified approach is
	// used.
	RECT rcDesktop;
	rcDesktop.left = GetSystemMetrics(SM_XVIRTUALSCREEN);
	rcDesktop.right = rcDesktop.left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
	rcDesktop.top = GetSystemMetrics(SM_YVIRTUALSCREEN);
	rcDesktop.bottom = rcDesktop.top + GetSystemMetrics(SM_CYVIRTUALSCREEN);

	// Animate first sprite
	OffsetRect(&s_pSprite1->rcDest, s_pSprite1->xVel, s_pSprite1->yVel);
	if (s_pSprite1->rcDest.right > rcDesktop.right ||
		s_pSprite1->rcDest.left < rcDesktop.left) {
		s_pSprite1->xVel = -s_pSprite1->xVel;
	}
	if (s_pSprite1->rcDest.bottom > rcDesktop.bottom ||
		s_pSprite1->rcDest.top < rcDesktop.top) {
		s_pSprite1->yVel = -s_pSprite1->yVel;
	}

	// Animate second sprite
	OffsetRect(&s_pSprite2->rcDest, s_pSprite2->xVel, s_pSprite2->yVel);
	if (s_pSprite2->rcDest.right > rcDesktop.right ||
		s_pSprite2->rcDest.left < rcDesktop.left) {
		s_pSprite2->xVel = -s_pSprite2->xVel;
	}
	if (s_pSprite2->rcDest.bottom > rcDesktop.bottom ||
		s_pSprite2->rcDest.top < rcDesktop.top) {
		s_pSprite2->yVel = -s_pSprite2->yVel;
	}
}

//-----------------------------------------------------------------------------
// Name: MainLoop()
// Desc: The main window message pump.  When the application is active (not
//       minimized), it uses PeekMessage so that UpdateFrame can be called
//       frequently once all window messages are handled.  When it is not
//       active, GetMessage is used instead to give more processing time to
//       other running programs.
//-----------------------------------------------------------------------------
static HRESULT MainLoop(VOID)
{
	MSG msg;

	for (;;) {
		BOOL bGotMsg;
		if (s_bActive) {
			bGotMsg = PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE);
		} else {
			bGotMsg = GetMessageA(&msg, NULL, 0, 0);
		}
		if (msg.message == WM_QUIT) {
			return S_OK;
		}

		if (bGotMsg) {
			TranslateMessage(&msg);
			DispatchMessageA(&msg);
		} else if (s_bActive) {
			UpdateFrame();
		}
	}
}

//-----------------------------------------------------------------------------
// Name: WinMain()
// Desc: Initializes the application, then starts the main application loop.
//-----------------------------------------------------------------------------
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE /* hPrevInstance */,
	LPSTR /* lpCmdLine */, INT /* nCmdShow */)
{
	HRESULT hr;

	if (FAILED(hr = CreateFocusWindow(hInstance))) {
		return 0;
	}

	if (FAILED(hr = EnumerateScreens())) {
		return 0;
	}

	if (FAILED(hr = InitScreens())) {
		return 0;
	}

	if (FAILED(hr = InitSprites())) {
		return 0;
	}

	if (FAILED(hr = MainLoop())) {
		return 0;
	}

	return 0;
}