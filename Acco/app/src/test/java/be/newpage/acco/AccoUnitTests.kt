package be.newpage.acco

import org.junit.Assert.assertEquals
import org.junit.Test

class AccoUnitTests {

    @Test
    fun divideIntegers() {
        assertEquals(34, Util().divideAndCeil(100,3))
        assertEquals(25, Util().divideAndCeil(100,4))
        assertEquals(4, Util().divideAndCeil(16,5))
    }
}
