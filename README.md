## Compiling:

```
cd build
cmake ../
make -j12
sudo make install
```

will finish with a statement such as
```
-- Set runtime path of "/usr/local/lib/python3.11/dist-packages/gnuradio/m17/m17_python.cpython-311-x86_64-linux-gnu.so" to ""
```
meaning that variables must be set to help GNU Radio Companion find the Python libraries:

```
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib/x86_64-linux-gnu/
export PYTHONPATH=/usr/local/lib/python3.11/dist-packages/
```

where the ``LD_LIBRARY_PATH`` setting results from

```
find /usr/local/ -name libgnuradio-m17.so.1.0.0 -print
```

When running the flowgraph found in ``examples`` 

<img src="examples/m17_streamer.png">

the output should be

```
Generating: '.../M17_Implementations/SP5WWP/gr-m17/examples/m17_streamer.py'

Executing: /usr/bin/python3 -u .../M17_Implementations/SP5WWP/gr-m17/examples/m17_streamer.py

new meta: helloworld
new SRC ID: 1 2 3 4 5 6
new DST ID: 255 255 255 255 255 255
New sampling rate: 8800.000000
new type: 0 0
got_lsf=1
LSF
DST: #BCAST    SRC: FPR-4U3F  TYPE: 0000 META: 68656C6C6F776F726C6400000000 LSF_CRC_OK 
FN: 0000 PLD: 00000000000000000000000000000000
FN: 0001 PLD: 00000000000000000000000000000000
FN: 0002 PLD: 00000000000000000000000000000000
FN: 0003 PLD: 00000000000000000000000000000000
FN: 0004 PLD: 00000000000000000000000000000000
DST: #BCAST    SRC: FPR-4U3F  TYPE: 0000 META: 68656C6C6F776F726C6400000000 LSF_CRC_OK 
FN: 0005 PLD: 00000000000000000000000000000000
FN: 0006 PLD: 00000000000000000000000000000000
FN: 0007 PLD: 00000000000000000000000000000000
FN: 0008 PLD: 00000000000000000000000000000000
FN: 0009 PLD: 00000000000000000000000000000000
FN: 000A PLD: 00000000000000000000000000000000
DST: #BCAST    SRC: FPR-4U3F  TYPE: 0000 META: 68656C6C6F776F726C6400000000 LSF_CRC_OK 
FN: 000B PLD: 00000000000000000000000000000000
FN: 000C PLD: 00000000000000000000000000000000
FN: 000D PLD: 00000000000000000000000000000000
FN: 000E PLD: 00000000000000000000000000000000
FN: 000F PLD: 00000000000000000000000000000000
```
but will end up desynchronizing at some point if not straight from the beginning.

**Warning**: the default gr_modtool informs GNU Radio Companion to ``import m17`` rather than
``from gnuradio import m17``. This has to be changed in the YML files manually as the template
is erroneous.


TODO:
* Solve short naming issue in 3.8 (int ?)
* Check non-16 multiple buffer size -- cause of desynchronization? At least avoid crash !
* Change META as a variable and not a constant in the block
* Test on 3.8
* How to handle fixed symbol rate but varying output rate?
* How to modulate the RF transmitted with the real output?
