// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
// If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// Copyright (c) 2019-2020 Mozilla.
//
// Author(s): Philip Lamb, Patrick O'Shaughnessey

using UnityEngine;
using System;
using System.Collections;
using System.Collections.Generic;
using System.Threading.Tasks;
using UnityEngine.UI;

public class ServoUnityWindow : ServoUnityPointableSurface
{
    private ServoUnityController suc = null;
    public int DefaultWidthToRequest = 1920;
    public int DefaultHeightToRequest = 1080;
    public bool flipX = false;
    public bool flipY = false;
    private static float DefaultWidth = 3.0f;
    public float Width = DefaultWidth;
    private float Height;
    private float textureScaleU;
    private float textureScaleV;

    private GameObject _videoMeshGO = null; // The GameObject which holds the MeshFilter and MeshRenderer for the video. 

    private Texture2D _videoTexture = null; // Texture object with the video image.

    private TextureFormat _textureFormat;

    public Vector2Int PixelSize
    {
        get => videoSize;
        private set { videoSize = value; }
    }

    public TextureFormat TextureFormat
    {
        get => _textureFormat;
        private set { _textureFormat = value; }
    }

    public static ServoUnityWindow FindWindowWithUID(int uid)
    {
        //Debug.Log("ServoUnityWindow.FindWindowWithUID(uid:" + uid + ")");
        if (uid != 0)
        {
            ServoUnityWindow[] windows = GameObject.FindObjectsOfType<ServoUnityWindow>();
            foreach (ServoUnityWindow window in windows)
            {
                if (window.GetInstanceID() == uid) return window;
            }
        }

        return null;
    }

    public static ServoUnityWindow CreateNewInParent(GameObject parent)
    {
        Debug.Log("ServoUnityWindow.CreateNewInParent(parent:" + parent + ")");
        ServoUnityWindow window = parent.AddComponent<ServoUnityWindow>();
        return window;
    }

    void Awake()
    {
        suc = FindObjectOfType<ServoUnityController>();
    }

    void OnDestroy()
    {
        suc = null;
    }

    public bool Visible
    {
        get => visible;
        set
        {
            visible = value;
            if (_videoMeshGO != null)
            {
                _videoMeshGO.SetActive(visible);
            }
        }
    }

    private bool visible = true;

    public int WindowIndex
    {
        get => _windowIndex;
    }

    private void HandleCloseKeyPressed()
    {
    }

    void Start()
    {
        Debug.Log("ServoUnityWindow.Start()");

        if (_windowIndex == 0) {
            servo_unity_plugin?.ServoUnityRequestNewWindow(GetInstanceID(), DefaultWidthToRequest, DefaultHeightToRequest);
        }
    }

    public void CleanupRenderer()
    {
        Debug.Log("ServoUnityWindow.CleanupRenderer()");
        if (_windowIndex == 0) return;

        servo_unity_plugin?.ServoUnityCleanupRenderer(_windowIndex);
    }

    public void Close()
    {
        Debug.Log("ServoUnityWindow.Close()");
        if (_windowIndex == 0) return;

        servo_unity_plugin?.ServoUnityCloseWindow(_windowIndex);
        _windowIndex = 0;
    }

    public void RequestSizeMultiple(float sizeMultiple)
    {
        Width = DefaultWidth * sizeMultiple;
        Resize(Mathf.FloorToInt(DefaultWidthToRequest * sizeMultiple),
            Mathf.FloorToInt(DefaultHeightToRequest * sizeMultiple));
    }

    public bool Resize(int widthPixels, int heightPixels)
    {
        if (servo_unity_plugin != null)
        {
            return servo_unity_plugin.ServoUnityRequestWindowSizeChange(_windowIndex, widthPixels, heightPixels);
        }
        return false;
    }

    /// <summary>
    /// Gets called once the plugin-side setup is done.
    /// </summary>
    /// <param name="windowIndex">The ID used on the plugin side to refer to this window.</param>
    /// <param name="widthPixels"></param>
    /// <param name="heightPixels"></param>
    /// <param name="format"></param>
    public void WasCreated(int windowIndex, int widthPixels, int heightPixels, TextureFormat format)
    {
        Debug.Log("ServoUnityWindow.WasCreated(windowIndex:" + windowIndex + ", widthPixels:" + widthPixels +
                  ", heightPixels:" + heightPixels + ", format:" + format + ")");
        _windowIndex = windowIndex;
        Height = (Width / widthPixels) * heightPixels;
        videoSize = new Vector2Int(widthPixels, heightPixels);
        _textureFormat = format;
        _videoTexture = CreateWindowTexture(videoSize.x, videoSize.y, _textureFormat, out textureScaleU, out textureScaleV);
        _videoMeshGO = ServoUnityTextureUtils.Create2DVideoSurface(_videoTexture, textureScaleU, textureScaleV, Width, Height,
            0, flipX, flipY, ServoUnityTextureUtils.VideoSurfaceColliderType.Mesh);
        _videoMeshGO.transform.parent = this.gameObject.transform;
        _videoMeshGO.transform.localPosition = Vector3.zero;
        _videoMeshGO.transform.localRotation = Quaternion.identity;
        _videoMeshGO.SetActive(Visible);

        suc.NavbarWindow = this; // Set ourself as the active window for the navbar.
    }

    public void WasResized(int widthPixels, int heightPixels)
    {
        Debug.Log("ServoUnityWindow.WasResized(widthPixels:" + widthPixels +
                  ", heightPixels:" + heightPixels + ")");
        if (_windowIndex == 0) return;
        Height = (Width / widthPixels) * heightPixels;
        videoSize = new Vector2Int(widthPixels, heightPixels);
        var oldTexture = _videoTexture;
        _videoTexture = CreateWindowTexture(videoSize.x, videoSize.y, _textureFormat, out textureScaleU, out textureScaleV);
        Destroy(oldTexture);

        ServoUnityTextureUtils.Configure2DVideoSurface(_videoMeshGO, _videoTexture, textureScaleU, textureScaleV, Width,
            Height, flipX, flipY);
    }

    // Update is called once per frame
    void Update()
    {
        if (_windowIndex == 0) return;

        servo_unity_plugin?.ServoUnityServiceWindowEvents(_windowIndex);

        //Debug.Log("ServoUnityWindow.Update() with _windowIndex == " + _windowIndex);
        servo_unity_plugin?.ServoUnityRequestWindowUpdate(_windowIndex, Time.deltaTime);
    }

    private Texture2D CreateWindowTexture(int videoWidth, int videoHeight, TextureFormat format,
        out float textureScaleU, out float textureScaleV)
    {
        // Check parameters.
        var vt = ServoUnityTextureUtils.CreateTexture(videoWidth, videoHeight, format);
        if (vt == null)
        {
            textureScaleU = 0;
            textureScaleV = 0;
        }
        else
        {
            textureScaleU = 1;
            textureScaleV = 1;
        }

        // Now pass the ID to the native side.
        IntPtr nativeTexPtr = vt.GetNativeTexturePtr();
        //Debug.Log("Calling ServoUnitySetWindowUnityTextureID(windowIndex:" + _windowIndex + ", nativeTexPtr:" + nativeTexPtr.ToString("X") + ")");
        servo_unity_plugin?.ServoUnitySetWindowUnityTextureID(_windowIndex, nativeTexPtr);

        return vt;
    }

    private void DestroyWindow()
    {
        bool ed = Application.isEditor;
        if (_videoTexture != null)
        {
            if (ed) DestroyImmediate(_videoTexture);
            else Destroy(_videoTexture);
            _videoTexture = null;
        }

        if (_videoMeshGO != null)
        {
            if (ed) DestroyImmediate(_videoMeshGO);
            else Destroy(_videoMeshGO);
            _videoMeshGO = null;
        }

        Resources.UnloadUnusedAssets();
    }
}