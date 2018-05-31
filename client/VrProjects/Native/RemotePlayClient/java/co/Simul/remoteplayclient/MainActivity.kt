// (C) Copyright 2018 Simul.co

package co.Simul.remoteplayclient

import android.graphics.SurfaceTexture
import android.os.Bundle
import android.util.Log
import android.view.Surface
import com.oculus.vrappframework.VrActivity
import java.util.concurrent.Executors

class MainActivity : VrActivity(), SurfaceTexture.OnFrameAvailableListener {
    external fun nativeSetAppInterface(act: VrActivity?, fromPackageNameString: String, commandString: String, uriString: String): Long
    external fun nativeFrameAvailable(appPtr: Long)

    private val mStreamDecoder = StreamDecoder(VideoCodec.H265)

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
        mStreamDecoder.configure(VIDEO_WIDTH, VIDEO_HEIGHT, Surface(videoTexture))

        val executorService = Executors.newSingleThreadExecutor()
        executorService.execute(DecoderService(mStreamDecoder))
    }

    override fun onFrameAvailable(surfaceTexture: SurfaceTexture?) {
        nativeFrameAvailable(appPtr)
    }

    private class DecoderService(private val mStreamDecoder: StreamDecoder): Runnable {
        override fun run() {
            Thread.sleep(500)
            while(true) {
                mStreamDecoder.process()
            }
        }
    }

    companion object {
        const val TAG = "RemotePlayClient"
        const val VIDEO_WIDTH  = 3840
        const val VIDEO_HEIGHT = 1920

        init {
            Log.d(TAG, "LoadLibrary")
            System.loadLibrary("ovrapp")
        }
    }
}