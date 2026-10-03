## Compiling for GNU Radio

The default targetted version is GNU Radio 3.10 (``main`` branch). Tested on Debian/GNU Linux sid with GNU Radio 
3.10.10.0 (Python 3.11.9), Ubuntu 24.04 LTS with GNU Radio 3.10.9.2, and Xubuntu 26.04 with GNU Radio 3.10.12.0 (Python 3.14.4), assuming the following
dependencies are installed:

```
sudo apt install git cmake build-essential doxygen gnuradio gnuradio-dev
```

For compiling ``gr-m17``:
```
git clone --recursive https://github.com/M17-Project/gr-m17
cd gr-m17
mkdir build
cd build
cmake ..
make -j`nproc`
sudo make install
sudo ldconfig
```

If the repository was cloned without ``--recursive``, fetch the submodules with ``git submodule update --init --recursive``
before running ``cmake``.

will finish with a statement such as
```
-- Set runtime path of "/usr/local/lib/python3.11/dist-packages/gnuradio/m17/m17_python.cpython-311-x86_64-linux-gnu.so" to ""
```
If GNU Radio Companion then reports ``ImportError: libgnuradio-m17.so.1.0.0: cannot open shared object file``, make sure
``sudo ldconfig`` was run after ``sudo make install``. Depending on Linux distribution, variables might also have to be set
(tested with Debian/sid, but not needed with Ubuntu 24.04 LTS) to help GNU Radio Companion find the Python libraries:

```
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib/x86_64-linux-gnu/
export PYTHONPATH=/usr/local/lib/python3.11/dist-packages/
```

where the ``LD_LIBRARY_PATH`` setting results from

```
find /usr/local/ -name libgnuradio-m17.so.1.0.0 -print
```

to solve any issue related to ``ImportError: libgnuradio-m17.so.1.0.0: cannot open shared object file: No such file or directory``
(which means that ``/usr/local`` is not part of the GNU Radio Companion paths)

When running the flowgraph found in ``examples`` with ``gnuradio-companion ../examples/m17_loopback.grc`` 

<img src="examples/m17_loopback.png">

See <a href="examples/README.md">examples/README.md</a> for the expected output and unit testing examples.

## Using the blocks

* **Transmission control** (M17 Encoder): in *Messages* mode, a stream is started and ended with ``SOT`` and ``EOT`` messages
on the ``transmission_control`` port, and ``SMS`` messages (e.g. from a QT GUI Msg Push Button, property name ``SMS``, value = text,
up to 821 bytes) send a text message. In *Continuous* mode, the stream starts with the flowgraph and ends when the input ends.
* **META** (M17 Encoder, *Meta* tab, no encryption only): text (up to 13 bytes, UTF-8), GNSS position, extended callsign data,
or raw bytes (hex), selected with *Meta type*. With AES, META carries the nonce.
* **Keys and seeds** are entered as hex strings (spaces allowed): AES key 32/48/64 hex digits, scrambler seed 2/4/6 hex digits
(= 8/16/24-bit), ECDSA private key (encoder) 64 hex digits, public key (decoder) 128 hex digits. With *Debug* enabled, the
encoder prints the public key derived from the private key.
* **M17 Decoder**: the encryption type and key size are taken from the received stream; enter the AES key and/or scrambler seed
needed. Frames over the Viterbi threshold are output as Codec2 encoded silence; frames that cannot be decrypted and signature
frames are output as Codec2 silence, zeros or nothing (selectable).
* **Console output**: one line per event, ``HH:MM:SS.mmm [TAG] message``, with ``TAG`` being ``M17_ENC``/``M17_DEC``
(or the block alias, if set). *Debug data*/*Debug control* add per-frame details.

## Developer notes

In case of error related to ``Python bindings for m17_coder.h are out of sync`` after changing
header files in ``include/gnuradio/m17``, make sure that 
```
md5sum include/gnuradio/m17/m17_decoder.h
```
match the information in ``python/m17/bindings/*cc``.

Rather than manually changing the md5sum, the proper way of handling bindings in the Python directory is to execute
```
gr_modtool bind m17_decoder
gr_modtool bind m17_coder
``` 
from the ``gr-m17`` directory, assuming ``gr_modtool bind`` works, otherwise check https://github.com/gnuradio/gnuradio/issues/6477

### A note on output generation

The coder block is an interpolating block outputing 12 times more symbols than input bytes (16 bytes per frame become 192 symbols). The (well named) ``noutput_items``
is the **output** buffer size which fills much faster than the input stream so we fill ``out`` until ``noutput_items`` are reached, then
send this to the GNU Radio scheduler, and consume the few input samples needed to fill the output buffer. The ring buffer mechanism of GNU Radio makes sure the dataflow is consistent.
