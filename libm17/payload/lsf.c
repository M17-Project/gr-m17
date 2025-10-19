//--------------------------------------------------------------------
// M17 C library - payload/lsf.c
//
// This file contains:
// - Link Setup Frame related functions
//
// Wojciech Kaczmarski, SP5WWP
// M17 Foundation, 20 April 2025
//--------------------------------------------------------------------
#include <m17.h>

/**
 * @brief Update LSF CRC.
 * 
 * @param lsf Pointer to an LSF struct.
 */
void update_LSF_CRC(lsf_t *lsf)
{
	uint16_t lsf_crc = LSF_CRC(lsf);
	lsf->crc[0] = lsf_crc >> 8;
	lsf->crc[1] = lsf_crc & 0xFF;
}

/**
 * @brief Fill LSF data structure.
 * 
 * @param lsf Pointer to an LSF struct.
 * @param src Pointer to the source callsign.
 * @param dst Pointer to the destination callsign.
 * @param type Value of the LSF TYPE field.
 * @param meta Pointer to a 14-byte array for META field contents.
 *   NULL pointer zeros out META field.
 */
void set_LSF(lsf_t *lsf, char *src, char *dst, uint16_t type, uint8_t meta[14])
{
	encode_callsign_bytes(lsf->src, (uint8_t*)src);
	encode_callsign_bytes(lsf->dst, (uint8_t*)dst);

	lsf->type[0] = type >> 8;
	lsf->type[1] = type & 0xFF;

	if(meta!=NULL)
		memcpy(lsf->meta, meta, 14);
	else
		memset(lsf->meta, 0, 14);

	update_LSF_CRC(lsf);
}

/**
 * @brief Fill the LSF META field and update the CRC.
 * 
 * @param lsf Pointer to an LSF struct.
 * @param meta Pointer to a 14-byte array for META field contents.
 *   NULL pointer zeros out META field.
 */
void set_LSF_meta(lsf_t *lsf, const uint8_t meta[14])
{
	if(meta!=NULL)
		memcpy(lsf->meta, meta, 14);
	else
		memset(lsf->meta, 0, 14);

	update_LSF_CRC(lsf);
}

/**
 * @brief Fill the LSF META field with position data and update the CRC.
 * @brief Hemisphere setting flags are applied automatically.
 * 
 * @param lsf Pointer to an LSF struct.
 * @param data_source Data source.
 * @param station_type Type of the transmitting station.
 * @param lat Latitude in degrees.
 * @param lon Longitude in degrees.
 * @param flags Hemisphere, altitude, speed, and bearing field.
 * @param altitude Altitude in feet (-1500..64035).
 * @param bearing Bearing in degrees.
 * @param speed Speed in miles per hour.
 */
void set_LSF_meta_position(lsf_t *lsf, const uint8_t data_source, const uint8_t station_type,
	const float lat, const float lon, const uint8_t flags, const int32_t altitude, const uint16_t bearing, const uint8_t speed)
{
	uint8_t tmp[14] = {0};
	uint16_t v;

	tmp[0] = data_source;
	tmp[1] = station_type;

	tmp[2] = fabsf(floorf(lat));
	if(signbit(lat)!=0)
		tmp[2] -= 1;
	v = floorf((fabsf(lat)-floorf(fabsf(lat)))*65536.0f);
	tmp[3] = v>>8;
	tmp[4] = v&0xFF;

	tmp[5] = fabsf(floorf(lon));
	if(signbit(lon)!=0)
		tmp[5] -= 1;
	v = floorf((fabsf(lon)-floorf(fabsf(lon)))*65536.0f);
	tmp[6] = v>>8;
	tmp[7] = v&0xFF;

	if(lat>=0.0f)
		tmp[8] |= M17_META_LAT_NORTH;
	else
		tmp[8] |= M17_META_LAT_SOUTH;

	if(lon>=0.0f)
		tmp[8] |= M17_META_LON_EAST;
	else
		tmp[8] |= M17_META_LON_WEST;

	tmp[8] |= flags;

	if(altitude <= -1500)
		v = 0;
	else if(altitude >= (0x10000-1500))
		v = 0xFFFF;
	else
		v = altitude + 1500;
	tmp[9] = v>>8;
	tmp[10] = v&0xFF;

	tmp[11] = bearing>>8;
	tmp[12] = bearing&0xFF;

	tmp[13] = speed;

	set_LSF_meta(lsf, tmp);
}

/**
 * @brief Fill the LSF META field with Extended Callsign Data and update the CRC.
 * 
 * @param lsf Pointer to an LSF struct.
 * @param cf1 Callsign Field 1.
 * @param cf2 Callsign Field 2.
 */
void set_LSF_meta_ecd(lsf_t *lsf, const char *cf1, const char *cf2)
{
	uint8_t tmp[14] = {0};

	encode_callsign_bytes(&tmp[0], (uint8_t*)cf1);
	encode_callsign_bytes(&tmp[6], (uint8_t*)cf2);

	set_LSF_meta(lsf, tmp);
}

/**
 * @brief Fill the LSF META field with nonce and update the CRC.
 * 
 * @param lsf Pointer to an LSF struct.
 * @param ts Timestamp (Unix epoch).
 * @param rand Random, 10-byte vector.
 */
void set_LSF_meta_nonce(lsf_t *lsf, const time_t ts, const uint8_t rand[10])
{
	uint8_t tmp[14] = {0};
	uint32_t ts_2020 = (uint32_t)ts - 1577836800UL; //convert to 2020 epoch

	//copy the timestamp MSB to LSB (big-endian)
	for(uint8_t i=0; i<4; i++)
		tmp[i] = ts_2020 >> (24-(i*8));
	
	//copy the 10-byte random part
	memcpy(&tmp[4], rand, 10);

	set_LSF_meta(lsf, tmp);
}

/**
 * @brief Decode the LSF META position data.
 * 
 * @param data_source Data source.
 * @param station_type Type of the transmitting station.
 * @param lat Latitude in degrees.
 * @param lon Longitude in degrees.
 * @param flags Hemisphere, altitude, speed, and bearing field.
 * @param altitude Altitude in feet (-1500..64035).
 * @param bearing Bearing in degrees.
 * @param speed Speed in miles per hour.
 * @param lsf Pointer to an LSF struct.
 * @return 0 if CRC is valid, -1 otherwise.
 */
int8_t get_LSF_meta_position(uint8_t *data_source, uint8_t *station_type,
	float *lat, float *lon, uint8_t *flags, int32_t *altitude, uint16_t *bearing, uint8_t *speed, const lsf_t *lsf)
{
	if(CRC_M17((uint8_t*)lsf, sizeof(*lsf)))
		return -1;

	uint8_t tmp[14];

	memcpy(tmp, lsf->meta, 14);

	if(data_source!=NULL) *data_source=tmp[0];
	if(station_type!=NULL) *station_type=tmp[1];

	if(lat!=NULL)
	{
		*lat = 0.0f;

		if(tmp[8] & M17_META_LAT_SOUTH)
		{
			*lat = -tmp[2];
			*lat -= (((uint16_t)tmp[3]<<8)+tmp[4])/65536.0f;
		}
		else
		{
			*lat = tmp[2];
			*lat += (((uint16_t)tmp[3]<<8)+tmp[4])/65536.0f;
		}
	}

	if(lon!=NULL)
	{
		*lon = 0.0f;

		if(tmp[8] & M17_META_LON_WEST)
		{
			*lon = -tmp[5];
			*lon -= (((uint16_t)tmp[6]<<8)+tmp[7])/65536.0f;
		}
		else
		{
			*lon = tmp[5];
			*lon += (((uint16_t)tmp[6]<<8)+tmp[7])/65536.0f;
		}
	}

	if(flags!=NULL) *flags = tmp[8];

	if(altitude!=NULL)
	{
		*altitude = (((uint16_t)tmp[9]<<8)+tmp[10]) - 1500.0f;
	}

	if(bearing!=NULL)
	{
		*bearing = (((uint16_t)tmp[11]<<8)+tmp[12]);
	}

	if(speed!=NULL) *speed = tmp[13];

	return 0;
}
