// Copyright (c) Facebook Technologies, LLC and its affiliates. All Rights reserved.
package com.oculus.sdk.vrcubeworldfw

import android.app.Activity
import android.os.Bundle

/**
 * When using NativeActivity, we currently need to handle loading of dependent shared libraries
 * manually before a shared library that depends on them is loaded, since there is not currently a
 * way to specify a shared library dependency for NativeActivity via the manifest meta-data.
 *
 *
 * The simplest method for doing so is to subclass NativeActivity with an empty activity that
 * calls System.loadLibrary on the dependent libraries, which is unfortunate when the goal is to
 * write a pure native C/C++ only Android activity.
 *
 *
 * A native-code only solution is to load the dependent libraries dynamically using dlopen().
 * However, there are a few considerations, see:
 * https://groups.google.com/forum/#!msg/android-ndk/l2E2qh17Q6I/wj6s_6HSjaYJ
 *
 *
 * 1. Only call dlopen() if you're sure it will succeed as the bionic dynamic linker will
 * remember if dlopen failed and will not re-try a dlopen on the same lib a second time.
 *
 *
 * 2. Must remember what libraries have already been loaded to avoid infinitely looping when
 * libraries have circular dependencies.
 */
class MainActivity : Activity()
{
    private var alreadyRequestedRecordingPermission = false

    external fun nativeInitFromJava():Long

    override fun onCreate(savedInstanceState: Bundle?)
    {
        nativeInitFromJava();
        super.onCreate(savedInstanceState)
    }

    companion object {
        init {
            System.loadLibrary("vrapi")
            System.loadLibrary("vrcubeworldfw")
        }
    }
    private fun requestRecordPermission()
    {
        if(alreadyRequestedRecordingPermission)
        {
            // don't check again because the dialog is still open
            return;
        }
/*
        if(ActivityCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED)
        {
            //&& ActivityCompat.shouldShowRequestPermissionRationale(this, Manifest.permission.RECORD_AUDIO)){
            // the dialog will be opened so we have to keep that in memory
            alreadyRequestedRecordingPermission = true
            ActivityCompat.requestPermissions(this, arrayOf<String>(Manifest.permission.RECORD_AUDIO), 1024)
        }*/
    }
}