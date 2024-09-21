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

extern HWND hWndMain;
extern DWORD ReadKeyboardInput(void);
extern DWORD ReadJoystickInput(void);

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

static BOOL InitKeyboardInput(LPDIRECTINPUT pdi)
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
	LPDIRECTINPUT pdi = static_cast<LPDIRECTINPUT>(pvRef);
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
	LPDIRECTINPUT pdi;

	// Note: Joystick support is a DirectX 5.0 feature.
	// Since we also want to run on DirectX 3.0, we will start out
	// with DirectX 3.0 to make sure that at least we get the keyboard.

	// create the DirectInput interface object
	if (DirectInput8Create(
			hInst, 0x0800, IID_IDirectInput8A, (void**)&pdi, NULL) != DI_OK) {
		OutputDebugStringA("DirectInputCreate 8.0 FAILED\n");
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
