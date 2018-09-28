package be.newpage.acco

import android.app.Application
import timber.log.Timber
import timber.log.Timber.DebugTree

class MyApplication : Application() {

    companion object {
        lateinit var preferences: Preferences
    }

    override fun onCreate() {
        preferences = Preferences(applicationContext)
        super.onCreate()

        Timber.plant(DebugTree())
    }
}