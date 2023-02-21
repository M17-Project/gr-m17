## Compiling:

```
cd build
cmake ../
make -j12
sudo make install
```

will finish with a statement such as
```
-- Set runtime path of "/usr/local/lib/python3/dist-packages/m17/_m17_swig.so" to ""
```
meaning that variables must be set to help GNU Radio Companion find the Python libraries:

```
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib/x86_64-linux-gnu/
export PYTHONPATH=/usr/local/lib/python3/dist-packages/
```

where the ``LD_LIBRARY_PATH`` setting results from

```
find /usr/local/ -name '*m17.so*' -print
```

When running the flowgraph found in ``examples`` 

<img src="examples/m17_streamer.png">

the output should be

```
Generating: '.../gr-m17/examples/m17_streamer.py'

Executing: /usr/bin/python3 -u .../gr-m17/examples/m17_streamer.py

new meta: helloworld
new SRC ID: 0 0 31 36 93 81
new DST ID: 255 255 255 255 0 0
New sampling rate: 192000,000000
new type: 0 5
got_lsf=1
LSF
DST: GAA RTXTT SRC: S0-P7JBRI TYPE: 8EE0 META: CEF0BE521FF9BA43577F6F7A9AA1 LSF_CRC_ERR
LSF
DST:           SRC: AB1CDEBRI TYPE: 0500 META: 68656C6C6F776F726C6400000000 LSF_CRC_OK 
FN: 0000 PLD: 00000000000000000000000000000000
FN: 003D PLD: 00000000000000000000000000000000
FN: 003E PLD: 00000000000000000000000000000000
FN: 003F PLD: 00000000000000000000000000000000
FN: 0040 PLD: 00000000000000000000000000000000
DST:           SRC: AB1CDEBRI TYPE: 0500 META: 68656C6C6F776F726C6400000000 LSF_CRC_OK 
FN: 0041 PLD: 00000000000000000000000000000000
FN: 0042 PLD: 00000000000000000000000000000000
FN: 0043 PLD: 00000000000000000000000000000000
FN: 009A PLD: 00000000000000000000000000000000
FN: 009B PLD: 00000000000000000000000000000000
FN: 009C PLD: 00000000000000000000000000000000
FN: 009D PLD: 00000000000000000000000000000000
FN: 009E PLD: 00000000000000000000000000000000
```
