package be.newpage.acco

import android.bluetooth.BluetoothDevice
import android.os.Handler
import android.os.Looper
import timber.log.Timber
import java.util.concurrent.Executors
import java.util.concurrent.ScheduledExecutorService
import java.util.concurrent.ScheduledFuture
import java.util.concurrent.TimeUnit


const val START_DELAY = 2000L; //ms
const val TICK_DELAY = 10L; //ms
const val TEMPO_MIN = 0.3f;
const val TEMPO_MAX = 3f;

class TickTackTock(val track: Track, val bluetoothDevice: BluetoothDevice?) {

    internal var startTime: Long = 0;
    internal var nextTick: Long = 0;
    internal var index = 0;
    internal var handler: Handler = Handler(Looper.getMainLooper())

    internal val scheduleTaskExecutor: ScheduledExecutorService = Executors.newScheduledThreadPool(1)
    internal var scheduleHandler: ScheduledFuture<*>? = null

    fun start() {
        startTime = System.currentTimeMillis() + START_DELAY
        Timber.d("start '%s' startTime: %d", track.title, startTime)

        scheduleHandler = scheduleTaskExecutor.scheduleAtFixedRate({ onTick() }, START_DELAY, TICK_DELAY, TimeUnit.MILLISECONDS)
    }

    fun stop() {
        Timber.d("stop '%s'", track.title)
        scheduleHandler?.cancel(true)
    }

    private fun onTick() {
        index++
        Timber.d("Tick #%d %s", index, System.currentTimeMillis())
    }
}