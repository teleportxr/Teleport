package co.simul.teleportvrquestclient

import android.app.ActivityManager
import android.content.Context
import android.text.format.Formatter
import android.util.Log

class MemoryUtil {
    companion object {
        private lateinit var context: Context

        fun setContext(con: Context) {
            context=con
        }
    }

    private fun getAvailableMemory() : Long
    {
        val memoryInfo = ActivityManager.MemoryInfo()
        (context.getSystemService(Context.ACTIVITY_SERVICE) as ActivityManager).getMemoryInfo(memoryInfo)
        return memoryInfo.availMem
    }

    private fun getTotalMemory() : Long
    {
        val memoryInfo = ActivityManager.MemoryInfo()
        (context.getSystemService(Context.ACTIVITY_SERVICE) as ActivityManager).getMemoryInfo(memoryInfo)
        return memoryInfo.totalMem
    }

    private fun printMemoryStats()
    {
        val memoryInfo = ActivityManager.MemoryInfo()
        (context.getSystemService(Context.ACTIVITY_SERVICE) as ActivityManager).getMemoryInfo(memoryInfo)
        val nativeHeapSize = memoryInfo.totalMem
        val nativeHeapFreeSize = memoryInfo.availMem
        val usedMemInBytes = nativeHeapSize - nativeHeapFreeSize
        val usedMemInPercentage = usedMemInBytes * 100 / nativeHeapSize
        Log.d("AppLog", "total:${Formatter.formatFileSize(context, nativeHeapSize)} " +
                "free:${Formatter.formatFileSize(context, nativeHeapFreeSize)} " +
                "used:${Formatter.formatFileSize(context, usedMemInBytes)} ($usedMemInPercentage%)")
    }
}