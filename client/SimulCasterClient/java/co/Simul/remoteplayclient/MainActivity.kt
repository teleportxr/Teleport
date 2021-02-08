// (C) Copyright 2018 Simul.co

package co.Simul.remoteplayclient

import android.Manifest
import android.content.Intent
import android.content.pm.ActivityInfo
import android.content.pm.PackageManager
import android.os.Bundle
import android.util.Log
import androidx.core.app.ActivityCompat
import com.oculus.vrappframework.VrActivity
import com.simul.simulcasterclient.MainActivity

class MainActivity : VrActivity() {
    private var alreadyRequestedRecordingPermission = false

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

    override fun onStart() {
        super.onStart()

        //requestRecordPermission()
//        Toast.makeText(applicationContext,
//                "This is a test!!!!!", Toast.LENGTH_SHORT).show()
        //setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_SENSOR);

        //val switchActivityIntent = Intent(this, MainActivity::class.java)
       // startActivity(switchActivityIntent)
    }

    private fun requestRecordPermission() {
        if(alreadyRequestedRecordingPermission){
            // don't check again because the dialog is still open
            return;
        }

        if(ActivityCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED){
            // the dialog will be opened so we have to keep that in memory
            alreadyRequestedRecordingPermission = true
            setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_NOSENSOR);
            ActivityCompat.requestPermissions(this, arrayOf<String>(Manifest.permission.RECORD_AUDIO), 0)
        }
    }
}
