//
// ServoUnityWindowDX11.cpp
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0.If a copy of the MPL was not distributed with this
// file, You can obtain one at https ://mozilla.org/MPL/2.0/.
//
// Copyright (c) 2019-2020 Mozilla, Inc.
//
// Author(s): Philip Lamb
//

#include <stdlib.h>
#include "ServoUnityWindowDX11.h"
#if SUPPORT_D3D11
#include <d3d11.h>
#include "IUnityGraphicsD3D11.h"
#include "servo_unity_log.h"

#include <assert.h>
#include <stdio.h>

static ID3D11Device* s_D3D11Device = nullptr;

void ServoUnityWindowDX11::initDevice(IUnityInterfaces* unityInterfaces) {
	IUnityGraphicsD3D11* ud3d = unityInterfaces->Get<IUnityGraphicsD3D11>();
	s_D3D11Device = ud3d->GetDevice();
}

void ServoUnityWindowDX11::finalizeDevice() {
	s_D3D11Device = nullptr; // The object itself being owned by Unity will go away without our help, but we should clear our weak reference.
}

ServoUnityWindowDX11::ServoUnityWindowDX11(int uid, int uidExt, Size size) :
	ServoUnityWindow(uid, uidExt),
	m_servoTexPtr(nullptr),
	m_servoTexHandle(nullptr),
	m_size(size),
	m_format(ServoUnityTextureFormat_Invalid),
    m_unityTexPtr(nullptr)
{
}

ServoUnityWindowDX11::~ServoUnityWindowDX11() {
}

static int getServoUnityTextureFormatForDXGIFormat(DXGI_FORMAT format)
{
	switch (format) {
		case DXGI_FORMAT_R8G8B8A8_TYPELESS:
		case DXGI_FORMAT_R8G8B8A8_UNORM:
		case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
		case DXGI_FORMAT_R8G8B8A8_UINT:
			return ServoUnityTextureFormat_RGBA32;
			break;
		case DXGI_FORMAT_B8G8R8A8_UNORM:
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
		case DXGI_FORMAT_B8G8R8A8_TYPELESS:
			return ServoUnityTextureFormat_BGRA32;
			break;
		case DXGI_FORMAT_B4G4R4A4_UNORM:
			return ServoUnityTextureFormat_RGBA4444;
			break;
		case DXGI_FORMAT_B5G6R5_UNORM:
			return ServoUnityTextureFormat_RGB565;
			break;
		case DXGI_FORMAT_B5G5R5A1_UNORM:
			return ServoUnityTextureFormat_RGBA5551;
			break;
		default:
			return ServoUnityTextureFormat_Invalid;
	}
}

bool ServoUnityWindowDX11::init(PFN_WINDOWCREATEDCALLBACK windowCreatedCallback, PFN_WINDOWRESIZEDCALLBACK windowResizedCallback, PFN_BROWSEREVENTCALLBACK browserEventCallback)
{
    if (!ServoUnityWindow::init(windowCreatedCallback, windowResizedCallback, browserEventCallback)) return false;
    
    // TODO: Get Servo texture handle into m_servoTexHandle.
    if (!m_servoTexHandle) {
		SERVOUNITYLOGe("Error: Servo texture handle is null.\n");
		return false;
	} else {
		// Extract a pointer to the D3D texture from the shared handle.
		HRESULT hr = s_D3D11Device->OpenSharedResource(m_servoTexHandle, IID_PPV_ARGS(&m_servoTexPtr));
		if (hr != S_OK) {
			SERVOUNITYLOGe("Can't get pointer to Servo texture from handle.\n");
			return false;
		} else {
			D3D11_TEXTURE2D_DESC descServo = { 0 };
			m_servoTexPtr->GetDesc(&descServo);
			m_size = Size({ (int)descServo.Width, (int)descServo.Height });
            m_format = getServoUnityTextureFormatForDXGIFormat(descServo.Format);

			if (m_windowCreatedCallback) (*m_windowCreatedCallback)(m_uidExt, m_uid, m_size.w, m_size.h, m_format);
		}
	}

    return true;
}

ServoUnityWindow::Size ServoUnityWindowDX11::size() {
	return m_size;
}

void ServoUnityWindowDX11::setSize(ServoUnityWindow::Size size) {
	// TODO: request change in the Servo window size.

    if (m_windowResizedCallback) (*m_windowResizedCallback)(m_uidExt, m_size.w, m_size.h);
}

void ServoUnityWindowDX11::setNativePtr(void* texPtr) {
	m_unityTexPtr = texPtr;
}

void* ServoUnityWindowDX11::nativePtr() {
	return m_unityTexPtr;
}

void ServoUnityWindowGL::initRenderer(CInitOptions cio, void (*wakeup)(void), CHostCallbacks chc) {
    // init_with_gl will capture the active GL context for later use by fill_gl_texture.
    // This will be the Unity GL context.
    init_with_egl(cio, wakeup, chc);
}

void ServoUnityWindowDX11::requestUpdate(float timeDelta) {
    SERVOUNITYLOGd("ServoUnityWindowDX11::requestUpdate(%f)\n", timeDelta);

    ServoUnityWindow::requestUpdate(timeDelta);

	if (!m_servoTexPtr || !m_unityTexPtr) {
		SERVOUNITYLOGi("ServoUnityWindowDX11::requestUpdate() m_servoTexPtr=%p, m_unityTexPtr=%p.\n", m_servoTexPtr, m_unityTexPtr);
		return;
	}

	ID3D11DeviceContext* ctx = NULL;
	s_D3D11Device->GetImmediateContext(&ctx);

	D3D11_TEXTURE2D_DESC descUnity = { 0 };
	D3D11_TEXTURE2D_DESC descServo = { 0 };

	m_servoTexPtr->GetDesc(&descServo);
	((ID3D11Texture2D*)m_unityTexPtr)->GetDesc(&descUnity);
	//SERVOUNITYLOGd("Unity texture is %dx%d, DXGI_FORMAT=%d (ServoUnityTextureFormat=%d), MipLevels=%d, D3D11_USAGE Usage=%d, BindFlags=%d, CPUAccessFlags=%d, MiscFlags=%d\n", descUnity.Width, descUnity.Height, descUnity.Format, getServoUnityTextureFormatForDXGIFormat(descUnity.Format), descUnity.MipLevels, descUnity.Usage, descUnity.BindFlags, descUnity.CPUAccessFlags, descUnity.MiscFlags);
	if (descServo.Width != descUnity.Width || descServo.Height != descServo.Height) {
		SERVOUNITYLOGe("Error: Unity texture size %dx%d does not match Servo texture size %dx%d.\n", descUnity.Width, descUnity.Height, descServo.Width, descServo.Height);
	} else {
		ctx->CopyResource((ID3D11Texture2D*)m_unityTexPtr, m_servoTexPtr);
	}

	ctx->Release();
}

#endif // SUPPORT_D3D11
