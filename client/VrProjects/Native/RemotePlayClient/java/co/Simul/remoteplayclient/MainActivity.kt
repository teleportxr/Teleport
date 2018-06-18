// (C) Copyright 2018 Simul.co

package co.Simul.remoteplayclient

import android.graphics.SurfaceTexture
import android.os.Bundle
import android.util.Log
import android.view.Surface
import com.oculus.vrappframework.VrActivity
import java.util.concurrent.CyclicBarrier
import java.util.concurrent.Executors

class MainActivity : VrActivity(), SurfaceTexture.OnFrameAvailableListener {
    external fun nativeSetAppInterface(act: VrActivity?, fromPackageNameString: String, commandString: String, uriString: String): Long
    external fun nativeFrameAvailable(appPtr: Long)

    private val mStreamDecoder = StreamDecoder(VideoCodec.H265)
    private var mDecoderService: DecoderService? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val commandString = getCommandStringFromIntent(intent)
        val fromPackageNameString = getPackageStringFromIntent(intent)
        val uriString = getUriStringFromIntent(intent)

        appPtr = nativeSetAppInterface(this, fromPackageNameString, commandString, uriString)
    }

    fun initializeVideoStream(port: Int, width: Int, height: Int, videoTexture: SurfaceTexture?) {
        videoTexture?.let {
            runOnUiThread({
                initializeVideoStream_Implementation(port, width, height, it)
            })
        }
    }

    fun closeVideoStream() {
        runOnUiThread({
            closeVideoStream_Implementation()
        })
    }

    private fun initializeVideoStream_Implementation(port: Int, width: Int, height: Int, videoTexture: SurfaceTexture) {
        if(mStreamDecoder.isConfigured()) {
            closeVideoStream_Implementation()
        }

        videoTexture.setOnFrameAvailableListener(this)
        mStreamDecoder.configure(port, width, height, Surface(videoTexture))

        val executorService = Executors.newSingleThreadExecutor()
        mDecoderService = DecoderService(mStreamDecoder)
        executorService.execute(mDecoderService)
    }

    private fun closeVideoStream_Implementation() {
        mDecoderService?.stop()
        mDecoderService = null
        mStreamDecoder.reset()
    }

    override fun onFrameAvailable(surfaceTexture: SurfaceTexture?) {
        nativeFrameAvailable(appPtr)
    }

    private class DecoderService(private val mStreamDecoder: StreamDecoder): Runnable {
        private val mBarrier = CyclicBarrier(2)
        @Volatile private var mRunning = true

        override fun run() {
            Thread.sleep(500)
            while(mRunning) {
                mStreamDecoder.process()
            }
            mBarrier.await()
        }
        fun stop() {
            mRunning = false
            mBarrier.await()
        }
    }

    companion object {
        const val TAG = "RemotePlayClient"
        init {
            Log.d(TAG, "LoadLibrary")
            System.loadLibrary("ovrapp")
        }
    }
}