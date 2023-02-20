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
Generating: '.../M17_Implementations/SP5WWP/gr-m17v3.8/examples/m17_streamer.py'

Executing: /usr/bin/python3 -u .../M17_Implementations/SP5WWP/gr-m17v3.8/examples/m17_streamer.py

LSF
DST: GAA RTXTT SRC: S0-P7JBRI TYPE: 8EE0 META: CEF0BE521FF9BA43577F6F7A9AA1 LSF_CRC_ERR
LSF
DST: GAA RTXTT SRC: S0-P7JBRI TYPE: 0000 META: 0000000000000000000000000000 LSF_CRC_OK 
LSF
DST: GAA RTXTT SRC: S0-P7JBRI TYPE: 0000 META: 0000000000000000000000000000 LSF_CRC_OK 
LSF
DST: GAA RTXTT SRC: S0-P7JBRI TYPE: 0000 META: 0000000000000000000000000000 LSF_CRC_OK 
LSF
DST: GAA RTXTT SRC: S0-P7JBRI TYPE: 0000 META: 0000000000000000000000000000 LSF_CRC_OK 
LSF
DST: GAA RTXTT SRC: S0-P7JBRI TYPE: 0000 META: 0000000000000000000000000000 LSF_CRC_OK 
LSF
DST: GAA RTXTT SRC: S0-P7JBRI TYPE: 0000 META: 0000000000000000000000000000 LSF_CRC_OK 

>>> Done
```
