//
// servo_unity_c.h
// 
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0.If a copy of the MPL was not distributed with this
// file, You can obtain one at https ://mozilla.org/MPL/2.0/.
//
// Copyright (c) 2019-2020 Mozilla, Inc.
//
// Author(s): Philip Lamb
//
// Declarations of plugin interfaces which are invoked from Unity via P/Invoke.
//


#ifndef __servo_unity_c_h__
#define __servo_unity_c_h__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Which platform we are on?
// UNITY_WIN - Windows (regular win32)
// UNITY_OSX - Mac OS X
// UNITY_LINUX - Linux
// UNITY_IOS - iOS
// UNITY_TVOS - tvOS
// UNITY_ANDROID - Android
// UNITY_METRO - WSA or UWP
// UNITY_WEBGL - WebGL
#if _MSC_VER
#  define UNITY_WIN 1
#elif defined(__APPLE__)
#  if TARGET_OS_TV
#    define UNITY_TVOS 1
#  elif TARGET_OS_IOS
#    define UNITY_IOS 1
#  else
#    define UNITY_OSX 1
#  endif
#elif defined(__ANDROID__)
#  define UNITY_ANDROID 1
#elif defined(UNITY_METRO) || defined(UNITY_LINUX) || defined(UNITY_WEBGL)
// these are defined externally
#elif defined(__EMSCRIPTEN__)
// this is already defined in Unity 5.6
#  define UNITY_WEBGL 1
#else
#  error "Unknown platform!"
#endif

// Which graphics device APIs we possibly support?
#if UNITY_METRO
#  define SUPPORT_D3D11 1
#  if WINDOWS_UWP
#    define SUPPORT_D3D12 1
#  endif
#elif UNITY_WIN
#  define SUPPORT_D3D11 1 // comment this out if you don't have D3D11 header/library files
#  define SUPPORT_D3D12 1 // comment this out if you don't have D3D12 header/library files
#  define SUPPORT_OPENGL_UNIFIED 0
#  define SUPPORT_OPENGL_CORE 0
#  define SUPPORT_VULKAN 0 // Requires Vulkan SDK to be installed
#elif UNITY_IOS || UNITY_TVOS || UNITY_ANDROID || UNITY_WEBGL
#  ifndef SUPPORT_OPENGL_ES
#    define SUPPORT_OPENGL_ES 1
#  endif
#  define SUPPORT_OPENGL_UNIFIED SUPPORT_OPENGL_ES
#  ifndef SUPPORT_VULKAN
#    define SUPPORT_VULKAN 0
#  endif
#elif UNITY_OSX || UNITY_LINUX
#  define SUPPORT_OPENGL_UNIFIED 1
#  define SUPPORT_OPENGL_CORE 1
#endif

#if UNITY_IOS || UNITY_TVOS || UNITY_OSX
#  define SUPPORT_METAL 1
#endif

// ServoUnity defines.

#define SERVO_UNITY_PLUGIN_VERSION "1.0"

#ifdef _WIN32
#  ifdef SERVO_UNITY_STATIC
#    define SERVO_UNITY_EXTERN
#  else
#    ifdef SERVO_UNITY_EXPORTS
#      define SERVO_UNITY_EXTERN __declspec(dllexport)
#    else
#      define SERVO_UNITY_EXTERN __declspec(dllimport)
#    endif
#  endif
#  define SERVO_UNITY_CALLBACK __stdcall
#else
#  define SERVO_UNITY_EXTERN
#  define SERVO_UNITY_CALLBACK
#endif

#define HOMEPAGE_DEFAULT "https://servo.org/"
#define SEARCH_URI_DEFAULT "https://www.google.com/search?client=firefox-b-d&q="

#ifdef __cplusplus
extern "C" {
#endif

enum  {
	ServoUnityTextureFormat_Invalid = 0,
	ServoUnityTextureFormat_RGBA32 = 1,
	ServoUnityTextureFormat_BGRA32 = 2,
	ServoUnityTextureFormat_ARGB32 = 3,
	ServoUnityTextureFormat_ABGR32 = 4,
	ServoUnityTextureFormat_RGB24 = 5,
	ServoUnityTextureFormat_BGR24 = 6,
	ServoUnityTextureFormat_RGBA4444 = 7,
	ServoUnityTextureFormat_RGBA5551 = 8,
	ServoUnityTextureFormat_RGB565 = 9
};

enum {
	ServoUnityVideoProjection_2D = 0,
	ServoUnityVideoProjection_360 = 1,
	ServoUnityVideoProjection_360S = 2, // 360 stereo
	ServoUnityVideoProjection_180 = 3,
	ServoUnityVideoProjection_180LR = 4, // 180 left to right
	ServoUnityVideoProjection_180TB = 5, // 180 top to bottom
	ServoUnityVideoProjection_3D = 6 // 3D side by side
};

enum {
    ServoUnityBrowserEvent_NOP = 0,
    ServoUnityBrowserEvent_Shutdown = 1,
    ServoUnityBrowserEvent_LoadStateChanged = 2, // eventData1: 0=LoadEnded, 1=LoadStarted,
    ServoUnityBrowserEvent_FullscreenStateChanged = 3, // eventData1: 0=WillEnterFullscreen, 1=DidEnterFullscreen, 2=WillExitFullscreen, 3=DidExitFullscreen,
    ServoUnityBrowserEvent_IMEStateChanged = 4, // eventData1: 0=HideIME, 1=ShowIME
    ServoUnityBrowserEvent_HistoryChanged = 5, // eventData1: 0=CantGoBack, 1=CanGoBack, eventData2: 0=CantGoForward, 1=CanGoForward
    ServoUnityBrowserEvent_TitleChanged = 6,
    ServoUnityBrowserEvent_URLChanged = 7,
    Total = 8
};

//
// ServoUnity custom plugin interface API.
//

typedef void (SERVO_UNITY_CALLBACK *PFN_LOGCALLBACK)(const char* msg);

///
/// Whenever a window is created (either because one was requested via
/// servoUnityRequestNewWindow, or when a browser action caused a new
/// window to be created), this callback will be invoked.
/// @param uidExt If the window was created in response to a request
///     to servoUnityRequestNewWindow the value passed in parameter
///     uidExt will be set to that value, otherwise it will be zero.
/// @param windowIndex The value of parameter windowIndex should be
///     used in subsequent API calls to specify this particular window as the target.
///
typedef void (SERVO_UNITY_CALLBACK *PFN_WINDOWCREATEDCALLBACK)(int uidExt, int windowIndex, int pixelWidth, int pixelHeight, int format);

typedef void (SERVO_UNITY_CALLBACK *PFN_WINDOWRESIZEDCALLBACK)(int uidExt, int pixelWidth, int pixelHeight);

typedef void (SERVO_UNITY_CALLBACK *PFN_BROWSEREVENTCALLBACK)(int uidExt, int eventType, int eventData0, int eventData1, const char *eventDataS);

///
/// Registers a callback function to use when a message is logged in the plugin.
/// If the callback is to become invalid, be sure to call this function with NULL
/// first so that the callback is unregistered.
///
SERVO_UNITY_EXTERN void servoUnityRegisterLogCallback(PFN_LOGCALLBACK logCcallback);

SERVO_UNITY_EXTERN void servoUnitySetLogLevel(const int logLevel);

SERVO_UNITY_EXTERN void servoUnityFlushLog(void);

///
/// Gets the plugin version as a C string, such as "1.0".
/// <remarks>This may be called at any time, including prior to calling servoUnityInit.</remarks>
/// @param buffer	The character buffer to populate
/// @param length	The maximum number of characters to set in buffer
/// @return			true if successful, false if an error occurred
///
SERVO_UNITY_EXTERN bool servoUnityGetVersion(char *buffer, int length);

///
/// Initialise the native plugin.
/// <remarks>Should be called on the main Unity thread.</remarks>
/// <param name="windowCreatedCallback">The supplied callback will be invoked when the native representation of a new window has been created.</param>
/// <param name="windowResizedCallback">>The supplied callback will be invoked when the native representation of a window has been resized. </param>
/// <param name="browserEventCallback">>The supplied callback will be invoked when a browser event has occured.</param>
/// <param name="userAgent">A null-terminated C string containing the user agent string to use in the browser, or NULL or an empty string to use the default.</param>
///
SERVO_UNITY_EXTERN void servoUnityInit(PFN_WINDOWCREATEDCALLBACK windowCreatedCallback, PFN_WINDOWRESIZEDCALLBACK windowResizedCallback, PFN_BROWSEREVENTCALLBACK browserEventCallback, const char *userAgent);

SERVO_UNITY_EXTERN void servoUnityFinalise(void);

///
/// Set the path in which the plugin should look for resources. Should be full filesystem path without trailing slash.
/// This should be called early on in the plugin lifecycle, typically from a Unity MonoBehaviour.OnEnable() event.
/// Normally this would be the path to Unity's StreamingAssets folder, which holds unprocessed resources for use at runtime.
///
SERVO_UNITY_EXTERN void servoUnitySetResourcesPath(const char *path);

enum {
    ServoUnityKeyCode_Null = 0,
    ServoUnityKeyCode_Character = 1,
    ServoUnityKeyCode_Backspace = 2,
    ServoUnityKeyCode_Delete = 3,
    ServoUnityKeyCode_Tab = 4,
    ServoUnityKeyCode_Clear = 5,
    ServoUnityKeyCode_Return = 6,
    ServoUnityKeyCode_Pause = 7,
    ServoUnityKeyCode_Escape = 8,
    ServoUnityKeyCode_Space = 9,
    ServoUnityKeyCode_UpArrow = 10,
    ServoUnityKeyCode_DownArrow = 11,
    ServoUnityKeyCode_RightArrow = 12,
    ServoUnityKeyCode_LeftArrow = 13,
    ServoUnityKeyCode_Insert = 14,
    ServoUnityKeyCode_Home = 15,
    ServoUnityKeyCode_End = 16,
    ServoUnityKeyCode_PageUp = 17,
    ServoUnityKeyCode_PageDown = 18,
    ServoUnityKeyCode_F1 = 19,
    ServoUnityKeyCode_F2 = 20,
    ServoUnityKeyCode_F3 = 21,
    ServoUnityKeyCode_F4 = 22,
    ServoUnityKeyCode_F5 = 23,
    ServoUnityKeyCode_F6 = 24,
    ServoUnityKeyCode_F7 = 25,
    ServoUnityKeyCode_F8 = 26,
    ServoUnityKeyCode_F9 = 27,
    ServoUnityKeyCode_F10 = 28,
    ServoUnityKeyCode_F11 = 29,
    ServoUnityKeyCode_F12 = 30,
    ServoUnityKeyCode_F13 = 31,
    ServoUnityKeyCode_F14 = 32,
    ServoUnityKeyCode_F15 = 33,
    ServoUnityKeyCode_F16 = 34,
    ServoUnityKeyCode_F17 = 35,
    ServoUnityKeyCode_F18 = 36,
    ServoUnityKeyCode_F19 = 37,
    ServoUnityKeyCode_Numlock = 38,
    ServoUnityKeyCode_CapsLock = 39,
    ServoUnityKeyCode_ScrollLock = 40,
    ServoUnityKeyCode_RightShift = 41,
    ServoUnityKeyCode_LeftShift = 42,
    ServoUnityKeyCode_RightControl = 43,
    ServoUnityKeyCode_LeftControl = 44,
    ServoUnityKeyCode_RightAlt = 45,
    ServoUnityKeyCode_LeftAlt = 46,
    ServoUnityKeyCode_LeftCommand = 47,
    ServoUnityKeyCode_LeftWindows = 48,
    ServoUnityKeyCode_RightCommand = 49,
    ServoUnityKeyCode_RightWindows = 50,
    ServoUnityKeyCode_AltGr = 51,
    ServoUnityKeyCode_Help = 52,
    ServoUnityKeyCode_Print = 53,
    ServoUnityKeyCode_SysReq = 54,
    ServoUnityKeyCode_Break = 55,
    ServoUnityKeyCode_Menu = 56,
    ServoUnityKeyCode_Keypad0 = 57,
    ServoUnityKeyCode_Keypad1 = 58,
    ServoUnityKeyCode_Keypad2 = 59,
    ServoUnityKeyCode_Keypad3 = 60,
    ServoUnityKeyCode_Keypad4 = 61,
    ServoUnityKeyCode_Keypad5 = 62,
    ServoUnityKeyCode_Keypad6 = 63,
    ServoUnityKeyCode_Keypad7 = 64,
    ServoUnityKeyCode_Keypad8 = 65,
    ServoUnityKeyCode_Keypad9 = 66,
    ServoUnityKeyCode_KeypadPeriod = 67,
    ServoUnityKeyCode_KeypadDivide = 68,
    ServoUnityKeyCode_KeypadMultiply = 69,
    ServoUnityKeyCode_KeypadMinus = 70,
    ServoUnityKeyCode_KeypadPlus = 71,
    ServoUnityKeyCode_KeypadEnter = 72,
    ServoUnityKeyCode_KeypadEquals = 73
};

/// @param upDown Set to 1 for keyDown events, 0 for keyUp events.
SERVO_UNITY_EXTERN void servoUnityKeyEvent(int windowIndex, int upDown, int keyCode, int character);

SERVO_UNITY_EXTERN int servoUnityGetWindowCount(void);

SERVO_UNITY_EXTERN bool servoUnityRequestNewWindow(int uidExt, int widthPixelsRequested, int heightPixelsRequested);

SERVO_UNITY_EXTERN bool servoUnityGetWindowTextureFormat(int windowIndex, int *width, int *height, int *format, bool *mipChain, bool *linear, void **nativeTextureID_p);

SERVO_UNITY_EXTERN uint64_t servoUnityGetBufferSizeForTextureFormat(int width, int height, int format);

///
/// On Direct3D-like devices pass a pointer to the base texture type (IDirect3DBaseTexture9 on D3D9, ID3D11Resource on D3D11),
/// or on OpenGL-like devices pass the texture "name", casting the integer to a pointer.
///
SERVO_UNITY_EXTERN bool servoUnitySetWindowUnityTextureID(int windowIndex, void *nativeTexturePtr);

SERVO_UNITY_EXTERN bool servoUnityRequestWindowSizeChange(int windowIndex, int width, int height);

SERVO_UNITY_EXTERN bool servoUnityCloseWindow(int windowIndex);

SERVO_UNITY_EXTERN bool servoUnityCloseAllWindows(void);

///
/// Call this function to service any browser event callbacks that must run on the main Unity thread.
/// <remarks>Since the native side does not have access to the Unity event loop, callback events
/// that should occur on the main Unity thread are instead queued, and must be periodically
/// serviced by invoking this function. The function does not return until all pending events
/// have been serviced.</remarks>
/// </summary>
/// <param name="windowIndex"></param>
///
SERVO_UNITY_EXTERN void servoUnityServiceWindowEvents(int windowIndex);

SERVO_UNITY_EXTERN void servoUnityGetWindowMetadata(int windowIndex, char *titleBuf, int titleBufLen, char *urlBuf, int urlBufLen);


///
/// Must be called from rendering thread with active rendering context.
/// As an alternative to invoking directly, an equivalent invocation can be invoked via call this sequence:
///     servoUnitySetRenderEventFunc1Params(windowIndex, timeDelta);
///     (*GetRenderEventFunc())(1);
///
SERVO_UNITY_EXTERN void servoUnityRequestWindowUpdate(int windowIndex, float timeDelta);

///
/// Must be called from rendering thread with active rendering context.
/// As an alternative to invoking directly, an equivalent invocation can be invoked via call this sequence:
///     servoUnitySetRenderEventFunc2Param(windowIndex);
///     (*GetRenderEventFunc())(2);
///
SERVO_UNITY_EXTERN void servoUnityCleanupRenderer(int windowIndex);

SERVO_UNITY_EXTERN void servoUnitySetRenderEventFunc1Params(int windowIndex, float timeDelta);

SERVO_UNITY_EXTERN void servoUnitySetRenderEventFunc2Param(int windowIndex);


enum {
	ServoUnityPointerEventID_Enter = 0,
	ServoUnityPointerEventID_Exit = 1,
	ServoUnityPointerEventID_Over = 2,
	ServoUnityPointerEventID_Press = 3,
	ServoUnityPointerEventID_Release = 4,
    ServoUnityPointerEventID_Click = 5,
	ServoUnityPointerEventID_ScrollDiscrete = 6,
    ServoUnityPointerEventID_TouchBegin = 7,
    ServoUnityPointerEventID_TouchMove = 8,
    ServoUnityPointerEventID_TouchEnd = 9,
    ServoUnityPointerEventID_TouchCancel = 10,
    ServoUnityPointerEventID_Max
};

enum {
    ServoUnityPointerEventMouseButtonID_Left = 0,
    ServoUnityPointerEventMouseButtonID_Right = 1,
    ServoUnityPointerEventMouseButtonID_Middle = 2,
    ServoUnityPointerEventMouseButtonID_Max
};

///
/// Send a pointer event to Servo.
/// <param name="windowIndex"></param>
/// <param name="eventID">A pointer event ID, from the enum ServoUnityPointerEventID_*.</param>
/// <param name="eventParam0"> For ServoUnityPointerEventID_Press, ServoUnityPointerEventID_Release, and ServoUnityPointerEventID_Click,
/// eventParam0 is a zero-based index for the mouse button, as indexed by the enum ServoUnityPointerEventMouseButtonID_*.
/// For ServoUnityPointerEventID_TouchBegin, ServoUnityPointerEventID_TouchMove, ServoUnityPointerEventID_TouchEnd,
/// and ServoUnityPointerEventID_TouchCancel event0 is an touch ID, which must be unique for any given finger for the duration of the
/// touch event. For all other eventID types, unused.
/// </param>
/// <param name="eventParam1"></param>
/// <param name="windowX">Window x coordinate at which the event occured.</param>
/// <param name="windowY">Window y coordinate at which the event occured.</param>
///
SERVO_UNITY_EXTERN void servoUnityWindowPointerEvent(int windowIndex, int eventID, int eventParam0, int eventParam1, int windowX, int windowY);

enum {
    ServoUnityWindowBrowserControlEventID_Refresh = 0,
    ServoUnityWindowBrowserControlEventID_Reload = 1,
    ServoUnityWindowBrowserControlEventID_Stop = 2,
    ServoUnityWindowBrowserControlEventID_GoBack = 3,
    ServoUnityWindowBrowserControlEventID_GoForward = 4,
    ServoUnityWindowBrowserControlEventID_GoHome = 5,
    ServoUnityWindowBrowserControlEventID_Navigate = 6,
    ServoUnityWindowBrowserControlEventID_IMEDismissed = 7,
    ServoUnityWindowBrowserControlEventID_Max
};

SERVO_UNITY_EXTERN void servoUnityWindowBrowserControlEvent(int windowIndex, int eventID, int eventParam0, int eventParam1, const char *eventParamS);

enum {
	ServoUnityParam_b_CloseNativeWindowOnClose = 0,
    ServoUnityParam_s_SearchURI = 1,
    ServoUnityParam_s_Homepage = 2,
	ServoUnityParam_Max
};

SERVO_UNITY_EXTERN void servoUnitySetParamBool(int param, bool flag);
SERVO_UNITY_EXTERN void servoUnitySetParamInt(int param, int val);
SERVO_UNITY_EXTERN void servoUnitySetParamFloat(int param, float val);
SERVO_UNITY_EXTERN void servoUnitySetParamString(int param, const char *s);
SERVO_UNITY_EXTERN bool servoUnityGetParamBool(int param);
SERVO_UNITY_EXTERN int servoUnityGetParamInt(int param);
SERVO_UNITY_EXTERN float servoUnityGetParamFloat(int param);
SERVO_UNITY_EXTERN void servoUnityGetParamString(int param, char *sbuf, int sbufLen);


#ifdef __cplusplus
}
#endif
#endif // !__servo_unity_c_h__
