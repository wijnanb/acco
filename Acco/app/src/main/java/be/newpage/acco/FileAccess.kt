package be.newpage.acco

import android.os.Environment
import android.os.Environment.DIRECTORY_DOCUMENTS
import timber.log.Timber
import java.io.File
import java.io.FileInputStream

class FileAccess {

    private val DIRECTORY: String = "Acco"

    fun getDirectoryListing(): List<File> {
        val dir = getPublicAlbumStorageDir(DIRECTORY)

        if (!dir?.exists()!!) {
            Timber.e("directory does not exist %s", dir.absolutePath)
            return ArrayList()
        }

        Timber.d("listing files for: %s", dir.absolutePath)
        val list = dir.listFiles().toCollection(ArrayList())

        for (file in list) {
            Timber.d("file: %s %.1fKB", file.absolutePath, file.length() / 1024f)
        }

        return list;
    }

    fun readFileContentsAsString(file: File) = FileInputStream(file).bufferedReader().use { it.readText() }

    fun readFileContentsAsBytes(file: File) = FileInputStream(file).use { it.readBytes() }

    /* Checks if external storage is available for read and write */
    fun isExternalStorageWritable(): Boolean {
        return Environment.getExternalStorageState() == Environment.MEDIA_MOUNTED
    }

    /* Checks if external storage is available to at least read */
    fun isExternalStorageReadable(): Boolean {
        return Environment.getExternalStorageState() in
                setOf(Environment.MEDIA_MOUNTED, Environment.MEDIA_MOUNTED_READ_ONLY)
    }

    fun getPublicAlbumStorageDir(albumName: String): File? {
        // Get the directory for the user's public documents directory.
        val file = File(Environment.getExternalStoragePublicDirectory(DIRECTORY_DOCUMENTS), albumName)
        return file
    }
}