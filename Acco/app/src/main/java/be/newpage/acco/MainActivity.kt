package be.newpage.acco

import android.Manifest.permission.READ_EXTERNAL_STORAGE
import android.content.pm.PackageManager
import android.content.pm.PackageManager.PERMISSION_GRANTED
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.support.v4.app.ActivityCompat
import android.support.v4.content.ContextCompat.checkSelfPermission
import android.support.v7.app.AlertDialog
import android.support.v7.app.AppCompatActivity
import android.support.v7.widget.LinearLayoutManager
import android.support.v7.widget.RecyclerView
import android.util.Log
import android.view.LayoutInflater
import android.view.View
import android.view.View.*
import android.view.ViewGroup
import android.widget.SeekBar
import android.widget.Toast
import app.akexorcist.bluetotohspp.library.BluetoothSPP
import app.akexorcist.bluetotohspp.library.BluetoothSPP.AutoConnectionListener
import app.akexorcist.bluetotohspp.library.BluetoothSPP.BluetoothConnectionListener
import app.akexorcist.bluetotohspp.library.BluetoothState
import kotlinx.android.synthetic.main.activity_main.*
import kotlinx.android.synthetic.main.dialog_tempo.view.*
import kotlinx.android.synthetic.main.row_track.view.*
import timber.log.Timber
import java.text.SimpleDateFormat
import java.util.*
import java.util.concurrent.Executors
import java.util.concurrent.ScheduledExecutorService
import java.util.concurrent.ScheduledFuture
import java.util.concurrent.TimeUnit
import kotlin.collections.ArrayList


class MainActivity : AppCompatActivity() {

    private val SYNC_BYTE: Byte = 0xFF.toByte()
    private val TEMPO_BYTE: Byte = 0xEF.toByte()
    private val START_BYTE: Byte = 0x3F.toByte()
    private val END_BYTE: Byte = 0x7F.toByte()
    private val NUL_BYTE: Byte = 0x00.toByte()

    private val START_DELAY: Long = 2000 //ms
    private val MAX_TICK_DELAY = 30 //ms
    private val MIN_TICK_DELAY = 3 //ms
    private var tickDelay: Int = MyApplication.preferences.tempo;//ms

    private val REQUEST_PERMISSIONS_RESULT_CODE: Int = 1
    private val timeFormat: SimpleDateFormat = SimpleDateFormat("m:ss", Locale.FRANCE)

    internal var tracks: List<Track> = ArrayList()
    internal var layoutManager: LinearLayoutManager = LinearLayoutManager(this)
    internal var adapter: RecyclerView.Adapter<*> = TracksAdapter()
    internal var connectedDeviceName: String? = null

    internal var selectedTrack: Track? = null
    internal var index = 0

    internal var noteIndex = 0
    internal var timeIndex = 0
    internal var bytes: ArrayList<Byte> = ArrayList()

    internal var songLength = 0
    internal var times: IntArray = IntArray(0)
    internal var timesBytes: ByteArray = ByteArray(0)
    internal var notes: ByteArray = ByteArray(0)


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
        tempoTextView.setOnClickListener { onTempoClicked() }

        requestFileAccess()

        connectWithBluetoothDevice()
    }

    fun onPauseClicked() {
        delayTextView.visibility = INVISIBLE
        pauseIcon.visibility = INVISIBLE
        playIcon.visibility = VISIBLE

        tempoTextView.text = getString(R.string.with_tempo_ms, tickDelay)
        tempoTextView.visibility = VISIBLE
        stop()
    }

    fun onPlayClicked() {
        if (connectedDeviceName == null) {
            Toast.makeText(this, getString(R.string.bluetooth_not_connected), Toast.LENGTH_SHORT).show()
            return
        }

        if (selectedTrack != null) start()
        playIcon.visibility = INVISIBLE
        pauseIcon.visibility = VISIBLE
        tempoTextView.visibility = GONE
    }

    fun onTempoClicked() {
        val context = this
        val view = layoutInflater.inflate(R.layout.dialog_tempo, null)
        view.seekbar.max = MAX_TICK_DELAY - MIN_TICK_DELAY
        view.seekbar.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(seekbar: SeekBar?, progress: Int, fromUser: Boolean) {
                view.textView.text = getString(R.string.ms, MIN_TICK_DELAY + progress)
            }

            override fun onStartTrackingTouch(p0: SeekBar?) {}
            override fun onStopTrackingTouch(p0: SeekBar?) {}
        })
        view.seekbar.progress = tickDelay - MIN_TICK_DELAY

        AlertDialog.Builder(context)
                .setTitle(getString(R.string.dialog_tempo_title))
                .setView(view)
                .setPositiveButton(android.R.string.ok) { dialog, i ->
                    tickDelay = MIN_TICK_DELAY + view.seekbar.progress
                    MyApplication.preferences.tempo = tickDelay
                    dialog.dismiss()
                    onTrackSelected(selectedTrack!!)
                }
                .setNegativeButton(android.R.string.cancel) { dialog, i -> dialog.cancel() }
                .show()
    }

    fun updateBluetoothState(state: String, warning: Boolean = false) {
        bluetoothStatusTextView.text = state;
        warningIcon.visibility = if (warning) VISIBLE else INVISIBLE

        Timber.log(if (warning) Log.ERROR else Log.DEBUG, "BT state: %s", state)
    }

    fun connectWithBluetoothDevice() {
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
                connectedDeviceName = null
            }

            override fun onDeviceConnectionFailed() {
                Timber.d("BT connection: device connection failed")
                connectedDeviceName = null
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
        timeIndex = 0
        noteIndex = 0

        timerTextView.text = timeFormat.format(0)
        delayTextView.text = getString(R.string.seconds_start_delay, TimeUnit.SECONDS.convert(START_DELAY, TimeUnit.MILLISECONDS))
        delayTextView.visibility = VISIBLE

        Timber.d("start '%s' with %dms delay", selectedTrack?.title, START_DELAY)
        scheduleHandler = scheduleTaskExecutor.scheduleAtFixedRate({ onTick() }, START_DELAY, tickDelay.toLong(), TimeUnit.MILLISECONDS)
    }

    fun stop() {
        if (selectedTrack != null) {
            Timber.d("stop '%s'", selectedTrack?.title)
            scheduleHandler?.cancel(true)
        }
    }

    fun onTick() {
        try {
            if (index == 0) delayTextView.visibility = INVISIBLE
            //if (index % 100 == 0) handler.post { timerTextView.text = timeFormat.format(time) }

            if (noteIndex == 0 || timeIndex >= times.get(noteIndex - 1)) {

                if (noteIndex == 0) {
                    sendBytes(bytes.subList(0, 4).toByteArray())
                }

                sendBytes(bytes.subList(index, index+9).toByteArray())

                noteIndex++
                timeIndex = 0
                if (index == bytes.size) onEnded()
            } else {
                timeIndex++
            }
        } catch (e: Exception) {
            Timber.e(e)
        }
    }

    fun onEnded() {
        Timber.d("onEnded")
        onPauseClicked()
    }

    fun requestFileAccess() {
        if (checkSelfPermission(this, READ_EXTERNAL_STORAGE) != PERMISSION_GRANTED) {
            ActivityCompat.requestPermissions(this, arrayOf(READ_EXTERNAL_STORAGE), REQUEST_PERMISSIONS_RESULT_CODE)
            return
        }

        readFiles()
    }

    fun sendBytes(list:ByteArray) {
        bt.send(list, false)

        val output = StringBuilder()
        for (b in list) {
            output.append(String.format("%02X ", b))
        }

        Timber.d("Tick block #%d %s waited %d ticks index: %d send 8 bytes: %s", noteIndex, System.currentTimeMillis(), timeIndex, index, output.toString())
        index += list.size;
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
        val byteLength = track.bytes.size


        val numReg = 7
        songLength = (byteLength - 3) / 8
        times = IntArray(songLength)
        timesBytes = ByteArray(songLength)
        notes = ByteArray(songLength * 7)

        for (i in 0..songLength - 1) {
            val b = track.bytes.get(songLength * numReg + i + 2)
            val time = b.toPositiveInt()
            times.set(i, time)
            timesBytes.set(i, b)
        }

        for (i in 0..songLength * numReg - 1) {
            notes.set(i, track.bytes.get(i + 1))
        }


        // first 2 bytes are to set tempo
        bytes.add(TEMPO_BYTE)
        bytes.add(tickDelay.toByte()) // byte is signed (-127..128) but not important for value of 3..30
        bytes.add(START_BYTE)
        bytes.add(NUL_BYTE)

        var duration = 0
        for (i in 0..songLength - 1) {
            bytes.add(SYNC_BYTE)
            bytes.add(timesBytes.get(i))
            duration += tickDelay
            for (j in 0..6) {
                bytes.add(notes.get(i * 7 + j))
            }
        }

        bytes.add(NUL_BYTE)
        bytes.add(END_BYTE)

        Timber.d("Selected track %s size: %d bytes (%d ms)", track.title, bytes.size, duration)

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
        stop()
        super.onDestroy()
    }

    fun Byte.toPositiveInt() = toInt() and 0xFF
}
