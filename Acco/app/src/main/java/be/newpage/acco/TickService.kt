package be.newpage.acco

import android.app.IntentService
import android.content.Context
import android.content.Intent
import android.os.Handler
import timber.log.Timber

private const val ACTION_START = "be.newpage.acco.action.START"
private const val EXTRA_TRACK = "be.newpage.acco.extra.TRACK"

class TickService : IntentService("TickService") {

    internal var handler: Handler = Handler()
    internal var startTime: Long = 0;
    internal var index = 0;

    override fun onHandleIntent(intent: Intent?) {
        when (intent?.action) {
            ACTION_START -> {
                val bytes = intent.getByteArrayExtra(EXTRA_TRACK)
                handleActionStart(bytes)
            }
        }
    }

    private fun handleActionStart(bytes: ByteArray) {
        startTime = System.currentTimeMillis() + START_DELAY

        Timber.d("startTime: %d", startTime)
        handler.postAtTime({ onTick() }, startTime)
    }

    private fun onTick() {
        index++
        Timber.d("Tick #%d %s", index, System.currentTimeMillis())

        //TODO send bytes over Bluetooth

        scheduleNextTick()
    }

    private fun scheduleNextTick() {
        val nextTick = startTime + (index + 1) * TICK_DELAY;
        handler.postAtTime({ onTick() }, nextTick)
    }

    override fun onDestroy() {
        Timber.d("service destroyed")
        super.onDestroy()
    }

    companion object {
        @JvmStatic
        fun startActionStart(context: Context, bytes: ByteArray) {
            val intent = Intent(context, TickService::class.java).apply {
                action = ACTION_START
                putExtra(EXTRA_TRACK, bytes)
            }
            context.startService(intent)
        }
    }
}
