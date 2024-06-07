## Compiling for GNU Radio

The default targetted version is GNU Radio 3.10 (``main`` branch). For GNU Radio 3.8,
insert ``git checkout 3.8`` after the ``git clone ...`` command and check the 3.8 branch
version of the README.md for ``LD_LIBRARY_PATH`` and ``PYTHONPATH`` tested on Debian/stable.

```
git clone --recursive https://github.com/M17-Project/gr-m17
cd gr-m17
mkdir build
cd build
cmake ../
make -j12
sudo make install
```

will finish with a statement such as
```
-- Set runtime path of "/usr/local/lib/python3.11/dist-packages/gnuradio/m17/m17_python.cpython-311-x86_64-linux-gnu.so" to ""
```
meaning that variables must be set (tested with Debian/sid) to help GNU Radio Companion find the Python libraries:

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

When running the flowgraph found in ``examples`` with ``gnuradio-companion ../examples/m17_streamer.grc`` 

<img src="examples/m17_loopback.png">

See <a href="examples/README.md">examples/README.md</a> for the expected output and unit testing examples.

Notice that due to the verbose output of ``gr-m17`` and the slow console of GNU Radio Companion, I 
would strongly advise generating the Python script from GNU Radio Companion and then execute 
``python3 m17_streamer.py`` from a terminal to avoid waiting for a long time for GNU Radio 
Companion to flush all messages.

## Developer note1

**Warning**: the default ``gr_modtool`` output informs GNU Radio Companion to ``import m17`` rather 
than ``from gnuradio import m17``. This has to be changed in the YML files manually as the template
is erroneous.

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


## Developer note2

The coder block is an interpolating block outputing 24 more times samples than input symbols. The (well named) ``noutput_items``
is the **output** buffer size which fills much faster than the input stream so we fill ``out`` until ``noutput_items`` are reached, then
send this to the GNU Radio scheduler, and consume the few input samples needed to fill the output buffer. The ring buffer mechanism of GNU Radio makes sure the dataflow is consistent.
