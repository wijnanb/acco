package be.newpage.acco

import java.io.File

class Track(val title: String, val file: File) {

    val bytes: ByteArray
        get() = FileAccess().readFileContentsAsBytes(file)
}
