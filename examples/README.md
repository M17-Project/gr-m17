## gr-m17 examples

### m17_rx.grc

This receiver testing example relies on ``../M17_Implementations/SP5WWP/m17-coder/m17-coder-sym``
to broadcast a signal. Following this unit testing example, run this flowchart along with
```
mkfifo /tmp/fifo1
mkfifo /tmp/fifo2
python3 ../M17_Implementations/SP5WWP/grc/m17_streamer.py
../M17_Implementations/SP5WWP/m17-coder/m17-coder-sym < /tmp/fifo1 > /tmp/fifo2 &
```

and the output of ``python3 ./m17_rx.py`` should be 

```
...
DST: ALL       SRC: AB1CDE    TYPE: 0005 META: 0000000000000000000000000000 LSF_CRC_OK 
DST: ALL       SRC: AB1CDE    TYPE: 0005 META: 0000000000000000000000000000 LSF_CRC_OK 
DST: ALL       SRC: AB1CDE    TYPE: 0005 META: 0000000000000000000000000000 LSF_CRC_OK 
DST: ALL       SRC: AB1CDE    TYPE: 0005 META: 0000000000000000000000000000 LSF_CRC_OK 
DST: ALL       SRC: AB1CDE    TYPE: 0005 META: 0000000000000000000000000000 LSF_CRC_OK 
...
```

It would be wise to execute from a terminal rather than the GNU Radio Companion graphical interface
to avoid flooding the console.

### transmitterPLUTOSDR.grc
M17 transmitter with ADALM Pluto SDR.

### receiverRTLSDR.grc
M17 receiver with RTL SDR. Automatic Frequency Correction can be enabled as an option.

### m17_streamer.grc
Old loopback demo.

<img src="m17_loopback.png">

### m17_loopback_noisychannel.grc
Loopback demo with the addition of noise, no modulation/no channnel.

<img src="m17_loopback_noisy.png">

### m17_loopback_noisychannel.grc
Loopback demo with a noisy channel, including full modulation & demodulation.

<img src="m17_loopback_noisychannel.png">

