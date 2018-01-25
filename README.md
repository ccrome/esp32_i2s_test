# esp32 I2S port verification program.

This program tests the esp32 I2S port to verify that it can perfectly,
sample accurately transmit and receive without loosing any samples.

The basic idea is to run what I call the 'ramp' test.  It sends out a
'ramp' or sawtooth wave out the TX port, and reads back from the RX
port.  It verifies that what is received is a perfect, but delayed
version of what got sent out.

## Hardware Setup
Simply connect IO pin 32 to pin 33 on your esp32 module.

## To Compile and upload
```platformio run --target upload```

## Expected output
```
delay tx_err rx_err ramp_err framing_err channel_sync_err
192 0 0 0 192
192 0 0 0 0
192 0 0 0 0
192 0 0 0 0
....
```

In this case, the DMA buffer was 32 samples, and 6 buffers = 192
samples of delay.

The first 192 samples of channel_sync_err is expected because the
first 192 samples are all zeros.

The delay will actually be 192*2 because you don't see the RX buffer
delay, only the tx buffer delay.

