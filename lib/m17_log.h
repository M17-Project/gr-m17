/* -*- c++ -*- */
/*
 * Copyright 2026 M17 Project.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

// Console output shared by the M17 blocks: one line per event, always in the form
//   HH:MM:SS.mmm [TAG] message
// where TAG is M17_ENC / M17_DEC, or the block alias if one is set in GRC.
// Numbers are always formatted with a decimal point, regardless of the system locale.

#ifndef INCLUDED_M17_LOG_H
#define INCLUDED_M17_LOG_H

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <locale.h>
#include <string>
#include <time.h>

#include "m17.h"

namespace gr
{
	namespace m17
	{
		__attribute__((format(printf, 2, 3))) inline void m17_log(const std::string &tag, const char *fmt, ...)
		{
			static locale_t c_locale = newlocale(LC_NUMERIC_MASK, "C", (locale_t)0);
			locale_t prev = uselocale(c_locale); // decimal point, not the system's decimal comma

			char msg[2048];
			va_list ap;
			va_start(ap, fmt);
			vsnprintf(msg, sizeof(msg), fmt, ap);
			va_end(ap);

			struct timespec ts;
			clock_gettime(CLOCK_REALTIME, &ts);
			struct tm t;
			localtime_r(&ts.tv_sec, &t);
			fprintf(stderr, "%02d:%02d:%02d.%03ld [%s] %s\n", t.tm_hour, t.tm_min, t.tm_sec,
					ts.tv_nsec / 1000000L, tag.c_str(), msg);

			uselocale(prev);
		}

		inline std::string m17_hex(const uint8_t *data, size_t len, size_t group = 0)
		{
			std::string s;
			char b[3];
			for (size_t i = 0; i < len; i++)
			{
				if (group && i && (i % group) == 0)
					s += ' ';
				snprintf(b, sizeof(b), "%02X", data[i]);
				s += b;
			}
			return s;
		}

		// "voice stream, AES-128, CAN 0, signed" / "packet, CAN 0"
		inline std::string m17_type_str(uint16_t type)
		{
			std::string s;
			if (!(type & 1))
				return "packet, CAN " + std::to_string((type >> 7) & 0xF);

			static const char *data[4] = {"reserved", "data", "voice", "voice+data"};
			s = std::string(data[(type >> 1) & 3]) + " stream, ";

			uint8_t encr = (type >> 3) & 3, sub = (type >> 5) & 3;
			if (encr == 0)
				s += "no encryption";
			else if (encr == 1)
				s += (sub < 3) ? "scrambler " + std::to_string(8 * (sub + 1)) + "-bit" : "scrambler (reserved size)";
			else if (encr == 2)
				s += (sub < 3) ? "AES-" + std::to_string(128 + 64 * sub) : "AES (reserved size)";
			else
				s += "reserved encryption";

			s += ", CAN " + std::to_string((type >> 7) & 0xF);
			if ((type >> 11) & 1)
				s += ", signed";
			return s;
		}

		// readable META contents (spec 2.0.x), selected by the TYPE field
		inline std::string m17_meta_str(const uint8_t meta[14], uint16_t type)
		{
			uint8_t encr = (type >> 3) & 3, sub = (type >> 5) & 3;
			char b[160];

			if (!(type & 1))
				return "META " + m17_hex(meta, 14);
			if (encr == 2)
				return "AES nonce " + m17_hex(meta, 14);
			if (encr != 0)
				return "META " + m17_hex(meta, 14);

			bool empty = true;
			for (int i = 0; i < 14; i++)
				empty &= (meta[i] == 0);
			if (empty)
				return "no META";

			if (sub == 0) // Text Data
			{
				if (meta[0] == 0x11) // single text block
				{
					std::string t((const char *)&meta[1], 13);
					size_t end = t.find_last_not_of(std::string(" \0", 2));
					t = (end == std::string::npos) ? "" : t.substr(0, end + 1);
					return "META text \"" + t + "\"";
				}
				snprintf(b, sizeof(b), "META text block (control byte 0x%02X): ", meta[0]);
				return b + m17_hex(&meta[1], 13);
			}
			if (sub == 1) // GNSS position
			{
				static const char *station[16] = {"fixed", "mobile", "handheld", "reserved", "reserved", "reserved",
												  "reserved", "reserved", "reserved", "reserved", "reserved", "reserved",
												  "reserved", "reserved", "reserved", "other"};
				uint8_t valid = meta[1] >> 4;
				int32_t lat = (int32_t)(((uint32_t)meta[3] << 24) | ((uint32_t)meta[4] << 16) | ((uint32_t)meta[5] << 8)) >> 8;
				int32_t lon = (int32_t)(((uint32_t)meta[6] << 24) | ((uint32_t)meta[7] << 16) | ((uint32_t)meta[8] << 8)) >> 8;
				std::string s = "META GNSS (" + std::string(station[meta[0] & 0xF]) + ")";
				if (valid & 8)
				{
					snprintf(b, sizeof(b), " %.5f, %.5f", lat * 90.0 / 8388607.0, lon * 180.0 / 8388607.0);
					s += b;
				}
				if (valid & 4)
				{
					snprintf(b, sizeof(b), ", %.1f m", (((uint16_t)meta[9] << 8) | meta[10]) / 2.0 - 500.0);
					s += b;
				}
				if (valid & 2)
				{
					snprintf(b, sizeof(b), ", %.1f km/h, bearing %d deg",
							 (((uint16_t)meta[11] << 4) | (meta[12] >> 4)) / 2.0, ((meta[1] & 1) << 8) | meta[2]);
					s += b;
				}
				return s;
			}
			if (sub == 2) // Extended Callsign Data
			{
				char cf1[10] = {0}, cf2[10] = {0};
				decode_callsign_bytes(cf1, &meta[0]);
				decode_callsign_bytes(cf2, &meta[6]);
				std::string s = "META ECD " + std::string(cf1);
				bool has_cf2 = false;
				for (int i = 6; i < 12; i++)
					has_cf2 |= (meta[i] != 0);
				if (has_cf2)
					s += ", " + std::string(cf2);
				return s;
			}
			return "META " + m17_hex(meta, 14); // reserved
		}

		// "N0CALL -> @ALL, TYPE 0015 (voice stream, AES-128, CAN 0), AES nonce ..."
		inline std::string m17_lsf_str(const lsf_t &lsf, bool callsigns)
		{
			uint16_t type = ((uint16_t)lsf.type[0] << 8) | lsf.type[1];
			std::string src, dst;
			if (callsigns)
			{
				char d[10] = {0}, s[10] = {0};
				decode_callsign_bytes(d, lsf.dst);
				decode_callsign_bytes(s, lsf.src);
				src = s;
				dst = d;
			}
			else
			{
				src = m17_hex(lsf.src, 6);
				dst = m17_hex(lsf.dst, 6);
			}
			char t[8];
			snprintf(t, sizeof(t), "%04X", type);
			return src + " -> " + dst + ", TYPE " + t + " (" + m17_type_str(type) + "), " + m17_meta_str(lsf.meta, type);
		}

	} // namespace m17
} // namespace gr

#endif /* INCLUDED_M17_LOG_H */
