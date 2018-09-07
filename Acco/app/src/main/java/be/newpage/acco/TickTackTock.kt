package be.newpage.acco

import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothSocket
import android.os.AsyncTask
import android.os.Handler
import android.os.Looper
import timber.log.Timber
import java.io.IOException
import java.util.*
import java.util.concurrent.Executors
import java.util.concurrent.ScheduledExecutorService
import java.util.concurrent.ScheduledFuture


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

    var myBluetooth: BluetoothAdapter? = null
    var btSocket: BluetoothSocket? = null
    private val isBtConnected = false
    val myUUID = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB")

    fun start() {
        startTime = System.currentTimeMillis() + START_DELAY
        Timber.d("start '%s' startTime: %d", track.title, startTime)


        handler.postDelayed({ sendDataOverBluetooth() }, START_DELAY)


//        scheduleHandler = scheduleTaskExecutor.scheduleAtFixedRate({ onTick() }, START_DELAY, TICK_DELAY, TimeUnit.MILLISECONDS)
    }

    private fun sendDataOverBluetooth() {
        ConnectBluetoothTask().execute()
    }

    fun stop() {
        Timber.d("stop '%s'", track.title)
//        scheduleHandler?.cancel(true)
    }

    private fun onTick() {
//        index++
//        Timber.d("Tick #%d %s", index, System.currentTimeMillis())
    }

    @SuppressLint("StaticFieldLeak")
    private inner class ConnectBluetoothTask : AsyncTask<Void, Void, Boolean>() {
        override fun doInBackground(vararg devices: Void): Boolean {
            Timber.d("opening bluetooth socket...")
            try {
                if (btSocket == null) {
                    btSocket = bluetoothDevice?.createInsecureRfcommSocketToServiceRecord(myUUID)
                    BluetoothAdapter.getDefaultAdapter().cancelDiscovery()
                    btSocket?.connect()

                    try {
                        val bytes = FileAccess().readFileContentsAsBytes(track.file)

                        Timber.d("sending %d bytes", bytes.size)
                        val outputStream = btSocket!!.outputStream.buffered()
                        outputStream.write(bytes)
                        btSocket?.close()
                        btSocket = null

                    } catch (e: IOException) {
                        e.printStackTrace()
                    }

                    return true;
                }
            } catch (e: IOException) {
                e.printStackTrace()
            }
            return false
        }

        override fun onPostExecute(result: Boolean) {
            super.onPostExecute(result)

            if (result) {
                Timber.d("bluetooth socket connected")
            } else {
                Timber.e("bluetooth socket connection failed")
            }
        }
    }
}