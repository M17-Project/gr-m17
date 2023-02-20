/* -*- c++ -*- */

#define M17_API

%include "gnuradio.i"           // the common stuff

//load generated python docstrings
%include "m17_swig_doc.i"

%{
#include "m17/m17_coder.h"
#include "m17/m17_decoder.h"
%}

%include "m17/m17_coder.h"
GR_SWIG_BLOCK_MAGIC2(m17, m17_coder);
%include "m17/m17_decoder.h"
GR_SWIG_BLOCK_MAGIC2(m17, m17_decoder);
