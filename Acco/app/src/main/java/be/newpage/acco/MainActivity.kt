package be.newpage.acco

import android.Manifest.permission.READ_EXTERNAL_STORAGE
import android.bluetooth.BluetoothDevice
import android.content.pm.PackageManager
import android.content.pm.PackageManager.PERMISSION_GRANTED
import android.os.Bundle
import android.support.v4.app.ActivityCompat
import android.support.v4.content.ContextCompat.checkSelfPermission
import android.support.v7.app.AppCompatActivity
import android.support.v7.widget.LinearLayoutManager
import android.support.v7.widget.RecyclerView
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import app.akexorcist.bluetotohspp.library.BluetoothSPP
import app.akexorcist.bluetotohspp.library.BluetoothSPP.AutoConnectionListener
import app.akexorcist.bluetotohspp.library.BluetoothSPP.BluetoothConnectionListener
import app.akexorcist.bluetotohspp.library.BluetoothState
import kotlinx.android.synthetic.main.activity_main.*
import kotlinx.android.synthetic.main.row_track.view.*
import kotlinx.android.synthetic.main.toolbar.*
import timber.log.Timber


class MainActivity : AppCompatActivity() {

    private val REQUEST_PERMISSIONS_RESULT_CODE: Int = 1

    internal var tracks: List<Track> = ArrayList()
    internal var layoutManager: LinearLayoutManager = LinearLayoutManager(this)
    internal var adapter: RecyclerView.Adapter<*> = TracksAdapter()
    internal var currentTickTackTock: TickTackTock? = null
    internal var connectedDevice: BluetoothDevice? = null;

    internal val bt = BluetoothSPP(this)

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        setSupportActionBar(toolbar)

        recyclerview.setHasFixedSize(true)
        recyclerview.setLayoutManager(layoutManager)
        recyclerview.setAdapter(adapter)

        requestFileAccess()

        connectWithBluetoothDevice()
    }

    private fun connectWithBluetoothDevice() {

        if (!bt.isBluetoothAvailable()) {
            Timber.e("bluetooth not available")
            return;
        }

        if (!bt.isBluetoothEnabled()) {
            Timber.e("bluetooth not enabled")
            return;
        }

        bt.setupService();
        bt.startService(BluetoothState.DEVICE_OTHER);

        bt.autoConnect("HC-05")

        bt.setBluetoothConnectionListener(object : BluetoothConnectionListener {
            override fun onDeviceConnected(name: String, address: String) {
                Timber.d("BT connection: device connected %s %s", name, address)
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

        bt.setBluetoothStateListener({
            if (it == BluetoothState.STATE_CONNECTED) {
                Timber.d("BT state: connected")
            } else if (it == BluetoothState.STATE_CONNECTING) {
                Timber.d("BT state: connecting")
            } else if (it == BluetoothState.STATE_LISTEN) {
                Timber.d("BT state: listening")
            } else if (it == BluetoothState.STATE_NONE) {
                Timber.d("BT state: none")
            }
        })


//        val bluetoothAdapter = BluetoothAdapter.getDefaultAdapter();
//
//        val pairedDevices = bluetoothAdapter.getBondedDevices()
//
//        if (pairedDevices.size > 0) {
//            for (device in pairedDevices) {
//                if (device.name.equals("HC-05", true)) {
//                    connectedDevice = device;
//                    Timber.d("Connected with HC-05 Bluetooth device.")
//                    Toast.makeText(this, "Connected with HC-05 Bluetooth device.", Toast.LENGTH_LONG).show()
//                }
//            }
//        } else {
//            Timber.e("No paired HC-05 Bluetooth devices found.")
//            Toast.makeText(this, "No paired HC-05 Bluetooth devices found.", Toast.LENGTH_LONG).show()
//        }
    }

    fun readFiles() {
        val files = FileAccess().getDirectoryListing()
        val tracks = ArrayList<Track>()

        for (file in files) {
            tracks.add(Track(file.nameWithoutExtension, file))
        }

        onTracksLoaded(tracks);
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

//        val contents = FileAccess().readFileContentsAsString(track.file)
//        Timber.d("file contents:")
//        Timber.d(contents)

        currentTickTackTock?.stop()

        val bytes = FileAccess().readFileContentsAsBytes(track.file)
        Timber.d("starting service with track %s size: %d bytes", track.title, bytes.size)

        Timber.d("sending bytes")
        bt.send(bytes, false)
        Timber.d("sending bytes finished")

//        currentTickTackTock = TickTackTock(track, connectedDevice)
//        currentTickTackTock?.start()
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
