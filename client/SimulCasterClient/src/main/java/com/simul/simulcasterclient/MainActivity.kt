package com.simul.simulcasterclient

import android.Manifest
import android.app.Activity
import android.content.pm.PackageManager
import android.os.Bundle
import android.widget.Toast
import androidx.annotation.NonNull
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat

class MainActivity : Activity() { //AppCompatActivity()  {

    private val AUDIO_ECHO_REQUEST = 0

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
    }

//    override fun onStart() {
//        super.onStart()
//
//        requestRecordPermission()
//    }
//
//    private fun requestRecordPermission() {
//        ActivityCompat.requestPermissions(
//                this, arrayOf<String>(Manifest.permission.RECORD_AUDIO),
//                AUDIO_ECHO_REQUEST)
//    }
//
//    override fun onRequestPermissionsResult(requestCode: Int,  permissions: Array<out String>, grantResults: IntArray) {
//        if (AUDIO_ECHO_REQUEST != requestCode) {
//            super.onRequestPermissionsResult(requestCode, permissions, grantResults)
//            return
//        }
//        if (grantResults.size != 1 ||
//                grantResults[0] != PackageManager.PERMISSION_GRANTED) {
//
//            // User denied the permission, without this we cannot record audio
//            // Acknowledge the user's response
//            Toast.makeText(applicationContext,
//                    "Teleport requires permission to record audio!", Toast.LENGTH_SHORT).show()
//        } else {
//            // Permission was granted from user so request permission from device to record
//            requestRecordPermission()
//        }
//    }
}
