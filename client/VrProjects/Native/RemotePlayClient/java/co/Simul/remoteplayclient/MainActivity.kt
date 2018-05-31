// (C) Copyright 2018 Simul.co

package co.Simul.remoteplayclient

import android.graphics.SurfaceTexture
import android.media.MediaPlayer
import android.os.Bundle
import android.util.Log
import android.view.Surface
import com.oculus.vrappframework.VrActivity

class MainActivity : VrActivity(), SurfaceTexture.OnFrameAvailableListener {
    external fun nativeSetAppInterface(act: VrActivity?, fromPackageNameString: String, commandString: String, uriString: String): Long
    external fun nativeFrameAvailable(appPtr: Long)

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val commandString = getCommandStringFromIntent(intent)
        val fromPackageNameString = getPackageStringFromIntent(intent)
        val uriString = getUriStringFromIntent(intent)

        appPtr = nativeSetAppInterface(this, fromPackageNameString, commandString, uriString)
    }

    fun initializeVideoStream(videoTexture: SurfaceTexture?) {
        videoTexture?.let {
            runOnUiThread({
                initializeVideoStream_Implementation(it)
            })
        }
    }

    private fun initializeVideoStream_Implementation(videoTexture: SurfaceTexture) {
        videoTexture.setOnFrameAvailableListener(this)
        mVideoSurface = Surface(videoTexture)
        mMediaPlayer.setSurface(mVideoSurface)

        val fd = assets.openFd("video.mp4")
        mMediaPlayer.setDataSource(fd.fileDescriptor, fd.startOffset, fd.length)
        mMediaPlayer.prepare()
        mMediaPlayer.start()
    }

    override fun onFrameAvailable(surfaceTexture: SurfaceTexture?) {
        nativeFrameAvailable(appPtr)
    }

    private val mMediaPlayer = MediaPlayer()
    private lateinit var mVideoSurface: Surface

    companion object {
        const val TAG = "RemotePlayClient"

        init {
            Log.d(TAG, "LoadLibrary")
            System.loadLibrary("ovrapp")
        }
    }
}