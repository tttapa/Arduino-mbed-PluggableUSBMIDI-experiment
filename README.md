# Arduino mbed PluggableUSBMIDI experiment

Code for https://github.com/arduino/ArduinoCore-mbed/issues/129.

This is a simplified USB-MIDI implementation for Arduino boards using the 
ArduinoCore-mbed core.

There seem to be issues with the received packets on the Arduino Nano 33 BLE,
the packet sizes are often incorrect. The code in this repository demonstrates 
the issue.

## Reproducing the problem

1. Attach a USB-to-UART adapter to the `Serial1` port of the Arduino, it will be
   used for printing debug information and assertion errors. Open it at 115,200 
   baud.
2. Upload the sketch in this repository.
3. Run `amidi -l` to find the ALSA MIDI port number of the Arduino (on Linux).
4. Send the MIDI SysEx message in this repository using
   `amidi -p hw:1,0,0 -s MIDI-sequential-SysEx.syx`, replacing `hw:1,0,0` with
   the correct port number from the previous step.
5. Inspect the output of `Serial1`. Send the MIDI message multiple times until
   it fails with an assertion error (use the reset button to recover from the 
   error to try again).

It seems to fail randomly about 50% of the time. I've added the output of both
the success and the failure cases with some annotations below:

### Correct output
```
64        ← USB packet size (printed in USB interrupt)
rsi       ← USB read start from interrupt
64
rsi
64
          ↓ USB packet data (printed in main loop)
04 F0 00 01  04 02 03 04  04 05 06 07  04 08 09 0A  04 0B 0C 0D  04 0E 0F 10  04 11 12 13  04 14 15 16  04 17 18 19  04 1A 1B 1C  04 1D 1E 1F  04 20 21 22  04 23 24 25  04 26 27 28  04 29 2A 2B  04 2C 2D 2E  
rsm       ← USB read start from main loop
64
04 2F 30 31  04 32 33 34  04 35 36 37  04 38 39 3A  04 3B 3C 3D  04 3E 3F 40  04 41 42 43  04 44 45 46  04 47 48 49  04 4A 4B 4C  04 4D 4E 4F  04 50 51 52  04 53 54 55  04 56 57 58  04 59 5A 5B  04 5C 5D 5E  
rsm
64
04 5F 60 61  04 62 63 64  04 65 66 67  04 68 69 6A  04 6B 6C 6D  04 6E 6F 70  04 71 72 73  04 74 75 76  04 77 78 79  04 7A 7B 7C  04 7D 7E 00  04 01 02 03  04 04 05 06  04 07 08 09  04 0A 0B 0C  04 0D 0E 0F  
rsm
24
04 10 11 12  04 13 14 15  04 16 17 18  04 19 1A 1B  04 1C 1D 1E  04 1F 20 21  04 22 23 24  04 25 26 27  04 28 29 2A  04 2B 2C 2D  04 2E 2F 30  04 31 32 33  04 34 35 36  04 37 38 39  04 3A 3B 3C  04 3D 3E 3F  
rsm
04 40 41 42  04 43 44 45  04 46 47 48  04 49 4A 4B  04 4C 4D 4E  04 4F 50 51  04 52 53 54  04 55 56 57  04 58 59 5A  04 5B 5C 5D  04 5E 5F 60  04 61 62 63  04 64 65 66  04 67 68 69  04 6A 6B 6C  04 6D 6E 6F  
04 70 71 72  04 73 74 75  04 76 77 78  04 79 7A 7B  04 7C 7D 7E  05 F7 00 00
```

### Incorrect output
```
64
rsi
64
rsi
64
04 F0 00 01  04 02 03 04  04 05 06 07  04 08 09 0A  04 0B 0C 0D  04 0E 0F 10  04 11 12 13  04 14 15 16  04 17 18 19  04 1A 1B 1C  04 1D 1E 1F  04 20 21 22  04 23 24 25  04 26 27 28  04 29 2A 2B  04 2C 2D 2E  
rsm
64
04 2F 30 31  04 32 33 34  04 35 36 37  04 38 39 3A  04 3B 3C 3D  04 3E 3F 40  04 41 42 43  04 44 45 46  04 47 48 49  04 4A 4B 4C  04 4D 4E 4F  04 50 51 52  04 53 54 55  04 56 57 58  04 59 5A 5B  04 5C 5D 5E  
rsm
24        ← Incorrect USB packet size
04 5F 60 61  04 62 63 64  04 65 66 67  04 68 69 6A  04 6B 6C 6D  04 6E 6F 70  04 71 72 73  04 74 75 76  04 77 78 79  04 7A 7B 7C  04 7D 7E 00  04 01 02 03  04 04 05 06  04 07 08 09  04 0A 0B 0C  04 0D 0E 0F  
rsm
24
04 10 11 12  04 13 14 15  04 16 17 18  04 19 1A 1B  04 1C 1D 1E  04 1F 20 21  04 22 23 24  04 25 26 27  04 28 29 2A  04 2B 2C 2D  04 2E 2F 30  04 31 32 33  04 34 35 36  04 37 38 39  04 3A 3B 3C  04 3D 3E 3F  
rsm
          ↓ USB packet data truncated because of incorrect size
04 40 41 42  04 43 44 45  04 46 47 48  04 49 4A 4B  04 4C 4D 4E  04 4F 50 51  
04 70 71 72  04 73 74 75  04 76 77 78  04 79 7A 7B  04 7C 7D 7E  05 F7 00 00  


++ MbedOS Error Info ++
Error Status: 0x80FF0144 Code: 324 Module: 255
Error Message: Assertion failed: p[1] == verify_cnt
Location: 0x15D0D
File: /tmp/arduino_build_936669/sketch/PluggableUSBMIDI.cpp+110
Error Value: 0x0
Current Thread: main Id: 0x2000A434 Entry: 0x16EDF StackSize: 0x8000 StackMem: 0x20002410 SP: 0x2000A354 
For more info, visit: https://mbed.com/s/error?error=0x80FF0144&tgt=ARDUINO_NANO33BLE
-- MbedOS Error Info --
```

## Explanation of the code

`PluggableUSBMIDI` is a class that inherits from the
`arduino::internal::PluggableUSBModule` class, it has USB-MIDI descriptors and 
supports reading incoming MIDI only. The reading logic consists of a circular
FIFO queue. Each element of the queue consists of a buffer containing the USB 
packet data and a size containing the number of bytes received in the buffer.
The data is written to the buffer by the USB stack, when the packet reception is
done, a USB interrupt calls back `PluggableUSBMIDI::out_callback()` which 
finishes the read and stores the size and the buffer into the FIFO. The main 
program can then pop this buffer from the FIFO and read the data. Whenever 
there's room in the FIFO, the next USB read is started.

## Explanation of the test

The `MIDI-sequential-SysEx.syx` file contains a MIDI System Exclusive message
with the data bytes just incrementing from 0x00 to 0x7E:
`F0 00 01 02 ... 7C 7D 7E F7`.  
`F0` and `F7` are the SysEx start and stop markers respectively.

The test program on the Arduino (more specifically the
`PluggableUSBMIDI::verify_data()` function) simply verifies that the incoming 
data matches this SysEx message. If it doesn't match, it throws an assertion
error.
