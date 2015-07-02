# Stentura 200 SRT Controller

USB controller for Stentura 200 SRT (and similar) stenotype writers using Arduino microprocessor. Allows the Stentura to be used as a realtime-writer via [Plover](http://stenoknight.com/wiki/Main_Page) and a single USB cable.

## Configuration
- Protocol: Gemini PR
- Baud: 115200
- Format: 8-N-1
- RTS/CTS: No
- Xon/Xoff: No
- Pinout: see source code

## Doesn't the SRT have a serial port?

Yes, it does. There are few reasons why I wrote this program:

1. I didn't have an easily available +12V power supply (it was being used by my router). I didn't want to buy another one without first verifying that the Stentura worked.

2. The Stentura didn't work when I plugged it in. The lights were on, the processor was responsive, but the serial link wasn't doing anything.

3. A single +5V USB cable is much cleaner than a +12V power cable and USB-Serial adapter.

## How it works

The Stentura consists of three main components:

1. Mechanicals: the big hunk of metal with gears and springs. This contains the keyboard and all the parts for standalone operation (via paper tape).

2. IO Expander: sits on top of mechanicals and has the steno alphabet printed on the top. You see it when you open the top cover.

3. Motherboard: screwed on underneath the mechanicals. Has the power, serial, memory, and processor circuitry. Processor communicates to the IO Expander via a ribbon cable and stores chords into memory. Host requests for chords over serial, which processor handles.

Something was wrong with the on-board serial, so I decided to bypass the Motherboard entirely. I soldered leads onto the ribbon cable and drove the IO Expander with an Arduino. The Arduino polled the keys and gave them to the host via the Gemini PR protocol, since that was the most straightforward protocol supported by Plover.

The IO Expander is nothing but a chain of serial shift registers with latches. Each key is weakly pulled up and connected to its own pin on a shift register. When a key is pressed, a little armbar pokes up and shorts the corresponding contact on the IO expander to the case, pulling the signal low.

To sample the keys, the processor locks/disables the latches on all the shift registers, then clocks out the data. Once done, the latches are reenabled/made transparent to capture the next sample.

## Improvements

- Add debounce logic. I haven't really noticed needing this, but it might be necessary when you start to type faster.
