/* -*- c++ -*- */
/*
 * Copyright 2023 jmfriedt.
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

// 240620: todo uncomment #idef AES for cryptography and #ifdef ECC for signature

// in m17_coder_impl.h: #define AES #define ECC

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gnuradio/io_signature.h>
#include "m17_coder_impl.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#include "m17.h"

#ifdef AES
#include "aes.h"
#endif

#ifdef ECC
#include "uECC.h"
#endif

namespace gr
{
  namespace m17
  {

    m17_coder::sptr
      m17_coder::make (std::string src_id, std::string dst_id, int mode,
		       int data, int encr_type, int encr_subtype, int can,
		       std::string meta, std::string key,
		       std::string priv_key, bool debug, bool signed_str, std::string seed)
    {
      return gnuradio::get_initial_sptr
	(new
	 m17_coder_impl (src_id, dst_id, mode, data, encr_type, encr_subtype,
			 can, meta, key, priv_key, debug, signed_str, seed));
    }

    /*
     * The private constructor
     */
    m17_coder_impl::m17_coder_impl (std::string src_id, std::string dst_id,
				    int mode, int data, int encr_type,
				    int encr_subtype, int can,
				    std::string meta, std::string key,
				    std::string priv_key, bool debug,
				    bool signed_str, std::string seed):gr::block 
                                    ("m17_coder", gr::io_signature:: make (1, 1, sizeof (char)),
	 					  gr::io_signature:: make (1, 1, sizeof (float))),
      _mode (mode), _data (data), _encr_subtype (encr_subtype), _can (can), _meta (meta), _debug (debug),
      _signed_str (signed_str)
    {
      set_encr_type(encr_type); // overwritten by set_seed()
      set_type (mode, data, _encr_type, encr_subtype, can);
      set_meta (meta);		// depends on   ^^^ encr_subtype
      set_seed (seed);		// depends on   ^^^ encr_subtype
      set_src_id (src_id);
      set_dst_id (dst_id);
      set_signed (signed_str);
      set_debug (debug);
      set_output_multiple (192);
      uint16_t ccrc = LSF_CRC (&_lsf);
        _lsf.crc[0] = ccrc >> 8;
        _lsf.crc[1] = ccrc & 0xFF;
        _got_lsf = 0;		//have we filled the LSF struct yet?
        _fn = 0;		//16-bit Frame Number (for the stream mode)
        _finished = false;
#ifdef AES
        srand (time (NULL));	//random number generator (for IV rand() seed value)
        memset (_key, 0, 32 * sizeof (uint8_t));
        memset (_iv, 0, 16 * sizeof (uint8_t));
#endif
      if (_encr_type == ENCR_AES)
	{
	  set_key (key);	// read key
	  *((int32_t *) & _iv[0]) = (uint32_t) time (NULL) - (uint32_t) epoch;	//timestamp
	  for (uint8_t i = 4; i < 4 + 10; i++)
	    _iv[i] = 0;		//10 random bytes TODO: replace with a rand() or pass through an additional arg
	}
     message_port_register_in(pmt::mp("end_of_transmission"));
     set_msg_handler(pmt::mp("end_of_transmission"), [this](const pmt::pmt_t& msg) { end_of_transmission(msg); });
    }

    void m17_coder_impl::end_of_transmission(const pmt::pmt_t& msg)
    {_finished=true;
     std::cout << "***** End of Transmission ********\n";
     pmt::print(msg);
    }

    void m17_coder_impl::set_encr_type (int encr_type)
    { 
      switch (encr_type)
      {case 0:_encr_type=ENCR_NONE;break;
       case 1:_encr_type=ENCR_SCRAM;break;
       case 2:_encr_type=ENCR_AES;break;
       case 3:_encr_type=ENCR_RES;break;
       default:_encr_type=ENCR_NONE;
      }
      printf ("new encr type: %x -> ", _encr_type);
    }

    void m17_coder_impl::set_signed (bool signed_str)
    {
      _signed_str = signed_str;
      if (_signed_str == true)
	printf ("Signed\n");
      else
	printf ("Unsigned\n");
    }

    void m17_coder_impl::set_debug (bool debug)
    {
      _debug = debug;
      if (_debug == true)
	printf ("Debug true\n");
      else
	printf ("Debug false\n");
    }

    void m17_coder_impl::set_src_id (std::string src_id)
    {
      int length;
      for (int i = 0; i < 10; i++)
	{
	  _src_id[i] = 0;
	}
      if (src_id.length () > 9)
	length = 9;
      else
	length = src_id.length ();
      for (int i = 0; i < length; i++)
	{
	  _src_id[i] = toupper (src_id.c_str ()[i]);
	}
      encode_callsign_bytes (_lsf.src, _src_id);	// 6 byte ID <- 9 char callsign

      uint16_t ccrc = LSF_CRC (&_lsf);
      _lsf.crc[0] = ccrc >> 8;
      _lsf.crc[1] = ccrc & 0xFF;
    }

    void m17_coder_impl::set_dst_id (std::string dst_id)
    {
      int length;
      for (int i = 0; i < 10; i++)
	{
	  _dst_id[i] = 0;
	}
      if (dst_id.length () > 9)
	length = 9;
      else
	length = dst_id.length ();
      for (int i = 0; i < length; i++)
	{
	  _dst_id[i] = toupper (dst_id.c_str ()[i]);
	}
      encode_callsign_bytes (_lsf.dst, _dst_id);	// 6 byte ID <- 9 char callsign
      uint16_t ccrc = LSF_CRC (&_lsf);
      _lsf.crc[0] = ccrc >> 8;
      _lsf.crc[1] = ccrc & 0xFF;
    }

    void m17_coder_impl::set_priv_key (std::string arg)	// *UTF-8* encoded byte array
    {
      int length;
      printf ("new private key: ");
      length = arg.size ();
      _priv_key_loaded = true;
      int i = 0, j = 0;
      while ((j < 32) && (i < length))
	{
	  if ((unsigned int) arg.data ()[i] < 0xc2)	// https://www.utf8-chartable.de/
	    {
	      _priv_key[j] = arg.data ()[i];
	      i++;
	      j++;
	    }
	  else
	    {
	      _priv_key[j] =
		(arg.data ()[i] - 0xc2) * 0x40 + arg.data ()[i + 1];
	      i += 2;
	      j++;
	    }
	}
      length = j;		// index from 0 to length-1
      printf ("%d bytes: ", length);
      for (i = 0; i < length; i++)
	printf ("%02X ", _priv_key[i]);
      printf ("\n");
      fflush (stdout);
    }

    void m17_coder_impl::set_key (std::string arg)	// *UTF-8* encoded byte array
    {
      int length;
      printf ("new key: ");
      length = arg.size ();
      int i = 0, j = 0;
      while ((j < 32) && (i < length))
	{
	  if ((unsigned int) arg.data ()[i] < 0xc2)	// https://www.utf8-chartable.de/
	    {
	      _key[j] = arg.data ()[i];
	      i++;
	      j++;
	    }
	  else
	    {
	      _key[j] = (arg.data ()[i] - 0xc2) * 0x40 + arg.data ()[i + 1];
	      i += 2;
	      j++;
	    }
	}
      length = j;		// index from 0 to length-1
      printf ("%d bytes: ", length);
      for (i = 0; i < length; i++)
	printf ("%02X ", _key[i]);
      printf ("\n");
      fflush (stdout);
    }

    void m17_coder_impl::set_seed (std::string arg)	// *UTF-8* encoded byte array
    {
      int length;
      printf ("new seed: ");
      length = arg.size ();
      int i = 0, j = 0;
      while ((j < 3) && (i < length))
	{
	  if ((unsigned int) arg.data ()[i] < 0xc2)	// https://www.utf8-chartable.de/
	    {
	      _seed[j] = arg.data ()[i];
	      i++;
	      j++;
	    }
	  else
	    {
	      _seed[j] = (arg.data ()[i] - 0xc2) * 0x40 + arg.data ()[i + 1];
	      i += 2;
	      j++;
	    }
	}
      length = j;		// index from 0 to length-1
      printf ("%d bytes: ", length);
      for (i = 0; i < length; i++)
	printf ("%02X ", _seed[i]);
      printf ("\n");
      fflush (stdout);
      if(length<=2)
        {   
         _scrambler_seed = _scrambler_seed >> 16;
         fprintf(stderr, "Scrambler key: 0x%02X (8-bit)\n", _scrambler_seed);
        }
      else if(length<=4)
        {   
         _scrambler_seed = _scrambler_seed >> 8;
         fprintf(stderr, "Scrambler key: 0x%04X (16-bit)\n", _scrambler_seed);
        }
      else
         fprintf(stderr, "Scrambler key: 0x%06X (24-bit)\n", _scrambler_seed);

      _encr_type=ENCR_SCRAM; //Scrambler key was passed
    }

    void m17_coder_impl::set_meta (std::string meta)	// either an ASCII string if encr_subtype==0 or *UTF-8* encoded byte array
    {
      int length;
      printf ("new meta: ");
      if (_encr_subtype == 0)	// meta is \0-terminated string
	{
	  if (meta.length () < 14)
	    length = meta.length ();
	  else
	    {
	      length = 14;
	      meta[length] = 0;
	    }
	  printf ("%s\n", meta.c_str ());
	  for (int i = 0; i < length; i++)
	    {
	      _lsf.meta[i] = meta[i];
	    }
	}
      else
	{
	  length = meta.size ();
	  int i = 0, j = 0;
	  while ((j < 14) && (i < length))
	    {
	      if ((unsigned int) meta.data ()[i] < 0xc2)	// https://www.utf8-chartable.de/
		{
		  _lsf.meta[j] = meta.data ()[i];
		  i++;
		  j++;
		}
	      else
		{
		  _lsf.meta[j] =
		    (meta.data ()[i] - 0xc2) * 0x40 + meta.data ()[i + 1];
		  i += 2;
		  j++;
		}
	    }
	  length = j;		// index from 0 to length-1
	  printf ("%d bytes: ", length);
	  for (i = 0; i < length; i++)
	    printf ("%02X ", _lsf.meta[i]);
	  printf ("\n");
	}
      fflush (stdout);
      uint16_t ccrc = LSF_CRC (&_lsf);
      _lsf.crc[0] = ccrc >> 8;
      _lsf.crc[1] = ccrc & 0xFF;
    }

    void m17_coder_impl::set_mode (int mode)
    {
      _mode = mode;
      printf ("new mode: %x -> ", _mode);
      set_type (_mode, _data, _encr_type, _encr_subtype, _can);
    }

    void m17_coder_impl::set_data (int data)
    {
      _data = data;
      printf ("new data type: %x -> ", _data);
      set_type (_mode, _data, _encr_type, _encr_subtype, _can);
    }

    void m17_coder_impl::set_encr_subtype (int encr_subtype)
    {
      _encr_subtype = encr_subtype;
      printf ("new encr subtype: %x -> ", _encr_subtype);
      set_type (_mode, _data, _encr_type, _encr_subtype, _can);
    }

    void m17_coder_impl::set_can (int can)
    {
      _can = can;
      printf ("new CAN: %x -> ", _can);
      set_type (_mode, _data, _encr_type, _encr_subtype, _can);
    }

    void m17_coder_impl::set_type (int mode, int data, encr_t encr_type,
				   int encr_subtype, int can)
    {
      short tmptype;
      tmptype =
	mode | (data << 1) | (encr_type << 3) | (encr_subtype << 5) | (can <<
								       7);
      _lsf.type[0] = tmptype >> 8;	// MSB
      _lsf.type[1] = tmptype & 0xff;	// LSB
      uint16_t ccrc = LSF_CRC (&_lsf);
      _lsf.crc[0] = ccrc >> 8;
      _lsf.crc[1] = ccrc & 0xFF;
      printf ("Transmission type: 0x%02X%02X\n", _lsf.type[0], _lsf.type[1]);
      fflush (stdout);
    }

    /*
     * Our virtual destructor.
     */
    m17_coder_impl::~m17_coder_impl ()
    {
    }

    void
      m17_coder_impl::forecast (int noutput_items,
				gr_vector_int & ninput_items_required)
    {
      ninput_items_required[0] = noutput_items / 12;	// 16 inputs -> 192 outputs
    }

    //scrambler pn sequence generation
    void m17_coder_impl::scrambler_sequence_generator ()
    {
      int i = 0;
      uint32_t lfsr, bit;
      lfsr = _scrambler_seed;

      //only set if not initially set (first run), it is possible (and observed) that the scrambler_subtype can 
      //change on subsequent passes if the current SEED for the LFSR falls below one of these thresholds
      if (_scrambler_subtype == -1)
	{
	  if (lfsr > 0 && lfsr <= 0xFF)
	    _scrambler_subtype = 0;	// 8-bit key
	  else if (lfsr > 0xFF && lfsr <= 0xFFFF)
	    _scrambler_subtype = 1;	//16-bit key
	  else if (lfsr > 0xFFFF && lfsr <= 0xFFFFFF)
	    _scrambler_subtype = 2;	//24-bit key
	  else
	    _scrambler_subtype = 0;	// 8-bit key (default)
	}

      //TODO: Set Frame Type based on scrambler_subtype value
      if (_debug == true)
	{
	  fprintf (stderr,
		   "\nScrambler Key: 0x%06X; Seed: 0x%06X; Subtype: %02d;",
		   _scrambler_seed, lfsr, _scrambler_subtype);
	  fprintf (stderr, "\n pN: ");
	}

      //run pN sequence with taps specified
      for (i = 0; i < 128; i++)
	{
	  //get feedback bit with specified taps, depending on the scrambler_subtype
	  if (_scrambler_subtype == 0)
	    bit = (lfsr >> 7) ^ (lfsr >> 5) ^ (lfsr >> 4) ^ (lfsr >> 3);
	  else if (_scrambler_subtype == 1)
	    bit = (lfsr >> 15) ^ (lfsr >> 14) ^ (lfsr >> 12) ^ (lfsr >> 3);
	  else if (_scrambler_subtype == 2)
	    bit = (lfsr >> 23) ^ (lfsr >> 22) ^ (lfsr >> 21) ^ (lfsr >> 16);
	  else
	    bit = 0;		//should never get here, but just in case

	  bit &= 1;		//truncate bit to 1 bit (required since I didn't do it above)
	  lfsr = (lfsr << 1) | bit;	//shift LFSR left once and OR bit onto LFSR's LSB
	  lfsr &= 0xFFFFFF;	//truncate lfsr to 24-bit (really doesn't matter)
	  _scrambler_pn[i] = bit;

	}
      //pack bit array into byte array for easy data XOR
      pack_bit_array_into_byte_array (_scrambler_pn, _scr_bytes, 16);

      //save scrambler seed for next round
      _scrambler_seed = lfsr;

      //truncate seed so subtype will continue to set properly on subsequent passes
      if (_scrambler_subtype == 0)
	_scrambler_seed &= 0xFF;
      else if (_scrambler_subtype == 1)
	_scrambler_seed &= 0xFFFF;
      else if (_scrambler_subtype == 2)
	_scrambler_seed &= 0xFFFFFF;

      if (_debug == true)
	{
	  //debug packed bytes
	  for (i = 0; i < 16; i++)
	    fprintf (stderr, " %02X", _scr_bytes[i]);
	  fprintf (stderr, "\n");
	}

    }

    //convert a user string (as hex octets) into a uint8_t array for key
    void m17_coder_impl::parse_raw_key_string (uint8_t * dest,
					       const char *inp)
    {
      uint16_t len = strlen (inp);

      if (len == 0)
	return;			//return silently and pretend nothing happened

      memset (dest, 0, len / 2);	//one character represents half of a byte

      if (!(len % 2))		//length even?
	{
	  for (uint8_t i = 0; i < len; i += 2)
	    {
	      if (inp[i] >= 'a')
		dest[i / 2] |= (inp[i] - 'a' + 10) * 0x10;
	      else if (inp[i] >= 'A')
		dest[i / 2] |= (inp[i] - 'A' + 10) * 0x10;
	      else if (inp[i] >= '0')
		dest[i / 2] |= (inp[i] - '0') * 0x10;

	      if (inp[i + 1] >= 'a')
		dest[i / 2] |= inp[i + 1] - 'a' + 10;
	      else if (inp[i + 1] >= 'A')
		dest[i / 2] |= inp[i + 1] - 'A' + 10;
	      else if (inp[i + 1] >= '0')
		dest[i / 2] |= inp[i + 1] - '0';
	    }
	}
      else
	{
	  if (inp[0] >= 'a')
	    dest[0] |= inp[0] - 'a' + 10;
	  else if (inp[0] >= 'A')
	    dest[0] |= inp[0] - 'A' + 10;
	  else if (inp[0] >= '0')
	    dest[0] |= inp[0] - '0';

	  for (uint8_t i = 1; i < len - 1; i += 2)
	    {
	      if (inp[i] >= 'a')
		dest[i / 2 + 1] |= (inp[i] - 'a' + 10) * 0x10;
	      else if (inp[i] >= 'A')
		dest[i / 2 + 1] |= (inp[i] - 'A' + 10) * 0x10;
	      else if (inp[i] >= '0')
		dest[i / 2 + 1] |= (inp[i] - '0') * 0x10;

	      if (inp[i + 1] >= 'a')
		dest[i / 2 + 1] |= inp[i + 1] - 'a' + 10;
	      else if (inp[i + 1] >= 'A')
		dest[i / 2 + 1] |= inp[i + 1] - 'A' + 10;
	      else if (inp[i + 1] >= '0')
		dest[i / 2 + 1] |= inp[i + 1] - '0';
	    }
	}
    }

    int
      m17_coder_impl::general_work (int noutput_items,
				    gr_vector_int & ninput_items,
				    gr_vector_const_void_star & input_items,
				    gr_vector_void_star & output_items)
    {
      const char *in = (const char *) input_items[0];
      float *out = (float *) output_items[0];
      int countin = 0;
      uint32_t countout = 0;

      uint8_t enc_bits[SYM_PER_PLD * 2];	//type-2 bits, unpacked
      uint8_t rf_bits[SYM_PER_PLD * 2];	//type-4 bits, unpacked
      uint8_t lich[6];		//48 bits packed raw, unencoded LICH
      uint8_t lich_encoded[12];	//96 bits packed, encoded LICH

      uint8_t data[16], next_data[16];	//raw payload, packed bits

      while ((countout < (uint32_t) noutput_items) && (countin + 16 <= noutput_items))
	{
	      if (!_got_lsf)	//stream frames
		{
		  //send LSF syncword
		  send_syncword (out, &countout, SYNC_LSF);

		  //encode LSF data
		  conv_encode_LSF (enc_bits, &_lsf);

		  //reorder bits
		  reorder_bits (rf_bits, enc_bits);

		  //randomize
		  randomize_bits (rf_bits);

		  //send LSF data
		  send_data (out, &countout, rf_bits);

		  //check the SIGNED STREAM flag
		  _signed_str = (_lsf.type[0] >> 3) & 1;

		  //set the flag
		  _got_lsf = 1;
		}

	      if (_debug == true)
		{
		  //destination set to "ALL"
		  memset (_next_lsf.dst, 0xFF, 6 * sizeof (uint8_t));

		  //source  set to "N0CALL"
		  _next_lsf.src[0] = 0x00;
		  _next_lsf.src[1] = 0x00;
		  _next_lsf.src[2] = 0x4B;
		  _next_lsf.src[3] = 0x13;
		  _next_lsf.src[4] = 0xD1;
		  _next_lsf.src[5] = 0x06;

		  if (_encr_type == ENCR_AES)	//AES ENC, 3200 voice
		    {
		      _next_lsf.type[0] = 0x03;
		      _next_lsf.type[1] = 0x95;
		    }
		  else if (_encr_type == ENCR_SCRAM)	//Scrambler ENC, 3200 Voice
		    {
		      _next_lsf.type[0] = 0x00;
		      _next_lsf.type[1] = 0x00;
		      if (_scrambler_subtype == 0)
			_next_lsf.type[1] = 0x0D;
		      else if (_scrambler_subtype == 1)
			_next_lsf.type[1] = 0x2D;
		      else if (_scrambler_subtype == 2)
			_next_lsf.type[1] = 0x4D;
		    }
		  else		//no enc or subtype field, normal 3200 voice
		    {
		      _next_lsf.type[0] = 0x00;
		      _next_lsf.type[1] = 0x05;
		    }

		  //a signature key is loaded, OR this bit
		  if (_priv_key_loaded == true)
		    _next_lsf.type[0] |= 0x8;

		  _finished = false;

		  memset (next_data, 0, sizeof (next_data));
		  memcpy (data, next_data, sizeof (data));
//		  if (_fn == 60)
//		    _finished = true;

		  //debug sig with random payloads (don't play the audio)
		  for (uint8_t i = 0; i < 16; i++)
		    data[i] = 0x69;	//rand() & 0xFF;

		}
/*
        //TODO: pass some of these through arguments?
        //read data
        dummy=fread(&(lsf.dst), 6, 1, stdin);
        dummy=fread(&(lsf.src), 6, 1, stdin);
        dummy=fread(&(lsf.type), 2, 1, stdin);
        dummy=fread(&(lsf.meta), 14, 1, stdin);
        dummy=fread(data, 16, 1, stdin);
*/
	  if (countin + 16 <= noutput_items)
	    {
	      for (int i = 0; i < 16; i++)
		{
		  data[i] = in[countin];
		  countin++;
		}
            }

	      //AES encryption enabled - use 112 bits of IV
	      if (_encr_type == ENCR_AES)
		{
		  memcpy (&(_lsf.meta), _iv, 14);
		  _iv[14] = (_fn >> 8) & 0x7F;
		  _iv[15] = (_fn >> 0) & 0xFF;

		  //re-calculate LSF CRC with IV insertion
		  uint16_t ccrc = LSF_CRC (&_lsf);
		  _lsf.crc[0] = ccrc >> 8;
		  _lsf.crc[1] = ccrc & 0xFF;
		}

//	      while (_finished == false)
		{
		  if (!_got_lsf)
		    {		//debug
		      //fprintf(stderr, "LSF\n");

		      //send LSF syncword
		      send_syncword (out, &countout, SYNC_LSF);

		      //encode LSF data
		      conv_encode_LSF (enc_bits, &_lsf);

		      //reorder bits
		      reorder_bits (rf_bits, enc_bits);

		      //randomize
		      randomize_bits (rf_bits);

		      //send LSF data
		      send_data (out, &countout, rf_bits);

		      //check the SIGNED STREAM flag
		      _signed_str = (_lsf.type[0] >> 3) & 1;

		      //set the flag
		      _got_lsf = 1;
		    }

		  if (_debug == true)
		    {
		      //destination set to "ALL"
		      memset (_next_lsf.dst, 0xFF, 6 * sizeof (uint8_t));

		      //source  set to "N0CALL"
		      _next_lsf.src[0] = 0x00;
		      _next_lsf.src[1] = 0x00;
		      _next_lsf.src[2] = 0x4B;
		      _next_lsf.src[3] = 0x13;
		      _next_lsf.src[4] = 0xD1;
		      _next_lsf.src[5] = 0x06;

		      if (_encr_type == ENCR_AES)	//AES ENC, 3200 voice
			{
			  _next_lsf.type[0] = 0x03;
			  _next_lsf.type[1] = 0x95;
			}
		      else if (_encr_type == ENCR_SCRAM)	//Scrambler ENC, 3200 Voice
			{
			  _next_lsf.type[0] = 0x00;
			  _next_lsf.type[1] = 0x00;
			  if (_scrambler_subtype == 0)
			    _next_lsf.type[1] = 0x0D;
			  else if (_scrambler_subtype == 1)
			    _next_lsf.type[1] = 0x2D;
			  else if (_scrambler_subtype == 2)
			    _next_lsf.type[1] = 0x4D;
			}
		      else	//no enc or subtype field, normal 3200 voice
			{
			  _next_lsf.type[0] = 0x00;
			  _next_lsf.type[1] = 0x05;
			}

		      //a signature key is loaded, OR this bit
		      if (_priv_key_loaded == true)
			_next_lsf.type[0] |= 0x8;

		      _finished = false;

		      memset (next_data, 0, sizeof (next_data));
		      memcpy (data, next_data, sizeof (data));
		      if (_fn == 60)
			_finished = 1;

		      //debug sig with random payloads (don't play the audio)
		      for (uint8_t i = 0; i < 16; i++)
			data[i] = 0x69;	//rand() & 0xFF;

		    }		// end of debug==true
		  else
		    {
		      /*
		         //check if theres any more data
		         if(fread(&(next_lsf.dst), 6, 1, stdin)<1) finished=1;
		         if(fread(&(next_lsf.src), 6, 1, stdin)<1) finished=1;
		         if(fread(&(next_lsf.type), 2, 1, stdin)<1) finished=1;
		         if(fread(&(next_lsf.meta), 14, 1, stdin)<1) finished=1;
		         if(fread(next_data, 16, 1, stdin)<1) _finished=true;
		       */
	  if (countin + 16 <= noutput_items)
	    {
	      for (int i = 0; i < 16; i++)
		{
		  next_data[i] = in[countin];
		  countin++;
		}
             }
		    }

		  //AES
		  if (_encr_type == ENCR_AES)
		    {
		      memcpy (&(_next_lsf.meta), _iv, 14);
		      _iv[14] = (_fn >> 8) & 0x7F;
		      _iv[15] = (_fn >> 0) & 0xFF;
		      aes_ctr_bytewise_payload_crypt (_iv, _key, data, AES128);	//hardcoded for now
		    }

		  //Scrambler
		  else if (_encr_type == ENCR_SCRAM)
		    {
		      scrambler_sequence_generator ();
		      for (uint8_t i = 0; i < 16; i++)
			{
			  data[i] ^= _scr_bytes[i];
			}
		    }

		  if (_finished == false)
		    {
		      send_syncword (out, &countout, SYNC_STR);
		      extract_LICH (lich, _lich_cnt, &_lsf);
		      encode_LICH (lich_encoded, lich);
		      unpack_LICH (enc_bits, lich_encoded);
		      conv_encode_stream_frame (&enc_bits[96], data, _fn);
		      reorder_bits (rf_bits, enc_bits);
		      randomize_bits (rf_bits);
		      send_data (out, &countout, rf_bits);
		      _fn = (_fn + 1) % 0x8000;	//increment FN
		      _lich_cnt = (_lich_cnt + 1) % 6;	//continue with next LICH_CNT

		      //update the stream digest if required
		      if (_signed_str)
			{
			  for (uint8_t i = 0; i < sizeof (_digest); i++)
			    _digest[i] ^= data[i];
			  uint8_t tmp = _digest[0];
			  for (uint8_t i = 0; i < sizeof (_digest) - 1; i++)
			    _digest[i] = _digest[i + 1];
			  _digest[sizeof (_digest) - 1] = tmp;
			}

		      //update LSF every 6 frames (superframe boundary)
		      if (_fn > 0 && _lich_cnt == 0)
			{
			  _lsf = _next_lsf;

			  //calculate LSF CRC
			  uint16_t ccrc = LSF_CRC (&_lsf);
			  _lsf.crc[0] = ccrc >> 8;
			  _lsf.crc[1] = ccrc & 0xFF;
			}

		      memcpy (data, next_data, 16);
		    }
		  else		//send last frame(s)
		    { printf("Sending last frame\n");
		      send_syncword (out, &countout, SYNC_STR);
		      extract_LICH (lich, _lich_cnt, &_lsf);
		      encode_LICH (lich_encoded, lich);
		      unpack_LICH (enc_bits, lich_encoded);
		      if (!_signed_str)
			conv_encode_stream_frame (&enc_bits[96], data,
						  (_fn | 0x8000));
		      else
			conv_encode_stream_frame (&enc_bits[96], data, _fn);
		      reorder_bits (rf_bits, enc_bits);
		      randomize_bits (rf_bits);
		      send_data (out, &countout, rf_bits);
		      _lich_cnt = (_lich_cnt + 1) % 6;	//continue with next LICH_CNT

		      //if we are done, and the stream is signed, so we need to transmit the signature (4 frames)
		      if (_signed_str)
			{
			  //update digest
			  for (uint8_t i = 0; i < sizeof (_digest); i++)
			    _digest[i] ^= data[i];
			  uint8_t tmp = _digest[0];
			  for (uint8_t i = 0; i < sizeof (_digest) - 1; i++)
			    _digest[i] = _digest[i + 1];
			  _digest[sizeof (_digest) - 1] = tmp;

			  //sign the digest
			  uECC_sign (_priv_key, _digest, sizeof (_digest),
				     _sig, _curve);

			  //4 frames with 512-bit signature
			  _fn = 0x7FFC;	//signature has to start at 0x7FFC to end at 0x7FFF (0xFFFF with EoT marker set)
			  for (uint8_t i = 0; i < 4; i++)
			    {
			      send_syncword (out, &countout, SYNC_STR);
			      extract_LICH (lich, _lich_cnt, &_lsf);
			      encode_LICH (lich_encoded, lich);
			      unpack_LICH (enc_bits, lich_encoded);
			      conv_encode_stream_frame (&enc_bits[96],
							&_sig[i * 16], _fn);
			      reorder_bits (rf_bits, enc_bits);
			      randomize_bits (rf_bits);
			      send_data (out, &countout, rf_bits);
			      _fn =
				(_fn < 0x7FFE) ? _fn + 1 : (0x7FFF | 0x8000);
			      _lich_cnt = (_lich_cnt + 1) % 6;	//continue with next LICH_CNT
			    }
			  if (_debug == true)
			    {
			      fprintf (stderr, "Signature: ");
			      for (uint8_t i = 0; i < sizeof (_sig); i++)
				{
				  if (i == 16 || i == 32 || i == 48)
				    fprintf (stderr, "\n           ");
				  fprintf (stderr, "%02X", _sig[i]);
				}

			      fprintf (stderr, "\n");
			    }
			}
		      //send EOT frame
		      send_eot (out, &countout);
		      //fprintf(stderr, "Stream has ended. Exiting.\n");
		    }		// finished == true
		}		// finished == false
	}			// loop on input data
      // Tell runtime system how many input items we consumed on
      // each input stream.
      consume_each (countin);
      //    printf(" noutput_items=%d countin=%d countout=%d\n",noutput_items,countin,countout);
      // Tell runtime system how many output items we produced.
      if (_finished==false)
      return countout;
      else {printf("Killing flowgraph\n"); return -1;} // https://lists.gnu.org/archive/html/discuss-gnuradio/2016-12/msg00206.html
      // returning -1 (which is the magical value for "there's nothing coming anymore, you can shut down") would normally end a flow graph
    }

  }				/* namespace m17 */
}				/* namespace gr */
