// (C) Copyright 2018 Simul.co

package co.Simul.remoteplayclient

import android.os.Bundle
import android.util.Log
import com.oculus.vrappframework.VrActivity

class MainActivity : VrActivity() {
    external fun nativeSetAppInterface(act: VrActivity?, fromPackageNameString: String, commandString: String, uriString: String): Long

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val commandString = getCommandStringFromIntent(intent)
        val fromPackageNameString = getPackageStringFromIntent(intent)
        val uriString = getUriStringFromIntent(intent)

        appPtr = nativeSetAppInterface(this, fromPackageNameString, commandString, uriString)
    }

    companion object {
        const val TAG = "RemotePlayClient"

        init {
            Log.d(TAG, "LoadLibrary")
            System.loadLibrary("ovrapp")
        }
    }
}