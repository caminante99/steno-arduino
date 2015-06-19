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

/* IO Definitions */
#define CLK      5
#define DATA_IN  4
#define LATCH_EN 3
#define TEST_EN  2

/*
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
 */

// Byte array of depressed keys, following mapping above
// 0 means not pressed, 1 means pressed
char pressed_keys[24];

// Data to send back via serial to host
char data[6];

/*
 * Setup IO and serial connections
 */
void setup()
{
    // Initialize serial
    Serial.begin(115200);

    // Setup IOs
    pinMode(CLK, OUTPUT);
    pinMode(LATCH_EN, OUTPUT);
    pinMode(TEST_EN, OUTPUT);
    pinMode(DATA_IN, INPUT);

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
 * See plover source for more information on the Gemini PR format
 */
void constructData()
{
    data[0] = 0x80;
    data[1] = (pressed_keys[ 0] << 6)  /* S- */
            | (pressed_keys[ 1] << 4)  /* T- */
            | (pressed_keys[ 2] << 3)  /* K- */
            | (pressed_keys[ 3] << 2)  /* P- */
            | (pressed_keys[ 4] << 1)  /* W- */
            | (pressed_keys[ 5] << 0); /* H- */

    data[2] = (pressed_keys[ 6] << 6)  /* R- */
            | (pressed_keys[ 7] << 5)  /* A- */
            | (pressed_keys[ 8] << 4)  /* O- */
            | (pressed_keys[ 9] << 3); /* *  */

    data[3] = (pressed_keys[10] << 3)  /* -E */
            | (pressed_keys[11] << 2)  /* -U */
            | (pressed_keys[12] << 1)  /* -F */
            | (pressed_keys[13] << 0); /* -R */

    data[4] = (pressed_keys[14] << 6)  /* -P */
            | (pressed_keys[15] << 5)  /* -B */
            | (pressed_keys[16] << 4)  /* -L */
            | (pressed_keys[17] << 3)  /* -G */
            | (pressed_keys[18] << 2)  /* -T */
            | (pressed_keys[19] << 1)  /* -S */
            | (pressed_keys[20] << 0); /* -D */

    data[5] = (pressed_keys[22] << 6)  /* #  */
            | (pressed_keys[21] << 0); /* -Z */
}

/*
 * Read keystrokes and return them to the host
 */
void loop()
{
    int i;
    char pressed;
    char keys_down;
    static char in_progress;
    static char returnData;

    if (returnData) {
        constructData();
        Serial.write(data[0]);
        Serial.write(data[1]);
        Serial.write(data[2]);
        Serial.write(data[3]);
        Serial.write(data[4]);
        Serial.write(data[5]);

        /*
         * Data returned to host, reset statful variables
         * Note that we don't need to do this in the setup, becasue they are all
         * static and guaranteed to be 0 by spec
         */
        memset(pressed_keys, 0, 23);
        returnData  = 0;
        in_progress = 0;

    } else {
        // Latch data
        digitalWrite(LATCH_EN, HIGH);

        // Read all latched data
        keys_down = 0;
        for (i = 0; i < 24; i++) {
            /*
             * All inputs are pulled up. Pressing a key shorts the circuit to
             * ground.
             *
             * We invert the logic here to convert to more conventional positive
             * logic.
             */
            pressed          = !digitalRead(DATA_IN);
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
            returnData = 1;
        }
    }
}

