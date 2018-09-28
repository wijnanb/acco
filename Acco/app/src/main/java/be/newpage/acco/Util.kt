package be.newpage.acco

import kotlin.math.ceil

class Util {

    fun divideAndCeil(a:Int, b:Int):Int {
        return ceil(a.toFloat() / b.toFloat()).toInt();
    }

}