/*==========================================================================
 *
 *  Copyright (C) 1995-1999 Microsoft Corporation. All Rights Reserved.
 *
 *  File:       input.c
 *  Content:    Input routines for Space Donuts game
 *
 *
 ***************************************************************************/

#include "input.h"
#include "resource.h"

// Needed for Code Warrior
#if !defined(DIDFT_OPTIONAL)
#define DIDFT_OPTIONAL 0x80000000
#endif

typedef HRESULT(WINAPI* LPDirectInput8Create)(
	HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);

extern HWND hWndMain;
extern DWORD ReadKeyboardInput(void);
extern DWORD ReadJoystickInput(void);

static HMODULE g_hDirectInputModule;

// allocate external variables
DWORD (*ReadGameInput)(void) = ReadKeyboardInput;

/*
 *  We'll use up to the first ten input devices.
 *
 *  c_cpdiFound is the number of found devices.
 *  g_rgpdiFound[0] is the array of found devices.
 *  g_pdevCurrent is the device that we are using for input.
 */
#define MAX_DINPUT_DEVICES 10
int g_cpdevFound;
LPDIRECTINPUTDEVICE8A g_rgpdevFound[MAX_DINPUT_DEVICES];
LPDIRECTINPUTDEVICE8A g_pdevCurrent;

/*--------------------------------------------------------------------------
| AddInputDevice
|
| Records an input device in the array of found devices.
|
| In addition to stashing it in the array, we also add it to the device
| menu so the user can pick it.
|
*-------------------------------------------------------------------------*/

static void AddInputDevice(LPDIRECTINPUTDEVICE pdev, LPCDIDEVICEINSTANCE pdi)
{

	if (g_cpdevFound < MAX_DINPUT_DEVICES) {

		/*
		 *  Convert it to a Device2 so we can Poll() it.
		 */

		HRESULT hRes = pdev->QueryInterface(
			IID_IDirectInputDevice8A, (LPVOID*)&g_rgpdevFound[g_cpdevFound]);

		if (SUCCEEDED(hRes)) {

			/*
			 *  Add its description to the menu.
			 */
			HMENU hmenu = GetSubMenu(GetMenu(hWndMain), 0);

			InsertMenuA(hmenu, static_cast<UINT>(g_cpdevFound),
				MF_BYPOSITION | MF_STRING,
				static_cast<UINT>(IDC_DEVICES + g_cpdevFound),
				pdi->tszInstanceName);

			g_cpdevFound++;
		}
	}
}

/*--------------------------------------------------------------------------
|
| SetDIDwordProperty
|
| Set a DWORD property on a DirectInputDevice.
|
*-------------------------------------------------------------------------*/

static HRESULT SetDIDwordProperty(LPDIRECTINPUTDEVICE pdev,
	REFGUID guidProperty, DWORD dwObject, DWORD dwHow, DWORD dwValue)
{
	DIPROPDWORD dipdw;

	dipdw.diph.dwSize = sizeof(dipdw);
	dipdw.diph.dwHeaderSize = sizeof(dipdw.diph);
	dipdw.diph.dwObj = dwObject;
	dipdw.diph.dwHow = dwHow;
	dipdw.dwData = dwValue;

	return pdev->SetProperty(guidProperty, &dipdw.diph);
}

/*--------------------------------------------------------------------------
| InitKeyboardInput
|
| Initializes DirectInput for the keyboard.  Creates a keyboard device,
| sets the data format to our custom format, sets the cooperative level and
| adds it to the menu.
|
*-------------------------------------------------------------------------*/

static BOOL InitKeyboardInput(IDirectInput* pdi)
{
	LPDIRECTINPUTDEVICE pdev;
	DIDEVICEINSTANCE di;

	// create the DirectInput keyboard device
	if (pdi->CreateDevice(GUID_SysKeyboard, &pdev, NULL) != DI_OK) {
		OutputDebugStringA("IDirectInput::CreateDevice FAILED\n");
		return FALSE;
	}

	// set keyboard data format
	if (pdev->SetDataFormat(&c_dfDIKeyboard) != DI_OK) {
		OutputDebugStringA("IDirectInputDevice::SetDataFormat FAILED\n");
		pdev->Release();
		return FALSE;
	}

	// set the cooperative level
	if (pdev->SetCooperativeLevel(
			hWndMain, DISCL_NONEXCLUSIVE | DISCL_FOREGROUND) != DI_OK) {
		OutputDebugStringA("IDirectInputDevice::SetCooperativeLevel FAILED\n");
		pdev->Release();
		return FALSE;
	}

	// set buffer size
	if (SetDIDwordProperty(
			pdev, DIPROP_BUFFERSIZE, 0, DIPH_DEVICE, KEYBUFSIZE) != DI_OK) {
		OutputDebugStringA(
			"IDirectInputDevice::SetProperty(DIPH_DEVICE) FAILED\n");
		pdev->Release();
		return FALSE;
	}

	//
	// Get the name of the primary keyboard so we can add it to the menu.
	//
	di.dwSize = sizeof(di);
	if (pdev->GetDeviceInfo(&di) != DI_OK) {
		OutputDebugStringA("IDirectInputDevice::GetDeviceInfo FAILED\n");
		pdev->Release();
		return FALSE;
	}

	//
	// Add it to our list of devices.  If AddInputDevice succeeds,
	// he will do an AddRef.
	//
	AddInputDevice(pdev, &di);
	pdev->Release();

	return TRUE;
}

/*--------------------------------------------------------------------------
| InitJoystickInput
|
| Initializes DirectInput for an enumerated joystick device.
| Creates the device, device, sets the standard joystick data format,
| sets the cooperative level and adds it to the menu.
|
| If any step fails, just skip the device and go on to the next one.
|
*-------------------------------------------------------------------------*/

static BOOL FAR PASCAL InitJoystickInput(
	LPCDIDEVICEINSTANCE pdinst, LPVOID pvRef)
{
	IDirectInput* pdi = static_cast<IDirectInput*>(pvRef);
	LPDIRECTINPUTDEVICE pdev;
	DIPROPRANGE diprg;

	// create the DirectInput joystick device
	if (pdi->CreateDevice(pdinst->guidInstance, &pdev, NULL) != DI_OK) {
		OutputDebugStringA("IDirectInput::CreateDevice FAILED\n");
		return DIENUM_CONTINUE;
	}

	// set joystick data format
	if (pdev->SetDataFormat(&c_dfDIJoystick) != DI_OK) {
		OutputDebugStringA("IDirectInputDevice::SetDataFormat FAILED\n");
		pdev->Release();
		return DIENUM_CONTINUE;
	}

	// set the cooperative level
	if (pdev->SetCooperativeLevel(
			hWndMain, DISCL_NONEXCLUSIVE | DISCL_FOREGROUND) != DI_OK) {
		OutputDebugStringA("IDirectInputDevice::SetCooperativeLevel FAILED\n");
		pdev->Release();
		return DIENUM_CONTINUE;
	}

	// set X-axis range to (-1000 ... +1000)
	// This lets us test against 0 to see which way the stick is pointed.

	diprg.diph.dwSize = sizeof(diprg);
	diprg.diph.dwHeaderSize = sizeof(diprg.diph);
	diprg.diph.dwObj = DIJOFS_X;
	diprg.diph.dwHow = DIPH_BYOFFSET;
	diprg.lMin = -1000;
	diprg.lMax = +1000;

	if (pdev->SetProperty(DIPROP_RANGE, &diprg.diph) != DI_OK) {
		OutputDebugStringA(
			"IDirectInputDevice::SetProperty(DIPH_RANGE) FAILED\n");
		pdev->Release();
		return FALSE;
	}

	//
	// And again for Y-axis range
	//
	diprg.diph.dwObj = DIJOFS_Y;

	if (pdev->SetProperty(DIPROP_RANGE, &diprg.diph) != DI_OK) {
		OutputDebugStringA(
			"IDirectInputDevice::SetProperty(DIPH_RANGE) FAILED\n");
		pdev->Release();
		return FALSE;
	}

	// set X axis dead zone to 50% (to avoid accidental turning)
	// Units are ten thousandths, so 50% = 5000/10000.
	if (SetDIDwordProperty(
			pdev, DIPROP_DEADZONE, DIJOFS_X, DIPH_BYOFFSET, 5000) != DI_OK) {
		OutputDebugStringA(
			"IDirectInputDevice::SetProperty(DIPH_DEADZONE) FAILED\n");
		pdev->Release();
		return FALSE;
	}

	// set Y axis dead zone to 50% (to avoid accidental thrust)
	// Units are ten thousandths, so 50% = 5000/10000.
	if (SetDIDwordProperty(
			pdev, DIPROP_DEADZONE, DIJOFS_Y, DIPH_BYOFFSET, 5000) != DI_OK) {
		OutputDebugStringA(
			"IDirectInputDevice::SetProperty(DIPH_DEADZONE) FAILED\n");
		pdev->Release();
		return FALSE;
	}

	//
	// Add it to our list of devices.  If AddInputDevice succeeds,
	// he will do an AddRef.
	//
	AddInputDevice(pdev, pdinst);
	pdev->Release();

	return DIENUM_CONTINUE;
}

/*--------------------------------------------------------------------------
| InitInput
|
| Initializes DirectInput for the keyboard and all joysticks.
|
| For each input device, add it to the menu.
|
*-------------------------------------------------------------------------*/

BOOL InitInput(HINSTANCE hInst, HWND /* hWnd */)
{
	IDirectInput* pdi;

	// Find dinput8.dll
	g_hDirectInputModule = LoadLibraryA("dinput8.dll");
	if (!g_hDirectInputModule) {
		OutputDebugStringA("Unable to load dinput8.dll\n");
		return FALSE;
	}

	// Get the function pointer
	LPDirectInput8Create pDirectInput8Create =
		(LPDirectInput8Create)GetProcAddress(
			g_hDirectInputModule, "DirectInput8Create");
	if (!pDirectInput8Create) {
		OutputDebugStringA(
			"Unable to find DirectInput8Create, DirectInput 8 required\n");
		return FALSE;
	}

	// create the DirectInput interface object
	if (pDirectInput8Create(
			hInst, 0x0800, IID_IDirectInput8A, (void**)&pdi, NULL) != DI_OK) {
		OutputDebugStringA("DirectInput8Create() FAILED\n");
		return FALSE;
	}

	BOOL fRc = InitKeyboardInput(pdi);
	if (!fRc) {
		OutputDebugStringA("InitKeyboardInput FAILED\n");
		pdi->Release(); // Finished with DX 3.0
		return FALSE;
	}

	//
	// Enumerate the joystick devices.  If it doesn't work, oh well,
	// at least we got the keyboard.
	//

	pdi->EnumDevices(
		DI8DEVCLASS_GAMECTRL, InitJoystickInput, pdi, DIEDFL_ATTACHEDONLY);

	pdi->Release(); // Finished with DX 5.0.

	// Default device is the keyboard
	PickInputDevice(0);

	// if we get here, we were successful
	return TRUE;
}

/*--------------------------------------------------------------------------
| CleanupInput
|
| Cleans up all DirectInput objects.
*-------------------------------------------------------------------------*/
void CleanupInput(void)
{
	// make sure the device is unacquired
	// it doesn't harm to unacquire a device that isn't acquired

	if (g_pdevCurrent) {
		g_pdevCurrent->Unacquire();
	}

	// release all the devices we created
	int idev;
	for (idev = 0; idev < g_cpdevFound; idev++) {
		if (g_rgpdevFound[idev]) {
			g_rgpdevFound[idev]->Release();
			g_rgpdevFound[idev] = 0;
		}
	}

	// Release Dinput8.dll
	if (g_hDirectInputModule) {
		FreeLibrary(g_hDirectInputModule);
		g_hDirectInputModule = NULL;
	}
}

/*--------------------------------------------------------------------------
| ReacquireInput
|
| Reacquires the current input device.  If Acquire() returns S_FALSE,
| that means
| that we are already acquired and that DirectInput did nothing.
*-------------------------------------------------------------------------*/
BOOL ReacquireInput(void)
{
	// if we have a current device
	if (g_pdevCurrent) {
		// acquire the device
		HRESULT hRes = g_pdevCurrent->Acquire();
		if (SUCCEEDED(hRes)) {
			// acquisition successful
			return TRUE;
		}
		// acquisition failed
		return FALSE;
	}
	// we don't have a current device
	return FALSE;
}

/*--------------------------------------------------------------------------
| ReadKeyboardInput
|
| Requests keyboard data and performs any needed processing.
*-------------------------------------------------------------------------*/
DWORD ReadKeyboardInput(void)
{
	DIDEVICEOBJECTDATA rgKeyData[KEYBUFSIZE];
	static DWORD dwKeyState = 0;

	// get data from the keyboard
	DWORD dwEvents = KEYBUFSIZE;
	HRESULT hRes = g_pdevCurrent->GetDeviceData(
		sizeof(DIDEVICEOBJECTDATA), rgKeyData, &dwEvents, 0);

	if (hRes != DI_OK) {
		// did the read fail because we lost input for some reason?
		// if so, then attempt to reacquire.  If the second acquire
		// fails, then the error from GetDeviceData will be
		// DIERR_NOTACQUIRED, so we won't get stuck an infinite loop.
		if (hRes == DIERR_INPUTLOST) {
			ReacquireInput();
		}

		// return the fact that we did not read any data
		return 0;
	}

	// process the data
	DWORD dw;
	for (dw = 0; dw < dwEvents; dw++) {
		switch (rgKeyData[dw].dwOfs) {
		// fire
		case DIK_SPACE:
			if (rgKeyData[dw].dwData & 0x80) {
				dwKeyState |= KEY_FIRE;
			} else {
				dwKeyState &= (DWORD)~KEY_FIRE;
			}
			break;

		// stop
		case DIK_NUMPAD5:
			if (rgKeyData[dw].dwData & 0x80) {
				dwKeyState |= KEY_STOP;
			} else {
				dwKeyState &= ~KEY_STOP;
			}
			break;

		// shield
		case DIK_NUMPAD7:
			if (rgKeyData[dw].dwData & 0x80) {
				dwKeyState |= KEY_SHIELD;
			} else {
				dwKeyState &= ~KEY_SHIELD;
			}
			break;

		// thrust
		case DIK_UP:
		case DIK_NUMPAD8:
			if (rgKeyData[dw].dwData & 0x80) {
				dwKeyState |= KEY_UP;
			} else {
				dwKeyState &= ~KEY_UP;
			}
			break;

		// reverse thrust
		case DIK_DOWN:
		case DIK_NUMPAD2:
			if (rgKeyData[dw].dwData & 0x80) {
				dwKeyState |= KEY_DOWN;
			} else {
				dwKeyState &= ~KEY_DOWN;
			}
			break;

		// rotate left
		case DIK_LEFT:
		case DIK_NUMPAD4:
			if (rgKeyData[dw].dwData & 0x80) {
				dwKeyState |= KEY_LEFT;
			} else {
				dwKeyState &= ~KEY_LEFT;
			}
			break;

		// rotate right
		case DIK_RIGHT:
		case DIK_NUMPAD6:
			if (rgKeyData[dw].dwData & 0x80) {
				dwKeyState |= KEY_RIGHT;
			} else {
				dwKeyState &= ~KEY_RIGHT;
			}
			break;
		}
	}

	// return the state of the keys to the caller
	return dwKeyState;
}

/*--------------------------------------------------------------------------
| ReadJoystickInput
|
| Requests joystick data and performs any needed processing.
|
*-------------------------------------------------------------------------*/
DWORD ReadJoystickInput(void)
{
	DWORD dwKeyState;
	DIJOYSTATE js;

	// poll the joystick to read the current state
	HRESULT hRes = g_pdevCurrent->Poll();

	// get data from the joystick
	hRes = g_pdevCurrent->GetDeviceState(sizeof(DIJOYSTATE), &js);

	if (hRes != DI_OK) {
		// did the read fail because we lost input for some reason?
		// if so, then attempt to reacquire.  If the second acquire
		// fails, then the error from GetDeviceData will be
		// DIERR_NOTACQUIRED, so we won't get stuck an infinite loop.
		if (hRes == DIERR_INPUTLOST) {
			ReacquireInput();
		}

		// return the fact that we did not read any data
		return 0;
	}

	//
	// Now study the position of the stick and the buttons.
	//

	dwKeyState = 0;

	if (js.lX < 0) {
		dwKeyState |= KEY_LEFT;
	} else if (js.lX > 0) {
		dwKeyState |= KEY_RIGHT;
	}

	if (js.lY < 0) {
		dwKeyState |= KEY_UP;
	} else if (js.lY > 0) {
		dwKeyState |= KEY_DOWN;
	}

	if (js.rgbButtons[0] & 0x80) {
		dwKeyState |= KEY_FIRE;
	}

	if (js.rgbButtons[1] & 0x80) {
		dwKeyState |= KEY_SHIELD;
	}

	if (js.rgbButtons[2] & 0x80) {
		dwKeyState |= KEY_STOP;
	}

	return dwKeyState;
}

/*--------------------------------------------------------------------------
| PickInputDevice
|
| Make the n'th input device the one that we will use for game play.
|
*-------------------------------------------------------------------------*/

BOOL PickInputDevice(int n)
{
	if (n < g_cpdevFound) {

		/*
		 *  Unacquire the old device.
		 */
		if (g_pdevCurrent) {
			g_pdevCurrent->Unacquire();
		}

		/*
		 *  Move to the new device.
		 */
		g_pdevCurrent = g_rgpdevFound[n];

		/*
		 *  Set ReadGameInput to the appropriate handler.
		 */
		if (n == 0) {
			ReadGameInput = ReadKeyboardInput;
		} else {
			ReadGameInput = ReadJoystickInput;
		}

		CheckMenuRadioItem(GetSubMenu(GetMenu(hWndMain), 0), IDC_DEVICES,
			static_cast<UINT>(IDC_DEVICES + g_cpdevFound - 1),
			static_cast<UINT>(IDC_DEVICES + n), MF_BYCOMMAND);

		ReacquireInput();

		return TRUE;
	}
	return FALSE;
}

/***************************************

	Normally, these are in dinput8.lib, but they are included
	here so the ARM 32 bit version of this sample can compile

***************************************/

/***************************************

	Keyboard with 256 optional keys

***************************************/

DIOBJECTDATAFORMAT c_rgodfDIKeyboard[256] = {
	{&GUID_Key, 0x00, DIDFT_MAKEINSTANCE(0x00) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x01, DIDFT_MAKEINSTANCE(0x01) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x02, DIDFT_MAKEINSTANCE(0x02) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x03, DIDFT_MAKEINSTANCE(0x03) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x04, DIDFT_MAKEINSTANCE(0x04) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x05, DIDFT_MAKEINSTANCE(0x05) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x06, DIDFT_MAKEINSTANCE(0x06) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x07, DIDFT_MAKEINSTANCE(0x07) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x08, DIDFT_MAKEINSTANCE(0x08) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x09, DIDFT_MAKEINSTANCE(0x09) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x0A, DIDFT_MAKEINSTANCE(0x0A) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x0B, DIDFT_MAKEINSTANCE(0x0B) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x0C, DIDFT_MAKEINSTANCE(0x0C) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x0D, DIDFT_MAKEINSTANCE(0x0D) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x0E, DIDFT_MAKEINSTANCE(0x0E) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x0F, DIDFT_MAKEINSTANCE(0x0F) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x10, DIDFT_MAKEINSTANCE(0x10) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x11, DIDFT_MAKEINSTANCE(0x11) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x12, DIDFT_MAKEINSTANCE(0x12) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x13, DIDFT_MAKEINSTANCE(0x13) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x14, DIDFT_MAKEINSTANCE(0x14) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x15, DIDFT_MAKEINSTANCE(0x15) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x16, DIDFT_MAKEINSTANCE(0x16) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x17, DIDFT_MAKEINSTANCE(0x17) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x18, DIDFT_MAKEINSTANCE(0x18) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x19, DIDFT_MAKEINSTANCE(0x19) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x1A, DIDFT_MAKEINSTANCE(0x1A) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x1B, DIDFT_MAKEINSTANCE(0x1B) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x1C, DIDFT_MAKEINSTANCE(0x1C) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x1D, DIDFT_MAKEINSTANCE(0x1D) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x1E, DIDFT_MAKEINSTANCE(0x1E) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x1F, DIDFT_MAKEINSTANCE(0x1F) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x20, DIDFT_MAKEINSTANCE(0x20) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x21, DIDFT_MAKEINSTANCE(0x21) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x22, DIDFT_MAKEINSTANCE(0x22) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x23, DIDFT_MAKEINSTANCE(0x23) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x24, DIDFT_MAKEINSTANCE(0x24) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x25, DIDFT_MAKEINSTANCE(0x25) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x26, DIDFT_MAKEINSTANCE(0x26) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x27, DIDFT_MAKEINSTANCE(0x27) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x28, DIDFT_MAKEINSTANCE(0x28) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x29, DIDFT_MAKEINSTANCE(0x29) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x2A, DIDFT_MAKEINSTANCE(0x2A) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x2B, DIDFT_MAKEINSTANCE(0x2B) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x2C, DIDFT_MAKEINSTANCE(0x2C) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x2D, DIDFT_MAKEINSTANCE(0x2D) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x2E, DIDFT_MAKEINSTANCE(0x2E) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x2F, DIDFT_MAKEINSTANCE(0x2F) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x30, DIDFT_MAKEINSTANCE(0x30) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x31, DIDFT_MAKEINSTANCE(0x31) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x32, DIDFT_MAKEINSTANCE(0x32) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x33, DIDFT_MAKEINSTANCE(0x33) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x34, DIDFT_MAKEINSTANCE(0x34) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x35, DIDFT_MAKEINSTANCE(0x35) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x36, DIDFT_MAKEINSTANCE(0x36) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x37, DIDFT_MAKEINSTANCE(0x37) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x38, DIDFT_MAKEINSTANCE(0x38) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x39, DIDFT_MAKEINSTANCE(0x39) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x3A, DIDFT_MAKEINSTANCE(0x3A) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x3B, DIDFT_MAKEINSTANCE(0x3B) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x3C, DIDFT_MAKEINSTANCE(0x3C) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x3D, DIDFT_MAKEINSTANCE(0x3D) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x3E, DIDFT_MAKEINSTANCE(0x3E) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x3F, DIDFT_MAKEINSTANCE(0x3F) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x40, DIDFT_MAKEINSTANCE(0x40) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x41, DIDFT_MAKEINSTANCE(0x41) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x42, DIDFT_MAKEINSTANCE(0x42) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x43, DIDFT_MAKEINSTANCE(0x43) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x44, DIDFT_MAKEINSTANCE(0x44) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x45, DIDFT_MAKEINSTANCE(0x45) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x46, DIDFT_MAKEINSTANCE(0x46) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x47, DIDFT_MAKEINSTANCE(0x47) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x48, DIDFT_MAKEINSTANCE(0x48) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x49, DIDFT_MAKEINSTANCE(0x49) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x4A, DIDFT_MAKEINSTANCE(0x4A) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x4B, DIDFT_MAKEINSTANCE(0x4B) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x4C, DIDFT_MAKEINSTANCE(0x4C) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x4D, DIDFT_MAKEINSTANCE(0x4D) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x4E, DIDFT_MAKEINSTANCE(0x4E) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x4F, DIDFT_MAKEINSTANCE(0x4F) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x50, DIDFT_MAKEINSTANCE(0x50) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x51, DIDFT_MAKEINSTANCE(0x51) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x52, DIDFT_MAKEINSTANCE(0x52) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x53, DIDFT_MAKEINSTANCE(0x53) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x54, DIDFT_MAKEINSTANCE(0x54) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x55, DIDFT_MAKEINSTANCE(0x55) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x56, DIDFT_MAKEINSTANCE(0x56) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x57, DIDFT_MAKEINSTANCE(0x57) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x58, DIDFT_MAKEINSTANCE(0x58) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x59, DIDFT_MAKEINSTANCE(0x59) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x5A, DIDFT_MAKEINSTANCE(0x5A) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x5B, DIDFT_MAKEINSTANCE(0x5B) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x5C, DIDFT_MAKEINSTANCE(0x5C) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x5D, DIDFT_MAKEINSTANCE(0x5D) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x5E, DIDFT_MAKEINSTANCE(0x5E) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x5F, DIDFT_MAKEINSTANCE(0x5F) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x60, DIDFT_MAKEINSTANCE(0x60) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x61, DIDFT_MAKEINSTANCE(0x61) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x62, DIDFT_MAKEINSTANCE(0x62) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x63, DIDFT_MAKEINSTANCE(0x63) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x64, DIDFT_MAKEINSTANCE(0x64) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x65, DIDFT_MAKEINSTANCE(0x65) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x66, DIDFT_MAKEINSTANCE(0x66) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x67, DIDFT_MAKEINSTANCE(0x67) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x68, DIDFT_MAKEINSTANCE(0x68) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x69, DIDFT_MAKEINSTANCE(0x69) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x6A, DIDFT_MAKEINSTANCE(0x6A) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x6B, DIDFT_MAKEINSTANCE(0x6B) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x6C, DIDFT_MAKEINSTANCE(0x6C) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x6D, DIDFT_MAKEINSTANCE(0x6D) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x6E, DIDFT_MAKEINSTANCE(0x6E) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x6F, DIDFT_MAKEINSTANCE(0x6F) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x70, DIDFT_MAKEINSTANCE(0x70) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x71, DIDFT_MAKEINSTANCE(0x71) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x72, DIDFT_MAKEINSTANCE(0x72) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x73, DIDFT_MAKEINSTANCE(0x73) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x74, DIDFT_MAKEINSTANCE(0x74) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x75, DIDFT_MAKEINSTANCE(0x75) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x76, DIDFT_MAKEINSTANCE(0x76) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x77, DIDFT_MAKEINSTANCE(0x77) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x78, DIDFT_MAKEINSTANCE(0x78) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x79, DIDFT_MAKEINSTANCE(0x79) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x7A, DIDFT_MAKEINSTANCE(0x7A) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x7B, DIDFT_MAKEINSTANCE(0x7B) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x7C, DIDFT_MAKEINSTANCE(0x7C) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x7D, DIDFT_MAKEINSTANCE(0x7D) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x7E, DIDFT_MAKEINSTANCE(0x7E) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x7F, DIDFT_MAKEINSTANCE(0x7F) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x80, DIDFT_MAKEINSTANCE(0x80) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x81, DIDFT_MAKEINSTANCE(0x81) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x82, DIDFT_MAKEINSTANCE(0x82) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x83, DIDFT_MAKEINSTANCE(0x83) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x84, DIDFT_MAKEINSTANCE(0x84) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x85, DIDFT_MAKEINSTANCE(0x85) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x86, DIDFT_MAKEINSTANCE(0x86) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x87, DIDFT_MAKEINSTANCE(0x87) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x88, DIDFT_MAKEINSTANCE(0x88) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x89, DIDFT_MAKEINSTANCE(0x89) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x8A, DIDFT_MAKEINSTANCE(0x8A) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x8B, DIDFT_MAKEINSTANCE(0x8B) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x8C, DIDFT_MAKEINSTANCE(0x8C) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x8D, DIDFT_MAKEINSTANCE(0x8D) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x8E, DIDFT_MAKEINSTANCE(0x8E) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x8F, DIDFT_MAKEINSTANCE(0x8F) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x90, DIDFT_MAKEINSTANCE(0x90) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x91, DIDFT_MAKEINSTANCE(0x91) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x92, DIDFT_MAKEINSTANCE(0x92) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x93, DIDFT_MAKEINSTANCE(0x93) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x94, DIDFT_MAKEINSTANCE(0x94) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x95, DIDFT_MAKEINSTANCE(0x95) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x96, DIDFT_MAKEINSTANCE(0x96) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x97, DIDFT_MAKEINSTANCE(0x97) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x98, DIDFT_MAKEINSTANCE(0x98) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x99, DIDFT_MAKEINSTANCE(0x99) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x9A, DIDFT_MAKEINSTANCE(0x9A) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x9B, DIDFT_MAKEINSTANCE(0x9B) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x9C, DIDFT_MAKEINSTANCE(0x9C) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x9D, DIDFT_MAKEINSTANCE(0x9D) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x9E, DIDFT_MAKEINSTANCE(0x9E) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0x9F, DIDFT_MAKEINSTANCE(0x9F) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xA0, DIDFT_MAKEINSTANCE(0xA0) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xA1, DIDFT_MAKEINSTANCE(0xA1) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xA2, DIDFT_MAKEINSTANCE(0xA2) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xA3, DIDFT_MAKEINSTANCE(0xA3) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xA4, DIDFT_MAKEINSTANCE(0xA4) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xA5, DIDFT_MAKEINSTANCE(0xA5) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xA6, DIDFT_MAKEINSTANCE(0xA6) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xA7, DIDFT_MAKEINSTANCE(0xA7) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xA8, DIDFT_MAKEINSTANCE(0xA8) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xA9, DIDFT_MAKEINSTANCE(0xA9) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xAA, DIDFT_MAKEINSTANCE(0xAA) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xAB, DIDFT_MAKEINSTANCE(0xAB) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xAC, DIDFT_MAKEINSTANCE(0xAC) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xAD, DIDFT_MAKEINSTANCE(0xAD) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xAE, DIDFT_MAKEINSTANCE(0xAE) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xAF, DIDFT_MAKEINSTANCE(0xAF) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xB0, DIDFT_MAKEINSTANCE(0xB0) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xB1, DIDFT_MAKEINSTANCE(0xB1) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xB2, DIDFT_MAKEINSTANCE(0xB2) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xB3, DIDFT_MAKEINSTANCE(0xB3) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xB4, DIDFT_MAKEINSTANCE(0xB4) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xB5, DIDFT_MAKEINSTANCE(0xB5) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xB6, DIDFT_MAKEINSTANCE(0xB6) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xB7, DIDFT_MAKEINSTANCE(0xB7) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xB8, DIDFT_MAKEINSTANCE(0xB8) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xB9, DIDFT_MAKEINSTANCE(0xB9) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xBA, DIDFT_MAKEINSTANCE(0xBA) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xBB, DIDFT_MAKEINSTANCE(0xBB) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xBC, DIDFT_MAKEINSTANCE(0xBC) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xBD, DIDFT_MAKEINSTANCE(0xBD) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xBE, DIDFT_MAKEINSTANCE(0xBE) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xBF, DIDFT_MAKEINSTANCE(0xBF) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xC0, DIDFT_MAKEINSTANCE(0xC0) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xC1, DIDFT_MAKEINSTANCE(0xC1) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xC2, DIDFT_MAKEINSTANCE(0xC2) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xC3, DIDFT_MAKEINSTANCE(0xC3) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xC4, DIDFT_MAKEINSTANCE(0xC4) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xC5, DIDFT_MAKEINSTANCE(0xC5) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xC6, DIDFT_MAKEINSTANCE(0xC6) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xC7, DIDFT_MAKEINSTANCE(0xC7) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xC8, DIDFT_MAKEINSTANCE(0xC8) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xC9, DIDFT_MAKEINSTANCE(0xC9) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xCA, DIDFT_MAKEINSTANCE(0xCA) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xCB, DIDFT_MAKEINSTANCE(0xCB) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xCC, DIDFT_MAKEINSTANCE(0xCC) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xCD, DIDFT_MAKEINSTANCE(0xCD) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xCE, DIDFT_MAKEINSTANCE(0xCE) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xCF, DIDFT_MAKEINSTANCE(0xCF) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xD0, DIDFT_MAKEINSTANCE(0xD0) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xD1, DIDFT_MAKEINSTANCE(0xD1) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xD2, DIDFT_MAKEINSTANCE(0xD2) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xD3, DIDFT_MAKEINSTANCE(0xD3) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xD4, DIDFT_MAKEINSTANCE(0xD4) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xD5, DIDFT_MAKEINSTANCE(0xD5) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xD6, DIDFT_MAKEINSTANCE(0xD6) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xD7, DIDFT_MAKEINSTANCE(0xD7) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xD8, DIDFT_MAKEINSTANCE(0xD8) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xD9, DIDFT_MAKEINSTANCE(0xD9) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xDA, DIDFT_MAKEINSTANCE(0xDA) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xDB, DIDFT_MAKEINSTANCE(0xDB) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xDC, DIDFT_MAKEINSTANCE(0xDC) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xDD, DIDFT_MAKEINSTANCE(0xDD) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xDE, DIDFT_MAKEINSTANCE(0xDE) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xDF, DIDFT_MAKEINSTANCE(0xDF) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xE0, DIDFT_MAKEINSTANCE(0xE0) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xE1, DIDFT_MAKEINSTANCE(0xE1) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xE2, DIDFT_MAKEINSTANCE(0xE2) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xE3, DIDFT_MAKEINSTANCE(0xE3) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xE4, DIDFT_MAKEINSTANCE(0xE4) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xE5, DIDFT_MAKEINSTANCE(0xE5) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xE6, DIDFT_MAKEINSTANCE(0xE6) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xE7, DIDFT_MAKEINSTANCE(0xE7) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xE8, DIDFT_MAKEINSTANCE(0xE8) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xE9, DIDFT_MAKEINSTANCE(0xE9) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xEA, DIDFT_MAKEINSTANCE(0xEA) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xEB, DIDFT_MAKEINSTANCE(0xEB) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xEC, DIDFT_MAKEINSTANCE(0xEC) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xED, DIDFT_MAKEINSTANCE(0xED) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xEE, DIDFT_MAKEINSTANCE(0xEE) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xEF, DIDFT_MAKEINSTANCE(0xEF) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xF0, DIDFT_MAKEINSTANCE(0xF0) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xF1, DIDFT_MAKEINSTANCE(0xF1) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xF2, DIDFT_MAKEINSTANCE(0xF2) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xF3, DIDFT_MAKEINSTANCE(0xF3) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xF4, DIDFT_MAKEINSTANCE(0xF4) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xF5, DIDFT_MAKEINSTANCE(0xF5) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xF6, DIDFT_MAKEINSTANCE(0xF6) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xF7, DIDFT_MAKEINSTANCE(0xF7) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xF8, DIDFT_MAKEINSTANCE(0xF8) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xF9, DIDFT_MAKEINSTANCE(0xF9) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xFA, DIDFT_MAKEINSTANCE(0xFA) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xFB, DIDFT_MAKEINSTANCE(0xFB) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xFC, DIDFT_MAKEINSTANCE(0xFC) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xFD, DIDFT_MAKEINSTANCE(0xFD) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xFE, DIDFT_MAKEINSTANCE(0xFE) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0},
	{&GUID_Key, 0xFF, DIDFT_MAKEINSTANCE(0xFF) | DIDFT_BUTTON | DIDFT_OPTIONAL,
		0}};

const DIDATAFORMAT c_dfDIKeyboard = {sizeof(DIDATAFORMAT),
	sizeof(DIOBJECTDATAFORMAT), // Buffer sizes
	DIDF_RELAXIS,               // Relative axis data
	256,                        // Size of the structure to store into
	sizeof(c_rgodfDIKeyboard) / sizeof(c_rgodfDIKeyboard[0]), // Size of array
	c_rgodfDIKeyboard};

/***************************************

	Joystick with two x/y/z sticks, two sliders, four POVs and up to 32 buttons

***************************************/

DIOBJECTDATAFORMAT c_rgodfDIJoy[44] = {
	{&GUID_XAxis, static_cast<DWORD>(FIELD_OFFSET(DIJOYSTATE, lX)),
		DIDFT_AXIS | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, DIDOI_ASPECTPOSITION},
	{&GUID_YAxis, static_cast<DWORD>(FIELD_OFFSET(DIJOYSTATE, lY)),
		DIDFT_AXIS | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, DIDOI_ASPECTPOSITION},
	{&GUID_ZAxis, static_cast<DWORD>(FIELD_OFFSET(DIJOYSTATE, lZ)),
		DIDFT_AXIS | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, DIDOI_ASPECTPOSITION},
	{&GUID_RxAxis, static_cast<DWORD>(FIELD_OFFSET(DIJOYSTATE, lRx)),
		DIDFT_AXIS | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, DIDOI_ASPECTPOSITION},
	{&GUID_RyAxis, static_cast<DWORD>(FIELD_OFFSET(DIJOYSTATE, lRy)),
		DIDFT_AXIS | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, DIDOI_ASPECTPOSITION},
	{&GUID_RzAxis, static_cast<DWORD>(FIELD_OFFSET(DIJOYSTATE, lRz)),
		DIDFT_AXIS | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, DIDOI_ASPECTPOSITION},
	{&GUID_Slider, static_cast<DWORD>(FIELD_OFFSET(DIJOYSTATE, rglSlider[0])),
		DIDFT_AXIS | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, DIDOI_ASPECTPOSITION},
	{&GUID_Slider, static_cast<DWORD>(FIELD_OFFSET(DIJOYSTATE, rglSlider[1])),
		DIDFT_AXIS | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, DIDOI_ASPECTPOSITION},
	{&GUID_POV, static_cast<DWORD>(FIELD_OFFSET(DIJOYSTATE, rgdwPOV[0])),
		DIDFT_POV | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, 0},
	{&GUID_POV, static_cast<DWORD>(FIELD_OFFSET(DIJOYSTATE, rgdwPOV[1])),
		DIDFT_POV | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, 0},
	{&GUID_POV, static_cast<DWORD>(FIELD_OFFSET(DIJOYSTATE, rgdwPOV[2])),
		DIDFT_POV | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, 0},
	{&GUID_POV, static_cast<DWORD>(FIELD_OFFSET(DIJOYSTATE, rgdwPOV[3])),
		DIDFT_POV | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, 0},
	{&GUID_Button, static_cast<DWORD>(FIELD_OFFSET(DIJOYSTATE, rgbButtons[0])),
		DIDFT_BUTTON | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, 0},
	{&GUID_Button, static_cast<DWORD>(FIELD_OFFSET(DIJOYSTATE, rgbButtons[1])),
		DIDFT_BUTTON | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, 0},
	{&GUID_Button, static_cast<DWORD>(FIELD_OFFSET(DIJOYSTATE, rgbButtons[2])),
		DIDFT_BUTTON | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, 0},
	{&GUID_Button, static_cast<DWORD>(FIELD_OFFSET(DIJOYSTATE, rgbButtons[3])),
		DIDFT_BUTTON | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, 0},
	{&GUID_Button, static_cast<DWORD>(FIELD_OFFSET(DIJOYSTATE, rgbButtons[4])),
		DIDFT_BUTTON | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, 0},
	{&GUID_Button, static_cast<DWORD>(FIELD_OFFSET(DIJOYSTATE, rgbButtons[5])),
		DIDFT_BUTTON | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, 0},
	{&GUID_Button, static_cast<DWORD>(FIELD_OFFSET(DIJOYSTATE, rgbButtons[6])),
		DIDFT_BUTTON | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, 0},
	{&GUID_Button, static_cast<DWORD>(FIELD_OFFSET(DIJOYSTATE, rgbButtons[7])),
		DIDFT_BUTTON | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, 0},
	{&GUID_Button, static_cast<DWORD>(FIELD_OFFSET(DIJOYSTATE, rgbButtons[8])),
		DIDFT_BUTTON | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, 0},
	{&GUID_Button, static_cast<DWORD>(FIELD_OFFSET(DIJOYSTATE, rgbButtons[9])),
		DIDFT_BUTTON | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, 0},
	{&GUID_Button, static_cast<DWORD>(FIELD_OFFSET(DIJOYSTATE, rgbButtons[10])),
		DIDFT_BUTTON | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, 0},
	{&GUID_Button, static_cast<DWORD>(FIELD_OFFSET(DIJOYSTATE, rgbButtons[11])),
		DIDFT_BUTTON | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, 0},
	{&GUID_Button, static_cast<DWORD>(FIELD_OFFSET(DIJOYSTATE, rgbButtons[12])),
		DIDFT_BUTTON | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, 0},
	{&GUID_Button, static_cast<DWORD>(FIELD_OFFSET(DIJOYSTATE, rgbButtons[13])),
		DIDFT_BUTTON | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, 0},
	{&GUID_Button, static_cast<DWORD>(FIELD_OFFSET(DIJOYSTATE, rgbButtons[14])),
		DIDFT_BUTTON | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, 0},
	{&GUID_Button, static_cast<DWORD>(FIELD_OFFSET(DIJOYSTATE, rgbButtons[15])),
		DIDFT_BUTTON | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, 0},
	{&GUID_Button, static_cast<DWORD>(FIELD_OFFSET(DIJOYSTATE, rgbButtons[16])),
		DIDFT_BUTTON | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, 0},
	{&GUID_Button, static_cast<DWORD>(FIELD_OFFSET(DIJOYSTATE, rgbButtons[17])),
		DIDFT_BUTTON | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, 0},
	{&GUID_Button, static_cast<DWORD>(FIELD_OFFSET(DIJOYSTATE, rgbButtons[18])),
		DIDFT_BUTTON | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, 0},
	{&GUID_Button, static_cast<DWORD>(FIELD_OFFSET(DIJOYSTATE, rgbButtons[19])),
		DIDFT_BUTTON | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, 0},
	{&GUID_Button, static_cast<DWORD>(FIELD_OFFSET(DIJOYSTATE, rgbButtons[20])),
		DIDFT_BUTTON | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, 0},
	{&GUID_Button, static_cast<DWORD>(FIELD_OFFSET(DIJOYSTATE, rgbButtons[21])),
		DIDFT_BUTTON | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, 0},
	{&GUID_Button, static_cast<DWORD>(FIELD_OFFSET(DIJOYSTATE, rgbButtons[22])),
		DIDFT_BUTTON | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, 0},
	{&GUID_Button, static_cast<DWORD>(FIELD_OFFSET(DIJOYSTATE, rgbButtons[23])),
		DIDFT_BUTTON | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, 0},
	{&GUID_Button, static_cast<DWORD>(FIELD_OFFSET(DIJOYSTATE, rgbButtons[24])),
		DIDFT_BUTTON | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, 0},
	{&GUID_Button, static_cast<DWORD>(FIELD_OFFSET(DIJOYSTATE, rgbButtons[25])),
		DIDFT_BUTTON | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, 0},
	{&GUID_Button, static_cast<DWORD>(FIELD_OFFSET(DIJOYSTATE, rgbButtons[26])),
		DIDFT_BUTTON | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, 0},
	{&GUID_Button, static_cast<DWORD>(FIELD_OFFSET(DIJOYSTATE, rgbButtons[27])),
		DIDFT_BUTTON | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, 0},
	{&GUID_Button, static_cast<DWORD>(FIELD_OFFSET(DIJOYSTATE, rgbButtons[28])),
		DIDFT_BUTTON | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, 0},
	{&GUID_Button, static_cast<DWORD>(FIELD_OFFSET(DIJOYSTATE, rgbButtons[29])),
		DIDFT_BUTTON | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, 0},
	{&GUID_Button, static_cast<DWORD>(FIELD_OFFSET(DIJOYSTATE, rgbButtons[30])),
		DIDFT_BUTTON | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, 0},
	{&GUID_Button, static_cast<DWORD>(FIELD_OFFSET(DIJOYSTATE, rgbButtons[31])),
		DIDFT_BUTTON | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL, 0}};

const DIDATAFORMAT c_dfDIJoystick = {sizeof(DIDATAFORMAT),
	sizeof(DIOBJECTDATAFORMAT), // Buffer sizes
	DIDF_ABSAXIS,               // Absolute axis data
	sizeof(DIJOYSTATE),         // Size of the structure to store into
	sizeof(c_rgodfDIJoy) / sizeof(c_rgodfDIJoy[0]), // Size of array
	c_rgodfDIJoy};
