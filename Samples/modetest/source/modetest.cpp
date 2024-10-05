//-----------------------------------------------------------------------------
// File: ModeTest.cpp
//
// Desc: This example demonstrates basic usage of the
// IDirectDraw7::StartModeTest
//   and IDirectDraw7::EvaluateMode methods. Together, these methods allow an
//   application to explore what display modes and refresh rates the monitor
//   connected to this display device is able to display, through a manual
//   user-controlled process. The application will present the UI that asks
//   the user if the current mode under test is being displayed correctly by
//   the monitor.
//
//   Applications should use these methods when they are interested in using
//   higher refresh rates.
//
//   The basic idea is that DirectDraw will setup a list of modes to be tested
//   (based on the list the app passed in), and then sequentially test them
//   under application control. The app starts the test process, and then
//   calls IDirectDraw7::EvaluateMode continuously. DirectDraw will take care
//   of setting the modes. All the app has to do is SetCooperativeLevel
//   beforehand, and then handle surface loss and drawing the UI that asks the
//   user if they can see the current mode under test. DirectDraw returns
//   enough information from IDirectDraw7::EvaluateMode to allow the app to
//   know when to do these things, and when to stop testing. The app can pass
//   a flag to IDirectDraw7::EvaluateMode if the user happened to say they
//   could see the mode corretly, which will cause DirectDraw to mark the mode
//   as good and move on. DirectDraw may also decide that time as run out and
//   give up on a certain mode.
//
//   DirectDraw uses information at its disposal from any automated means to
//   make the testing process as short as possible, and applications only need
//   to test modes they are interested in.
//
// Copyright (c) 1999 Microsoft Corp. All rights reserved.
//-----------------------------------------------------------------------------
#define STRICT
#define INITGUID

#include "ddmacros.h"

#include <windows.h>

#include "resource.h"
#include <commdlg.h>
#include <ddraw.h>
#include <initguid.h>
#include <stdio.h>

#if (_MSC_VER >= 1900)
// Symbol is never tested for nullness
#pragma warning(disable : 26429)

// Symbol is not tested for nullness
#pragma warning(disable : 26430)

// Prefer to use gsl::at() instead of unchecked subscript
#pragma warning(disable : 26446)

// Don't use static_cast
#pragma warning(disable : 26472)

// Only index into arrays using constant expressions
#pragma warning(disable : 26482)

// No array to pointer decay
#pragma warning(disable : 26485)

// Don't use reinterpret_cast
#pragma warning(disable : 26490)
#endif

//-----------------------------------------------------------------------------
// Defines, constants, and global variables
//-----------------------------------------------------------------------------

struct DDRAW_DEVICE_STRUCT {
	GUID guid;
	CHAR strDescription[256];
	CHAR strDriverName[64];
	DWORD dwModeCount;
	SIZE aModeSize[256];
};

HWND g_hDlg = NULL;
LPDIRECTDRAW g_pDD = NULL;

DDRAW_DEVICE_STRUCT g_aDevices[16];
DWORD g_dwDeviceCount;

//-----------------------------------------------------------------------------
// Name: SetupPrimarySurface()
// Desc: Setups a primary DirectDraw surface
//-----------------------------------------------------------------------------
static HRESULT SetupPrimarySurface(
	LPDIRECTDRAW7 pDD, LPDIRECTDRAWSURFACE7* ppDDS)
{
	DDSURFACEDESC2 ddsd;

	ZeroMemory(&ddsd, sizeof(ddsd));

	ddsd.dwSize = sizeof(ddsd);
	ddsd.dwFlags = DDSD_CAPS;
	ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;

	return pDD->CreateSurface(&ddsd, ppDDS, NULL);
}

//-----------------------------------------------------------------------------
// Name: UpdatePrimarySurface()
// Desc: Fills the primary surface with white, and diplays the timeout value
//       on screen
//-----------------------------------------------------------------------------
static HRESULT UpdatePrimarySurface(LPDIRECTDRAWSURFACE7 pDDS, DWORD dwTimeout)
{
	DDBLTFX ddbltfx;

	char strTimeout[128];

	// Clear the screen:
	ZeroMemory(&ddbltfx, sizeof(ddbltfx));
	ddbltfx.dwSize = sizeof(ddbltfx);
	ddbltfx.dwFillColor = 0xFFFFFFFF;

	HRESULT hr =
		pDDS->Blt(NULL, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAIT, &ddbltfx);
	if (FAILED(hr)) {
		return hr;
	}

	// Display the timeout value
	HDC hDC;
	if (FAILED(hr = pDDS->GetDC(&hDC))) {
		return hr;
	}

	RECT rect;
	GetWindowRect(g_hDlg, &rect);
	sprintf(strTimeout,
		"Press space to accept or escape to reject. "
		"%2d seconds until timeout",
		static_cast<int>(dwTimeout));
	DrawTextA(hDC, strTimeout, static_cast<int>(strlen(strTimeout)), &rect,
		DT_CENTER | DT_VCENTER | DT_SINGLELINE);

	// Cleanup
	pDDS->ReleaseDC(hDC);

	return S_OK;
}

//-----------------------------------------------------------------------------
// Name: EnumAllModesCallback()
// Desc: For each mode enumerated, it adds it to the "All Modes" listbox.
//-----------------------------------------------------------------------------
static HRESULT WINAPI EnumAllModesCallback(
	LPDDSURFACEDESC2 pddsd, LPVOID pContext)
{
	char strMode[64];

	HWND hWnd = static_cast<HWND>(pContext);
	sprintf(strMode, "%ux%ux%u - %u Hz", pddsd->dwWidth, pddsd->dwHeight,
		pddsd->ddpfPixelFormat.dwRGBBitCount, pddsd->dwRefreshRate);

	SendMessageA(hWnd, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(strMode));

	return TRUE;
}

//-----------------------------------------------------------------------------
// Name: UpdateModesListBox()
// Desc: Updates the "modes to test" and "all modes" list boxes
//-----------------------------------------------------------------------------
static HRESULT UpdateModesListBox(DWORD dwDeviceIndex)
{
	LPDIRECTDRAW7 pDD = NULL;
	HRESULT hr;

	HWND hWndModesToTest = GetDlgItem(g_hDlg, IDC_TEST_MODES_LIST);
	SendMessageA(hWndModesToTest, LB_RESETCONTENT, 0, 0);

	// Update the "modes to test" list box based on the display device selected
	DWORD i;
	for (i = 0; i < g_aDevices[dwDeviceIndex].dwModeCount; i++) {
		char strMode[64];

		// Make a string based on the this mode's size
		sprintf(strMode, "%u x %u",
			static_cast<unsigned int>(
				g_aDevices[dwDeviceIndex].aModeSize[i].cx),
			static_cast<unsigned int>(
				g_aDevices[dwDeviceIndex].aModeSize[i].cy));

		// Add it to the list box
		SendMessageA(hWndModesToTest, LB_ADDSTRING, 0,
			reinterpret_cast<LPARAM>(strMode));
		SendMessageA(hWndModesToTest, LB_SETITEMDATA, i,
			static_cast<LPARAM>(static_cast<int>(i)));
	}

	// Create a DirectDraw device based using the selected device guid
	if (SUCCEEDED(hr = DirectDrawCreateEx(&g_aDevices[dwDeviceIndex].guid,
					  (VOID**)&pDD, IID_IDirectDraw7, NULL))) {
		HWND hWndAllModes = GetDlgItem(g_hDlg, IDC_ALL_MODES_LIST);
		SendMessageA(hWndAllModes, LB_RESETCONTENT, 0, 0);

		// Enumerate and display all supported modes along
		// with supported bit depth, and refresh rates
		// in the "All Modes" listbox
		hr = pDD->EnumDisplayModes(
			DDEDM_REFRESHRATES, NULL, hWndAllModes, EnumAllModesCallback);

		// Release this device
		SAFE_RELEASE(pDD);
	}

	return hr;
}

//-----------------------------------------------------------------------------
// Name: OnInitDialog()
// Desc: Initializes the dialogs (sets up UI controls, etc.)
//-----------------------------------------------------------------------------
static VOID OnInitDialog()
{
	// Load the icon
	HINSTANCE hInst =
		reinterpret_cast<HINSTANCE>(GetWindowLongPtrA(g_hDlg, GWLP_HINSTANCE));
	HICON hIcon = LoadIconA(hInst, MAKEINTRESOURCE(IDI_ICON1));

	// Set the icon for this dialog.
	// Set big icon
	PostMessageA(g_hDlg, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(hIcon));

	// Set small icon
	PostMessageA(
		g_hDlg, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(hIcon));

	// Show all available DirectDraw devices in the listbox
	HWND hWndDeviceList = GetDlgItem(g_hDlg, IDC_DDRAW_DEVICE_LIST);
	UINT i;
	for (i = 0; i < g_dwDeviceCount; i++) {
		SendMessageA(hWndDeviceList, LB_ADDSTRING, 0,
			reinterpret_cast<LPARAM>(g_aDevices[i].strDescription));
		SendMessageA(hWndDeviceList, LB_SETITEMDATA, i,
			static_cast<LPARAM>(static_cast<int>(i)));
	}

	// Select the first device by default
	const DWORD dwCurrentSelect = 0;

	SendMessageA(hWndDeviceList, LB_SETCURSEL, dwCurrentSelect, 0);
	if (FAILED(UpdateModesListBox(dwCurrentSelect))) {
		MessageBoxA(g_hDlg,
			"Error enumerating DirectDraw modes. "
			"Sample will now exit.",
			"DirectDraw Sample", MB_OK | MB_ICONERROR);
		EndDialog(g_hDlg, IDABORT);
	}

	SetFocus(hWndDeviceList);
}

//-----------------------------------------------------------------------------
// Name: PerformDirectDrawModeTest()
// Desc: Perform the IDirectDraw7::StartModeTest and IDirectDraw7::EvaluateMode
//       tests
// Returns: S_OK if no modes needed testing, or all modes tested successfully,
//          informative error code otherwise.
//-----------------------------------------------------------------------------
static HRESULT PerformDirectDrawModeTest(
	LPDIRECTDRAW7 pDD, SIZE* aTestModes, DWORD dwTestModes)
{
	LPDIRECTDRAWSURFACE7 pDDSPrimary = NULL;
	MSG msg;
	DWORD dwFlags = 0;
	DWORD dwTimeout;
	BOOL bMsgReady;

	// First call StartModeTest with the DDSMT_ISTESTREQUIRED flag to determine
	// whether the tests can be performed and need to be performed.
	HRESULT hr =
		pDD->StartModeTest(aTestModes, dwTestModes, DDSMT_ISTESTREQUIRED);

	switch (hr) {
	case DDERR_NEWMODE:
		// DDERR_NEWMODE means that there are modes that need testing.
		break;
	case DDERR_TESTFINISHED:
		// DDERR_TESTFINISHED means that all the modes that we wish to test have
		// already been tested correctly
		return S_OK;
	default:
		// Possible return codes here include DDERR_NOMONITORINFORMATION or
		// DDERR_NODRIVERSUPPORT or other fatal error codes
		// (DDERR_INVALIDPARAMS, DDERR_NOEXCLUSIVEMODE, etc.)
		return hr;
	}

	hr = pDD->StartModeTest(aTestModes, dwTestModes, 0);
	if (hr == DDERR_TESTFINISHED) {
		// The tests completed early, so return
		return S_OK;
	}

	// Create the primary DirectDraw surface
	if (FAILED(SetupPrimarySurface(pDD, &pDDSPrimary))) {
		return hr;
	}

	// Loop until the mode tests are complete
	for (;;) {
		bMsgReady = PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE);

		if (bMsgReady) {
			if (msg.message == WM_KEYDOWN) {
				switch (msg.wParam) {
				case VK_SPACE:
					dwFlags = DDEM_MODEPASSED;
					break;

				case VK_ESCAPE:
					dwFlags = DDEM_MODEFAILED;
					break;
				default:
					break;
				}
			} else {
				TranslateMessage(&msg);
				DispatchMessageA(&msg);
			}
		} else {
			// This method will only succeed with monitors that contain EDID
			// data. If the monitor is not EDID compliant, then the method will
			// return DDERR_TESTFINISHED without testing any modes. If the EDID
			// table does not contain values higher than 60hz, no modes will
			// tested. Refresh rates higher than 100 hz will only be tested if
			// the EDID table contains values higher than 85hz.
			hr = pDD->EvaluateMode(dwFlags, &dwTimeout);

			if (hr == DD_OK) {
				if (pDDSPrimary) {
					// Clear the screen, and display the timeout value
					UpdatePrimarySurface(pDDSPrimary, dwTimeout);
				}
			} else if (hr == DDERR_NEWMODE) {
				// Cleanup the last DirectDraw surface, and create
				// a new one for the new mode
				SAFE_RELEASE(pDDSPrimary);

				if (FAILED(SetupPrimarySurface(pDD, &pDDSPrimary))) {
					return hr;
				}

				dwFlags = 0;
			} else if (hr == DDERR_TESTFINISHED) {
				// Test complete, so stop looping
				break;
			}

			Sleep(100);
		}
	}

	// Cleanup
	SAFE_RELEASE(pDDSPrimary);

	return S_OK;
}

//-----------------------------------------------------------------------------
// Name: OnModeTest()
// Desc: User hit the "Test" button
//-----------------------------------------------------------------------------
static HRESULT OnModeTest()
{
	HWND hWndModesToTest = GetDlgItem(g_hDlg, IDC_TEST_MODES_LIST);
	const LRESULT dwSelectCount =
		SendMessageA(hWndModesToTest, LB_GETSELCOUNT, 0, 0);

	if (dwSelectCount > 0) {
		LPDIRECTDRAW7 pDD = NULL;
		HRESULT hr;

		// Get the currently selected DirectDraw device
		HWND hWndDeviceList = GetDlgItem(g_hDlg, IDC_DDRAW_DEVICE_LIST);
		LRESULT dwDeviceIndex =
			SendMessageA(hWndDeviceList, LB_GETCURSEL, 0, 0);

		// Create a DirectDraw device based using the selected device guid
		if (FAILED(hr = DirectDrawCreateEx(&g_aDevices[dwDeviceIndex].guid,
					   (VOID**)&pDD, IID_IDirectDraw7, NULL))) {
			return hr;
		}
		// This is a good usage of DDSCL_CREATEDEVICEWINDOW: DirectDraw will
		// create a window that covers the monitor, and won't mess around with
		// our dialog box. Any mouse clicks on the cover window will therefore
		// not be received and misinterpreted by the dialog box, since such
		// clicks will be sent to DirectDraw's internal message procedure and
		// therein ignored.

		if (FAILED(hr = pDD->SetCooperativeLevel(g_hDlg,
					   DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN |
						   DDSCL_CREATEDEVICEWINDOW | DDSCL_SETFOCUSWINDOW))) {
			SAFE_RELEASE(pDD);
			return hr;
		}

		SIZE aTestModes[256];
		DWORD dwTestModes = 0;

		// Find out which modes are selected, then just test those
		DWORD i;
		for (i = 0; i < g_aDevices[dwDeviceIndex].dwModeCount; i++) {
			if (SendMessageA(hWndModesToTest, LB_GETSEL, i, 0)) {
				// Record the selected modes in aTestModes[]
				aTestModes[dwTestModes] =
					g_aDevices[dwDeviceIndex].aModeSize[i];
				dwTestModes++;
			}
		}

		// Perform test on each of the selected modes on the selected device
		hr = PerformDirectDrawModeTest(pDD, aTestModes, dwTestModes);

		// Release this device
		SAFE_RELEASE(pDD);

		switch (hr) {
		case DDERR_NOMONITORINFORMATION:
			// No EDID data is present for the current monitor.
			MessageBoxA(g_hDlg,
				"The current monitor cannot be identified electronically.\n"
				"High refresh rates are not allowed on such monitors, so the test will not be performed.",
				"Testing Will Not Proceed", MB_OK | MB_ICONINFORMATION);
			break;
		case DDERR_NODRIVERSUPPORT:
			// The driver cannot support refresh rate testing.
			MessageBoxA(g_hDlg,
				"The driver does not support specific refresh rates. Test cannot be performed.",
				"Testing Cannot Proceed", MB_OK | MB_ICONINFORMATION);
			break;
		default:
			if (SUCCEEDED(hr)) {
				MessageBoxA(g_hDlg, "Mode test completed", "Result", MB_OK);
				break;
			} else {
				// A StartModeTest error occurred.
				MessageBoxA(g_hDlg,
					"StartModeTest returned an unexpected value when called with the DDSMT_ISTESTREQUIRED flag.",
					"StartModeTest Error", MB_OK | MB_ICONEXCLAMATION);
				return hr;
			}
		}

		// Update the mode list boxes based on the device selected
		if (FAILED(
				hr = UpdateModesListBox(static_cast<DWORD>(dwDeviceIndex)))) {
			return hr;
		}
	} else {
		// There weren't any modes selected to test
		MessageBoxA(g_hDlg,
			"Select one or more modes to test from the list box",
			"No modes selected", MB_OK);
	}

	return S_OK;
}

//-----------------------------------------------------------------------------
// Name: ResetDeviceModes()
// Desc: If the user makes a mistake, and accidently says YES when a mode
//       cannot be seen, or NO when a mode really can be seen (thus ending up
//       with a lower refresh rate than possible) this allows the user to reset
//       the test results and try again.
//-----------------------------------------------------------------------------
static HRESULT ResetDeviceModes(DWORD dwDeviceIndex)
{
	LPDIRECTDRAW7 pDD = NULL;

	// Create a DirectDraw device based using the selected device guid
	HRESULT hr = DirectDrawCreateEx(&g_aDevices[dwDeviceIndex].guid,
		reinterpret_cast<VOID**>(&pDD), IID_IDirectDraw7, NULL);

	if (SUCCEEDED(hr)) {
		// Set the cooperative level to normal
		if (SUCCEEDED(hr = pDD->SetCooperativeLevel(g_hDlg, DDSCL_NORMAL))) {
			// Clear the previous mode tests
			//
			// We ignore the return code, since we would do nothing different on
			// error returns. Note that most applications would never need to
			// call StartModeTest this way. The reset functionality is intended
			// to be used when a user accidentally accepted a mode that didn't
			// actually display correctly.
			pDD->StartModeTest(NULL, 0, 0);
		}

		// Release this device
		SAFE_RELEASE(pDD);
	}

	hr = UpdateModesListBox(dwDeviceIndex);

	return hr;
}

//-----------------------------------------------------------------------------
// Name: EnumModesCallback()
// Desc: Enumerates the available modes for the device from which
//       EnumDisplayModes() was called.  It records the unique mode sizes in
//       the g_aDevices[g_dwDeviceCount].aModeSize array
//-----------------------------------------------------------------------------
static HRESULT WINAPI EnumModesCallback(
	LPDDSURFACEDESC pddsd, LPVOID /* pContext */)
{
	// For each mode, look through all previously found modes
	// to see if this mode has already been added to the list
	DWORD dwModeCount = g_aDevices[g_dwDeviceCount].dwModeCount;

	DWORD i;
	for (i = 0; i < dwModeCount; i++) {
		DWORD dwModeSizeX =
			static_cast<DWORD>(g_aDevices[g_dwDeviceCount].aModeSize[i].cx);
		DWORD dwModeSizeY =
			static_cast<DWORD>(g_aDevices[g_dwDeviceCount].aModeSize[i].cy);

		if ((dwModeSizeX == pddsd->dwWidth) &&
			(dwModeSizeY == pddsd->dwHeight)) {
			// If this mode has been added, then stop looking
			break;
		}
	}

	// If this mode was not in g_aDevices[g_dwDeviceCount].aModeSize[]
	// then added it.
	if (i == g_aDevices[g_dwDeviceCount].dwModeCount) {
		g_aDevices[g_dwDeviceCount].aModeSize[i].cx =
			static_cast<LONG>(pddsd->dwWidth);
		g_aDevices[g_dwDeviceCount].aModeSize[i].cy =
			static_cast<LONG>(pddsd->dwHeight);

		// Increase the number of modes found for this device
		g_aDevices[g_dwDeviceCount].dwModeCount++;
	}

	return TRUE;
}

//-----------------------------------------------------------------------------
// Name: DDEnumCallbackEx()
// Desc: Enumerates all available DirectDraw devices
//-----------------------------------------------------------------------------
static BOOL WINAPI DDEnumCallbackEx(GUID* pGUID, LPSTR strDriverDescription,
	LPSTR strDriverName, LPVOID /* pContext */, HMONITOR /* hm */)
{
	LPDIRECTDRAW pDD = NULL;

	// Create a DirectDraw device using the enumerated guid
	HRESULT hr = DirectDrawCreateEx(
		pGUID, reinterpret_cast<VOID**>(&pDD), IID_IDirectDraw7, NULL);

	if (SUCCEEDED(hr)) {
		if (pGUID) {
			// Add it to the global storage structure
			g_aDevices[g_dwDeviceCount].guid = *pGUID;
		} else {
			// Clear the guid from the global storage structure
			ZeroMemory(&g_aDevices[g_dwDeviceCount].guid, sizeof(GUID));
		}

		// Copy the description of the driver into the structure
		lstrcpyn(g_aDevices[g_dwDeviceCount].strDescription,
			strDriverDescription, 256);
		lstrcpyn(g_aDevices[g_dwDeviceCount].strDriverName, strDriverName, 64);

		// Retrive the modes this device can support
		g_aDevices[g_dwDeviceCount].dwModeCount = 0;
		hr = pDD->EnumDisplayModes(0, NULL, NULL, EnumModesCallback);

		// Increase the counter for the number of devices found
		g_dwDeviceCount++;

		// Release this device
		SAFE_RELEASE(pDD);
	}

	// Continue looking for more devices
	return TRUE;
}

//-----------------------------------------------------------------------------
// Name: GetDirectDrawDevices()
// Desc: Retrieves all available DirectDraw devices and stores the information
//       in g_aDevices[]
//-----------------------------------------------------------------------------
static HRESULT GetDirectDrawDevices()
{
	return DirectDrawEnumerateEx(DDEnumCallbackEx, NULL,
		DDENUM_ATTACHEDSECONDARYDEVICES | DDENUM_DETACHEDSECONDARYDEVICES |
			DDENUM_NONDISPLAYDEVICES);
}

//-----------------------------------------------------------------------------
// Name: MainDlgProc()
// Desc: Handles dialog messages
//-----------------------------------------------------------------------------
static INT_PTR CALLBACK MainDlgProc(
	HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
	case WM_INITDIALOG:
		// Store HWND in global
		g_hDlg = hDlg;

		OnInitDialog();
		break;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDCANCEL:
			EndDialog(hDlg, IDCANCEL);
			break;

		case IDC_TEST:
			if (FAILED(OnModeTest())) {
				MessageBoxA(g_hDlg,
					"Error doing starting mode test. "
					"Sample will now exit.",
					"DirectDraw Sample", MB_OK | MB_ICONERROR);
				EndDialog(g_hDlg, IDABORT);
			}
			break;

		case IDC_RESET:

			// Get the currently selected DirectDraw device
			{
				HWND hWndDeviceList = GetDlgItem(hDlg, IDC_DDRAW_DEVICE_LIST);
				const LRESULT dwDeviceIndex =
					SendMessageA(hWndDeviceList, LB_GETCURSEL, 0, 0);

				// Reset the modes for it
				if (FAILED(
						ResetDeviceModes(static_cast<DWORD>(dwDeviceIndex)))) {
					MessageBoxA(g_hDlg,
						"Error reset DirectDraw device. "
						"Sample will now exit.",
						"DirectDraw Sample", MB_OK | MB_ICONERROR);
					EndDialog(g_hDlg, IDABORT);
				}
			}
			break;

		case IDC_DDRAW_DEVICE_LIST:
			switch (HIWORD(wParam)) {
			case LBN_SELCHANGE: {
				// Get the currently selected DirectDraw device
				const LRESULT dwDeviceIndex2 = SendMessageA(
					reinterpret_cast<HWND>(lParam), LB_GETCURSEL, 0, 0);

				// Update the list boxes using it
				if (FAILED(UpdateModesListBox(
						static_cast<DWORD>(dwDeviceIndex2)))) {
					MessageBoxA(g_hDlg,
						"Error enumerating DirectDraw modes."
						"Sample will now exit.",
						"DirectDraw Sample", MB_OK | MB_ICONERROR);
					EndDialog(g_hDlg, IDABORT);
				}
			} break;

			default:
				return FALSE;
			}
			break;

		default:
			return FALSE; // Didn't handle message
		}
		break;

	default:
		return FALSE; // Didn't handle message
	}

	return TRUE; // Handled message
}

//-----------------------------------------------------------------------------
// Name: WinMain()
// Desc: Entry point for the application.  Since we use a simple dialog for
//       user interaction we don't need to pump messages.
//-----------------------------------------------------------------------------
int APIENTRY WinMain(HINSTANCE hInst, HINSTANCE /* hPrevInst */,
	LPSTR /* pCmdLine */, int /* nCmdShow */)
{
	if (FAILED(GetDirectDrawDevices())) {
		return FALSE;
	}

	// Display the main dialog box.
	DialogBoxA(hInst, MAKEINTRESOURCE(IDD_MAIN), NULL, MainDlgProc);

	return TRUE;
}
