package be.newpage.acco

import android.Manifest.permission.READ_EXTERNAL_STORAGE
import android.content.pm.PackageManager
import android.content.pm.PackageManager.PERMISSION_GRANTED
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.support.v4.app.ActivityCompat
import android.support.v4.content.ContextCompat.checkSelfPermission
import android.support.v7.app.AppCompatActivity
import android.support.v7.widget.LinearLayoutManager
import android.support.v7.widget.RecyclerView
import android.util.Log
import android.view.LayoutInflater
import android.view.View
import android.view.View.INVISIBLE
import android.view.View.VISIBLE
import android.view.ViewGroup
import app.akexorcist.bluetotohspp.library.BluetoothSPP
import app.akexorcist.bluetotohspp.library.BluetoothSPP.AutoConnectionListener
import app.akexorcist.bluetotohspp.library.BluetoothSPP.BluetoothConnectionListener
import app.akexorcist.bluetotohspp.library.BluetoothState
import kotlinx.android.synthetic.main.activity_main.*
import kotlinx.android.synthetic.main.row_track.view.*
import timber.log.Timber
import java.text.SimpleDateFormat
import java.util.*
import java.util.concurrent.Executors
import java.util.concurrent.ScheduledExecutorService
import java.util.concurrent.ScheduledFuture
import java.util.concurrent.TimeUnit


class MainActivity : AppCompatActivity() {

    private val START_DELAY: Long = 2000; //ms
    private val TICK_DELAY: Long = 10; //ms

    private val REQUEST_PERMISSIONS_RESULT_CODE: Int = 1
    private val timeFormat: SimpleDateFormat = SimpleDateFormat("m:ss", Locale.FRANCE)

    internal var tracks: List<Track> = ArrayList()
    internal var layoutManager: LinearLayoutManager = LinearLayoutManager(this)
    internal var adapter: RecyclerView.Adapter<*> = TracksAdapter()
    internal var connectedDeviceName: String? = null

    internal var selectedTrack: Track? = null
    internal var index = 0;
    internal var bytes: ByteArray? = null

    internal val scheduleTaskExecutor: ScheduledExecutorService = Executors.newScheduledThreadPool(1)
    internal var scheduleHandler: ScheduledFuture<*>? = null
    internal var handler: Handler = Handler(Looper.getMainLooper())

    internal val bt = BluetoothSPP(this)

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        recyclerview.setHasFixedSize(true)
        recyclerview.setLayoutManager(layoutManager)
        recyclerview.setAdapter(adapter)

        playIcon.setOnClickListener { onPlayClicked() }
        pauseIcon.setOnClickListener { onPauseClicked() }

        requestFileAccess()

        connectWithBluetoothDevice()
    }

    private fun onPauseClicked() {
        stop()
        playIcon.visibility = VISIBLE
        pauseIcon.visibility = INVISIBLE
    }

    private fun onPlayClicked() {
        if (selectedTrack != null) start()
        playIcon.visibility = INVISIBLE
        pauseIcon.visibility = VISIBLE
    }

    private fun updateBluetoothState(state: String, warning: Boolean = false) {
        bluetoothStatusTextView.text = state;
        warningIcon.visibility = if (warning) VISIBLE else INVISIBLE

        Timber.log(if (warning) Log.ERROR else Log.DEBUG, "BT state: %s", state)
    }

    private fun connectWithBluetoothDevice() {
        if (!bt.isBluetoothAvailable()) {
            updateBluetoothState("Not available", true)
            return;
        }

        if (!bt.isBluetoothEnabled()) {
            updateBluetoothState("Not enabled", true)
            return;
        }

        bt.setupService();
        bt.startService(BluetoothState.DEVICE_OTHER);

        bt.autoConnect("HC-05")

        bt.setBluetoothConnectionListener(object : BluetoothConnectionListener {
            override fun onDeviceConnected(name: String, address: String) {
                Timber.d("BT connection: device connected %s %s", name, address)
                connectedDeviceName = name
            }

            override fun onDeviceDisconnected() {
                Timber.d("BT connection: device disconnected")
            }

            override fun onDeviceConnectionFailed() {
                Timber.d("BT connection: device connection failed")
            }
        })

        bt.setAutoConnectionListener(object : AutoConnectionListener {
            override fun onNewConnection(name: String, address: String) {
                Timber.d("BT autoConnect: new connection")
            }

            override fun onAutoConnectionStarted() {
                Timber.d("BT autoConnect: started")
            }
        })

        bt.setBluetoothStateListener {
            if (it == BluetoothState.STATE_CONNECTED) {
                updateBluetoothState(String.format("Connected %s", connectedDeviceName))
            } else if (it == BluetoothState.STATE_CONNECTING) {
                updateBluetoothState("Connecting")
            } else if (it == BluetoothState.STATE_LISTEN) {
                updateBluetoothState("Listening")
            } else if (it == BluetoothState.STATE_NONE) {
                updateBluetoothState("None")
            }
        }
    }

    fun readFiles() {
        val files = FileAccess().getDirectoryListing()
        val tracks = ArrayList<Track>()

        for (file in files) {
            tracks.add(Track(file.nameWithoutExtension, file))
        }

        onTracksLoaded(tracks);
    }

    fun start() {
        index = 0

        delayTextView.text = getString(R.string.seconds_start_delay, TimeUnit.SECONDS.convert(START_DELAY, TimeUnit.MILLISECONDS))
        delayTextView.visibility = VISIBLE

        Timber.d("start '%s' with %dms delay", selectedTrack?.title, START_DELAY)
        scheduleHandler = scheduleTaskExecutor.scheduleAtFixedRate({ onTick() }, START_DELAY, TICK_DELAY, TimeUnit.MILLISECONDS)
    }

    fun stop() {
        if (selectedTrack != null) {
            Timber.d("stop '%s'", selectedTrack?.title)
            scheduleHandler?.cancel(true)
        }
    }

    private fun onTick() {
        val byte = bytes!!.get(index)

        val time = index * TICK_DELAY
        if (index == 0) delayTextView.visibility = INVISIBLE
        if (index % 10 == 0) handler.post { timerTextView.text = timeFormat.format(time) }

        Timber.d("Tick #%d %s   send byte: %s   time %s", index, System.currentTimeMillis(), String.format("%02X", byte), timeFormat.format(time))
        bt.send(ByteArray(1, { byte }), false)

        if (index == bytes!!.size - 1) onEnded()
        index++
    }

    private fun onEnded() {
        onPauseClicked()
    }


    fun requestFileAccess() {
        if (checkSelfPermission(this, READ_EXTERNAL_STORAGE) != PERMISSION_GRANTED) {
            ActivityCompat.requestPermissions(this, arrayOf(READ_EXTERNAL_STORAGE), REQUEST_PERMISSIONS_RESULT_CODE)
            return
        }

        readFiles()
    }

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<String>, grantResults: IntArray) {
        when (requestCode) {
            REQUEST_PERMISSIONS_RESULT_CODE -> {
                if ((grantResults.isNotEmpty() && grantResults[0] == PackageManager.PERMISSION_GRANTED)) {
                    readFiles()
                } else {
                    finish()
                }
            }
        }
    }

    fun onTracksLoaded(tracks: List<Track>) {
        Timber.d("onTracksLoaded: %s", tracks)
        this.tracks = tracks;
        adapter.notifyDataSetChanged();
    }

    fun onTrackSelected(track: Track) {
        Timber.d("track selected: %s", track.title)
        onPauseClicked()

        selectedTrack = track
        bytes = track.bytes
        val byteLength = bytes!!.size
        val duration = byteLength * 10
        Timber.d("Selected track %s size: %d bytes (%d ms)", track.title, byteLength, duration)

        selectedTrackTextView.text = track.title
        timerTextView.text = timeFormat.format(0)
        timerTotalTextView.text = String.format(" / %s", timeFormat.format(duration))
    }

    internal inner class TracksAdapter : RecyclerView.Adapter<TracksAdapter.ViewHolder>() {

        private val onItemClickListener = OnItemClickListener()

        override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): TracksAdapter.ViewHolder {
            val view = LayoutInflater.from(parent.context).inflate(R.layout.row_track, parent, false)
            view.setOnClickListener(onItemClickListener)
            return ViewHolder(view)
        }

        override fun onBindViewHolder(holder: TracksAdapter.ViewHolder, position: Int) {
            holder.bindItems(tracks[position])
        }

        inner class ViewHolder(itemView: View) : RecyclerView.ViewHolder(itemView) {
            fun bindItems(track: Track) {
                itemView.tvTitle.text = track.title
            }
        }

        internal inner class OnItemClickListener : View.OnClickListener {
            override fun onClick(view: View) = onTrackSelected(tracks.get(recyclerview.getChildLayoutPosition(view)))
        }

        override fun getItemCount() = tracks.size
    }

    override fun onDestroy() {
        bt.stopAutoConnect()
        bt.stopService();
        super.onDestroy()
    }
}
