//--------------------------------------------------------------------
// M17 C library - decode/viterbi.c
//
// This file contains:
// - the Viterbi decoder
//
// Wojciech Kaczmarski, SP5WWP
// M17 Project, 29 December 2023
//--------------------------------------------------------------------
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <m17.h>
#include <m17_safe.h>

// Thread-safe Viterbi decoder state
typedef struct {
    uint32_t prevMetrics[M17_CONVOL_STATES];
    uint32_t currMetrics[M17_CONVOL_STATES];
    uint32_t prevMetricsData[M17_CONVOL_STATES];
    uint32_t currMetricsData[M17_CONVOL_STATES];
    uint16_t viterbi_history[244];
    bool initialized;
} viterbi_state_t;

// Thread-local storage for Viterbi state
#ifdef M17_THREAD_SAFE
static __thread viterbi_state_t* viterbi_state = NULL;
#else
static viterbi_state_t* viterbi_state = NULL;
#endif

// Initialize Viterbi state
static m17_error_t viterbi_init_state(void)
{
    if (viterbi_state == NULL) {
        viterbi_state = (viterbi_state_t*)calloc(1, sizeof(viterbi_state_t));
        if (viterbi_state == NULL) {
            return M17_ERROR_MEMORY_ALLOCATION;
        }
        viterbi_state->initialized = true;
    }
    return M17_SUCCESS;
}

/**
 * @brief Decode unpunctured convolutionally encoded data.
 *
 * @param out Destination array where decoded data is written.
 * @param in Input data.
 * @param len Input length in bits.
 * @return Number of bit errors corrected.
 */
uint32_t viterbi_decode(uint8_t* out, const uint16_t* in, const uint16_t len)
{
    // Input validation
    if (out == NULL || in == NULL) {
        return 0xFFFFFFFF; // Error indicator
    }
    
    if (len == 0 || len > 244*2) {
        return 0xFFFFFFFF; // Error indicator
    }
    
    // Initialize state if needed
    m17_error_t err = viterbi_init_state();
    if (err != M17_SUCCESS) {
        return 0xFFFFFFFF; // Error indicator
    }
    
    M17_MUTEX_LOCK(viterbi_mutex);
    
    viterbi_reset();

    size_t pos = 0;
    for(size_t i = 0; i < len; i += 2)
    {
        uint16_t s0 = in[i];
        uint16_t s1 = in[i + 1];

        viterbi_decode_bit(s0, s1, pos);
        pos++;
    }

    uint32_t result = viterbi_chainback(out, pos, len/2);
    
    M17_MUTEX_UNLOCK(viterbi_mutex);
    
    return result;
}

/**
 * @brief Decode punctured convolutionally encoded data.
 *
 * @param out Destination array where decoded data is written.
 * @param in Input data.
 * @param punct Puncturing matrix.
 * @param in_len Input data length.
 * @param p_len Puncturing matrix length (entries).
 * @return Number of bit errors corrected.
 */
uint32_t viterbi_decode_punctured(uint8_t* out, const uint16_t* in, const uint8_t* punct, const uint16_t in_len, const uint16_t p_len)
{
    // Input validation
    if (out == NULL || in == NULL || punct == NULL) {
        return 0xFFFFFFFF; // Error indicator
    }
    
    if (in_len == 0 || in_len > 244*2 || p_len == 0) {
        return 0xFFFFFFFF; // Error indicator
    }

	uint16_t umsg[244*2];           //unpunctured message
	uint8_t p=0;		            //puncturer matrix entry
	uint16_t u=0;		            //bits count - unpunctured message
    uint16_t i=0;                   //bits read from the input message

	while(i<in_len)
	{
		if(punct[p])
		{
			umsg[u]=in[i];
			i++;
		}
		else
		{
			umsg[u]=0x7FFF;
		}

		u++;
		p++;
		p%=p_len;
	}

    return viterbi_decode(out, umsg, u) - (u-in_len)*0x7FFF;
}

/**
 * @brief Decode one bit and update trellis.
 *
 * @param s0 Cost of the first symbol.
 * @param s1 Cost of the second symbol.
 * @param pos Bit position in history.
 */
void viterbi_decode_bit(uint16_t s0, uint16_t s1, const size_t pos)
{
    if (viterbi_state == NULL) {
        return; // State not initialized
    }
    
    static const uint16_t COST_TABLE_0[] = {0, 0, 0, 0, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};
    static const uint16_t COST_TABLE_1[] = {0, 0xFFFF, 0xFFFF, 0, 0, 0xFFFF, 0xFFFF, 0};

    for(uint8_t i = 0; i < M17_CONVOL_STATES/2; i++)
    {
        uint32_t metric = q_abs_diff(COST_TABLE_0[i], s0)
                        + q_abs_diff(COST_TABLE_1[i], s1);


        uint32_t m0 = viterbi_state->prevMetrics[i] + metric;
        uint32_t m1 = viterbi_state->prevMetrics[i + M17_CONVOL_STATES/2] + (0x1FFFE - metric);

        uint32_t m2 = viterbi_state->prevMetrics[i] + (0x1FFFE - metric);
        uint32_t m3 = viterbi_state->prevMetrics[i + M17_CONVOL_STATES/2] + metric;

        uint8_t i0 = 2 * i;
        uint8_t i1 = i0 + 1;

        if(m0 >= m1)
        {
            viterbi_state->viterbi_history[pos]|=(1<<i0);
            viterbi_state->currMetrics[i0] = m1;
        }
        else
        {
            viterbi_state->viterbi_history[pos]&=~(1<<i0);
            viterbi_state->currMetrics[i0] = m0;
        }

        if(m2 >= m3)
        {
            viterbi_state->viterbi_history[pos]|=(1<<i1);
            viterbi_state->currMetrics[i1] = m3;
        }
        else
        {
            viterbi_state->viterbi_history[pos]&=~(1<<i1);
            viterbi_state->currMetrics[i1] = m2;
        }
    }

    //swap
    uint32_t tmp[M17_CONVOL_STATES];
    for(uint8_t i=0; i<M17_CONVOL_STATES; i++)
    {
    	tmp[i]=viterbi_state->currMetrics[i];
	}
	for(uint8_t i=0; i<M17_CONVOL_STATES; i++)
    {
    	viterbi_state->currMetrics[i]=viterbi_state->prevMetrics[i];
    	viterbi_state->prevMetrics[i]=tmp[i];
	}
}

/**
 * @brief History chainback to obtain final byte array.
 *
 * @param out Destination byte array for decoded data.
 * @param pos Starting position for the chainback.
 * @param len Length of the output in bits (minus K-1=4).
 * @return Minimum Viterbi cost at the end of the decode sequence.
 */
uint32_t viterbi_chainback(uint8_t* out, size_t pos, uint16_t len)
{
    if (viterbi_state == NULL) {
        return 0xFFFFFFFF; // Error indicator
    }
    
    uint8_t state = 0;
    size_t bitPos = len+4;

    memset(out, 0, (bitPos/8)+1);

    while(pos > 0)
    {
        bitPos--;
        pos--;
        uint16_t bit = viterbi_state->viterbi_history[pos]&((1<<(state>>4)));
        state >>= 1;
        if(bit)
        {
        	state |= 0x80;
        	out[bitPos/8]|=1<<(7-(bitPos%8));
		}
    }

    uint32_t cost = viterbi_state->prevMetrics[0];

    for(size_t i = 0; i < M17_CONVOL_STATES; i++)
    {
        uint32_t m = viterbi_state->prevMetrics[i];
        if(m < cost) cost = m;
    }

    return cost;
}

/**
 * @brief Reset the decoder state. No args.
 *
 */
void viterbi_reset(void)
{
    if (viterbi_state == NULL) {
        return; // State not initialized
    }
    
	memset((uint8_t*)viterbi_state->viterbi_history, 0, 2*244);
	memset((uint8_t*)viterbi_state->currMetrics, 0, 4*M17_CONVOL_STATES);
    memset((uint8_t*)viterbi_state->prevMetrics, 0, 4*M17_CONVOL_STATES);
    memset((uint8_t*)viterbi_state->currMetricsData, 0, 4*M17_CONVOL_STATES);
    memset((uint8_t*)viterbi_state->prevMetricsData, 0, 4*M17_CONVOL_STATES);
}
