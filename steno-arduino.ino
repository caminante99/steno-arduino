/*
 * Copyright (c) 2015 Kevin Nygaard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
 * Pinout
 * Ribbon
 * ||||||
 * ||||||
 * ||||||
 * xxxxxx (contacts facing up)
 *  \\\\\\_   Power +5V
 *   \\\\\_ 2 Test
 *    \\\\_ 3 Shift load
 *     \\\_ 4 Serial out
 *      \\_ 5 Clock
 *       \_   Ground
 */

/* IO Definitions */
#define CLK      5
#define DATA_IN  4
#define LATCH_EN 3
#define TEST_EN  2
#define BAUD     115200

#define ARRAY_SIZE(array) \
    (sizeof(array) / sizeof(*array))

/*
 * Setup IO and serial connections
 */
void setup()
{
    // Initialize serial
    Serial.begin(BAUD);

    // Setup IOs
    pinMode(CLK,      OUTPUT);
    pinMode(LATCH_EN, OUTPUT);
    pinMode(TEST_EN,  OUTPUT);
    pinMode(DATA_IN,  INPUT);

    // Drive known values to outputs
    digitalWrite(CLK,      LOW);
    digitalWrite(LATCH_EN, LOW);
    digitalWrite(TEST_EN,  LOW);
}

/*
 * Convert raw Stentura byte data into packed Gemini PR format
 *
 * Data that is seemingly skipped is rendundant, and as far as I can tell,
 * unnecessary
 *
 * Raw matrix mapping of Stentura 200 SRT from serial shift registers
 *  0: S-
 *  1: T-
 *  2: K-
 *  3: P-
 *  4: W-
 *  5: H-
 *  6: R-
 *  7: A-
 *  8: O-
 *  9: *
 * 10: -E
 * 11: -U
 * 12: -F
 * 13: -R
 * 14: -P
 * 15: -B
 * 16: -L
 * 17: -G
 * 18: -T
 * 19: -S
 * 20: -D
 * 21: -Z
 * 22: #
 * 23: N/A
 *
 * Gemini PR protocol (from plover source ./plover/machine/geminipr.py)
 *
 * In the Gemini PR protocol, each packet consists of exactly six bytes and the
 * most significant bit (MSB) of every byte is used exclusively to indicate
 * whether that byte is the first byte of the packet (MSB=1) or one of the
 * remaining five bytes of the packet (MSB=0). As such, there are really only
 * seven bits of steno data in each packet byte. This is why the STENO_KEY_CHART
 * below is visually presented as six rows of seven elements instead of six rows
 * of eight elements.
 * STENO_KEY_CHART = ("Fn" , "#" , "#" , "#" , "#" , "#"  , "#"  ,
 *                    "S-" , "S-", "T-", "K-", "P-", "W-" , "H-" ,
 *                    "R-" , "A-", "O-", "*" , "*" , "res", "res",
 *                    "pwr", "*" , "*" , "-E", "-U", "-F" , "-R" ,
 *                    "-P" , "-B", "-L", "-G", "-T", "-S" , "-D" ,
 *                    "#"  , "#" , "#" , "#" , "#" , "#"  , "-Z")
 *
 */
void construct_data(char raw_data[], char packed_data[])
{
    packed_data[0] = 0x80;
    packed_data[1] = (raw_data[ 0] << 6)  /* S- */
                   | (raw_data[ 1] << 4)  /* T- */
                   | (raw_data[ 2] << 3)  /* K- */
                   | (raw_data[ 3] << 2)  /* P- */
                   | (raw_data[ 4] << 1)  /* W- */
                   | (raw_data[ 5] << 0); /* H- */

    packed_data[2] = (raw_data[ 6] << 6)  /* R- */
                   | (raw_data[ 7] << 5)  /* A- */
                   | (raw_data[ 8] << 4)  /* O- */
                   | (raw_data[ 9] << 3); /* *  */

    packed_data[3] = (raw_data[10] << 3)  /* -E */
                   | (raw_data[11] << 2)  /* -U */
                   | (raw_data[12] << 1)  /* -F */
                   | (raw_data[13] << 0); /* -R */

    packed_data[4] = (raw_data[14] << 6)  /* -P */
                   | (raw_data[15] << 5)  /* -B */
                   | (raw_data[16] << 4)  /* -L */
                   | (raw_data[17] << 3)  /* -G */
                   | (raw_data[18] << 2)  /* -T */
                   | (raw_data[19] << 1)  /* -S */
                   | (raw_data[20] << 0); /* -D */

    packed_data[5] = (raw_data[22] << 6)  /* #  */
                   | (raw_data[21] << 0); /* -Z */
}

/*
 * Read keystrokes and return them to the host
 */
void loop()
{
    // Byte array of depressed keys, following mapping above
    // 0 means not pressed, 1 means pressed
    static char pressed_keys[24];

    // Keys packed in Gemini PR format, ready for transmission over serial
    static char packed_keys[6];

    // Chord has been started, but is not complete (all keys released)
    static char in_progress;

    // Send data to host over serial
    static char send_data;

    int i;
    char pressed;
    char keys_down;

    if (send_data) {
        construct_data(pressed_keys, packed_keys);
        Serial.write(packed_keys, ARRAY_SIZE(packed_keys));

        /*
         * Data returned to host, reset stateful variables
         * Note that we don't need to do this in the setup, because they are all
         * static and guaranteed to be 0 by spec
         */
        memset(pressed_keys, 0, sizeof(pressed_keys));
        send_data   = 0;
        in_progress = 0;

    } else {
        // Latch current state of all keys
        digitalWrite(LATCH_EN, HIGH);

        // Read all latched data
        keys_down = 0;
        for (i = 0; i < ARRAY_SIZE(pressed_keys); i++) {
            /*
             * All inputs are pulled up. Pressing a key shorts the circuit to
             * ground.
             *
             * We invert the logic here to convert to more conventional positive
             * logic.
             */
            pressed          = !digitalRead(DATA_IN);

            // Once a key is pressed, it stays pressed until the chord is over
            pressed_keys[i] |= pressed;
            if (pressed) {
                keys_down   = 1;
                in_progress = 1;
            }

            /*
             * Toggle clock. Max frequency of shift register MM74HC165 is 30
             * MHz, so it can be switched without any delay by Arduino.
             */
            digitalWrite(CLK, HIGH);
            digitalWrite(CLK, LOW);
        }

        // Make latch transparent again to capture next set of keystrokes
        digitalWrite(LATCH_EN, LOW);

        // Return data to host when all keys have been released
        if (in_progress && !keys_down) {
            send_data = 1;
        }
    }
}
