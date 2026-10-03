#
# Copyright 2026 M17 Project.
#
# SPDX-License-Identifier: GPL-3.0-or-later
#
'''
Builders for the 14-byte LSF META field used by the M17 Encoder block.

The position, ECD and raw builders return a Python str holding raw bytes (one
character per byte, latin-1), which is the form the C++ block expects for
byte-array META contents; meta_text() returns plain text, which the block
splits into Text Data blocks itself. Layouts follow the M17 specification 2.0.x. The encoders mirror libm17's set_LSF_meta_position() and
set_LSF_meta_ecd() byte for byte.
'''

import math

import numpy as np

META_LEN = 14

# Meta content selected by the encoder's Subtype field (encryption: None)
SUBTYPE_TEXT = 0
SUBTYPE_POSITION = 1
SUBTYPE_ECD = 2
SUBTYPE_RAW = 3

# GNSS data source and station type (spec, Appendix J)
SOURCE_M17_CLIENT = 0
SOURCE_OPENRTX = 1
SOURCE_OTHER = 15
STATION_FIXED = 0
STATION_MOBILE = 1
STATION_HANDHELD = 2
STATION_OTHER = 15

# GNSS validity flags
VALID_LAT_LON = 1 << 3
VALID_ALTITUDE = 1 << 2
VALID_VELOCITY = 1 << 1
VALID_RADIUS = 1 << 0

_CHAR_MAP = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-/."


def _as_meta(data):
    '''Pad/truncate to 14 bytes and return as a byte-per-character str.'''
    return bytes(data).ljust(META_LEN, b'\0')[:META_LEN].decode('latin-1')


def _clampf(x, lo, hi):
    x = np.float32(x)
    if not x >= lo:  # also catches NaN, like libm17
        return np.float32(lo)
    if x > hi:
        return np.float32(hi)
    return x


def _roundf(x):
    # C roundf(): halves away from zero (Python's round() rounds to even)
    return math.floor(float(x) + 0.5) if x >= 0 else math.ceil(float(x) - 0.5)


def encode_callsign(callsign):
    '''Encode a callsign into 6 bytes (big-endian base-40), like libm17.'''
    if len(callsign) > 9:
        raise ValueError('callsign "%s" is longer than 9 characters' % callsign)
    if callsign == '@ALL':
        value = 0xFFFFFFFFFFFF
    else:
        start = 1 if callsign.startswith('#') else 0
        value = 0
        for c in reversed(callsign[start:].upper()):
            idx = _CHAR_MAP.find(c)
            value = value * 40 + (idx if idx > 0 else 0)  # invalid -> space
        if start:
            value += 40 ** 9
    return value.to_bytes(6, 'big')


def meta_position(lat, lon, altitude=0.0, speed=0.0, bearing=0,
                  station_type=STATION_MOBILE, data_source=SOURCE_M17_CLIENT,
                  validity=VALID_LAT_LON | VALID_ALTITUDE | VALID_VELOCITY,
                  radius=1.0):
    '''GNSS position META (lat/lon in degrees, altitude in m, speed in km/h,
    bearing in degrees). Same byte layout as libm17 set_LSF_meta_position().'''
    tmp = [0] * META_LEN
    bearing = int(bearing) & 0xFFFF

    tmp[0] = ((int(data_source) << 4) | int(station_type)) & 0xFF

    tmp[1] |= (int(validity) << 4) & 0xFF
    radius = np.float32(radius)
    log_r = 7
    for i, r in enumerate((1.0, 2.0, 4.0, 8.0, 16.0, 32.0, 64.0, 128.0)):
        if radius <= r:
            log_r = i
            break
    tmp[1] |= log_r << 1
    tmp[1] |= (bearing >> 8) & 1
    tmp[2] = bearing & 0xFF

    # float32 arithmetic and truncation toward zero, as in the C code
    lat_tmp = int(_clampf(lat, -90.0, 90.0) / np.float32(90.0) * np.float32(8388607.0))
    lon_tmp = int(_clampf(lon, -180.0, 180.0) / np.float32(180.0) * np.float32(8388607.0))
    tmp[3:6] = [(lat_tmp >> 16) & 0xFF, (lat_tmp >> 8) & 0xFF, lat_tmp & 0xFF]
    tmp[6:9] = [(lon_tmp >> 16) & 0xFF, (lon_tmp >> 8) & 0xFF, lon_tmp & 0xFF]

    alt = _roundf((np.float32(500.0) + _clampf(altitude, -500.0, 32267.5)) * np.float32(2.0)) & 0xFFFF
    tmp[9] = alt >> 8
    tmp[10] = alt & 0xFF

    spd = _roundf(_clampf(speed, 0.0, 2047.5) * np.float32(2.0)) & 0xFFFF
    tmp[11] = (spd >> 4) & 0xFF
    tmp[12] = (spd << 4) & 0xF0  # low nibble reserved
    tmp[13] = 0

    return _as_meta(tmp)


def meta_ecd(cf1, cf2=''):
    '''Extended Callsign Data META: callsign field 1 and optional field 2.'''
    data = encode_callsign(cf1)
    data += encode_callsign(cf2) if cf2 else bytes(6)
    return _as_meta(data)


TEXT_MAX_LEN = 52   # 4 Text Data blocks of 13 bytes


def meta_text(text):
    '''Text Data META (spec 2.0.x): plain UTF-8 text of up to 52 bytes. The encoder block
    splits it into up to four 13-byte blocks, adds the Control Bytes and sends one block
    per superframe. Returned as a normal str: the block copies its UTF-8 bytes.'''
    n = len(text.encode('utf-8'))
    if n > TEXT_MAX_LEN:
        raise ValueError('META text is %d bytes long, the maximum is %d' % (n, TEXT_MAX_LEN))
    return text


def meta_hex(hex_str):
    '''Raw META from hex text (spaces allowed), zero-padded to 14 bytes.'''
    return _as_meta(bytes.fromhex(hex_str))


def build_meta(encr_type, subtype, text='', lat=0.0, lon=0.0, altitude=0.0,
               speed=0.0, bearing=0, station_type=STATION_MOBILE,
               data_source=SOURCE_M17_CLIENT, cf1='', cf2='', raw_hex=''):
    '''Build the META argument for m17_coder from the GRC dialog fields.

    META contents are only defined without encryption; with AES the block
    fills META with the IV itself, so '' (all zeros) is returned otherwise.'''
    if int(encr_type) != 0:
        return ''
    subtype = int(subtype)
    if subtype == SUBTYPE_TEXT:
        return meta_text(text)
    if subtype == SUBTYPE_POSITION:
        return meta_position(lat, lon, altitude, speed, bearing,
                             station_type, data_source)
    if subtype == SUBTYPE_ECD:
        return meta_ecd(cf1, cf2) if cf1 else ''
    return meta_hex(raw_hex)
