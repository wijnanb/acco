package be.newpage.acco

import android.content.Context
import android.content.SharedPreferences

class Preferences (context: Context) {
    val FILENAME = "be.newpage.acco.preferences"
    val TEMPO = "tempo"
    val sharedPreferences: SharedPreferences = context.getSharedPreferences(FILENAME, 0);

    var tempo: Int
        get() = sharedPreferences.getInt(TEMPO, 10)
        set(value) = sharedPreferences.edit().putInt(TEMPO, value).apply()
}