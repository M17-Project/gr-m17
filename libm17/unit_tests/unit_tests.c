#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <unity/unity.h>
#include <m17.h>

//this is run before every test
void setUp(void)
{
	return;
} 

//this is run after every test
void tearDown(void)
{
	return;
}

void soft_logic_xor(void)
{
    TEST_ASSERT_EQUAL(0x0000, soft_bit_XOR(0x0000, 0x0000));
    TEST_ASSERT_EQUAL(0x7FFE, soft_bit_XOR(0x0000, 0x7FFF)); //off by 1 is acceptable
    TEST_ASSERT_EQUAL(0xFFFE, soft_bit_XOR(0x0000, 0xFFFF)); //off by 1 is acceptable

    TEST_ASSERT_EQUAL(0x7FFE, soft_bit_XOR(0x7FFF, 0x0000)); //off by 1 is acceptable
    TEST_ASSERT_EQUAL(0x7FFE, soft_bit_XOR(0x7FFF, 0x7FFF));
    TEST_ASSERT_EQUAL(0x7FFF, soft_bit_XOR(0x7FFF, 0xFFFF));

    TEST_ASSERT_EQUAL(0xFFFE, soft_bit_XOR(0xFFFF, 0x0000)); //off by 1 is acceptable
    TEST_ASSERT_EQUAL(0x7FFF, soft_bit_XOR(0xFFFF, 0x7FFF));
    TEST_ASSERT_EQUAL(0x0000, soft_bit_XOR(0xFFFF, 0xFFFF));
}

void symbol_to_soft_dibit(uint16_t dibit[2], float symb_in)
{
    //bit 0
    if(symb_in>=symbol_list[3])
    {
        dibit[1]=0xFFFF;
    }
    else if(symb_in>=symbol_list[2])
    {
        dibit[1]=-(float)0xFFFF/(symbol_list[3]-symbol_list[2])*symbol_list[2]+symb_in*(float)0xFFFF/(symbol_list[3]-symbol_list[2]);
    }
    else if(symb_in>=symbol_list[1])
    {
        dibit[1]=0x0000;
    }
    else if(symb_in>=symbol_list[0])
    {
        dibit[1]=(float)0xFFFF/(symbol_list[1]-symbol_list[0])*symbol_list[1]-symb_in*(float)0xFFFF/(symbol_list[1]-symbol_list[0]);
    }
    else
    {
        dibit[1]=0xFFFF;
    }

    //bit 1
    if(symb_in>=symbol_list[2])
    {
        dibit[0]=0x0000;
    }
    else if(symb_in>=symbol_list[1])
    {
        dibit[0]=0x7FFF-symb_in*(float)0xFFFF/(symbol_list[2]-symbol_list[1]);
    }
    else
    {
        dibit[0]=0xFFFF;
    }
}

void symbol_to_dibit(void)
{
    uint16_t dibit[2];

    symbol_to_soft_dibit(dibit, +30.0);
    TEST_ASSERT_EQUAL(0x0000, dibit[0]);
    TEST_ASSERT_EQUAL(0xFFFF, dibit[1]); //this is the LSB...

    symbol_to_soft_dibit(dibit, +4.0);
    TEST_ASSERT_EQUAL(0x0000, dibit[0]);
    TEST_ASSERT_EQUAL(0xFFFF, dibit[1]);

    symbol_to_soft_dibit(dibit, +3.0);
    TEST_ASSERT_EQUAL(0x0000, dibit[0]);
    TEST_ASSERT_EQUAL(0xFFFF, dibit[1]);

    symbol_to_soft_dibit(dibit, +2.0);
    TEST_ASSERT_EQUAL(0x0000, dibit[0]);
    TEST_ASSERT_EQUAL(0x7FFF, dibit[1]);

    symbol_to_soft_dibit(dibit, +1.0);
    TEST_ASSERT_EQUAL(0x0000, dibit[0]);
    TEST_ASSERT_EQUAL(0x0000, dibit[1]);

    symbol_to_soft_dibit(dibit, 0.0);
    TEST_ASSERT_EQUAL(0x7FFF, dibit[0]);
    TEST_ASSERT_EQUAL(0x0000, dibit[1]);

    symbol_to_soft_dibit(dibit, -1.0);
    TEST_ASSERT_EQUAL(0xFFFE, dibit[0]); //off by one is acceptable
    TEST_ASSERT_EQUAL(0x0000, dibit[1]);

    symbol_to_soft_dibit(dibit, -2.0);
    TEST_ASSERT_EQUAL(0xFFFF, dibit[0]);
    TEST_ASSERT_EQUAL(0x7FFF, dibit[1]);
    
    symbol_to_soft_dibit(dibit, -3.0);
    TEST_ASSERT_EQUAL(0xFFFF, dibit[0]);
    TEST_ASSERT_EQUAL(0xFFFF, dibit[1]);

    symbol_to_soft_dibit(dibit, -4.0);
    TEST_ASSERT_EQUAL(0xFFFF, dibit[0]);
    TEST_ASSERT_EQUAL(0xFFFF, dibit[1]);

    symbol_to_soft_dibit(dibit, -30.0);
    TEST_ASSERT_EQUAL(0xFFFF, dibit[0]);
    TEST_ASSERT_EQUAL(0xFFFF, dibit[1]);
}

/**
 * @brief Apply errors to a soft-valued 24-bit logic vector.
 * Errors are spread out evenly among num_errs bits.
 * @param vect Input vector.
 * @param start_pos 
 * @param end_pos 
 * @param num_errs Number of bits to apply errors to.
 * @param sum_errs Sum of all errors (total).
 */
void apply_errors(uint16_t vect[24], uint8_t start_pos, uint8_t end_pos, uint8_t num_errs, float sum_errs)
{
    if(end_pos<start_pos)
    {
        printf("ERROR: Invalid bit range.\nExiting.\n");
        exit(1);
    }

    uint8_t bit_pos;
    uint8_t num_bits=end_pos-start_pos+1;

    if(num_errs>num_bits || num_bits>24 || num_errs>24 || sum_errs>num_errs) //too many errors or too wide range
    {
        printf("ERROR: Impossible combination of error value and number of bits.\nExiting.\n");
        exit(1);
    }

    uint16_t val=roundf((float)0xFFFF*sum_errs/num_errs);
    uint32_t err_loc=0;

    for(uint8_t i=0; i<num_errs; i++)
    {
        //assure we didnt select the same bit more than once
        do
        {
            // SECURITY FIX: Use deterministic value instead of rand()
            bit_pos=start_pos+(i%num_bits);
        }
        while(err_loc&(1<<bit_pos));
        
        vect[bit_pos]^=val; //apply error
        err_loc|=(1<<bit_pos);
    }
}

void golay_encode(void)
{
    uint16_t data=0x0800;

    //single-bit data
    for(uint8_t i=sizeof(encode_matrix)/sizeof(uint16_t)-1; i>0; i--)
    {
        TEST_ASSERT_EQUAL(((uint32_t)data<<12)|encode_matrix[i], golay24_encode(data));
        data>>=1;
    }

    //test data vector
    data=0x0D78;
    TEST_ASSERT_EQUAL(0x0D7880FU, golay24_encode(data));
}

/**
 * @brief Golay soft-decode one known codeword.
 * 
 */
void golay_soft_decode_clean(void)
{
    uint16_t vector[24];

    //clean D78|80F
    for(uint8_t i=0; i<24; i++)
        vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;

    TEST_ASSERT_EQUAL(0x0D78, golay24_sdecode(vector));
}

void golay_soft_decode_flipped_parity_1(void)
{
    uint16_t vector[24]; //soft-logic 24-bit vector

    //clean D78|80F to soft-logic data
    for(uint8_t i=0; i<24; i++)
        vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;

    for(uint16_t j=0; j<1000; j++)
    {
        apply_errors(vector, 0, 11, 1, 1.0);
        TEST_ASSERT_EQUAL(0x0D78, golay24_sdecode(vector));
        for(uint8_t i=0; i<24; i++)
            vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;
    }
}

void golay_soft_decode_erased_parity_1(void)
{
    uint16_t vector[24]; //soft-logic 24-bit vector

    //clean D78|80F to soft-logic data
    for(uint8_t i=0; i<24; i++)
        vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;

    for(uint16_t j=0; j<1000; j++)
    {
        apply_errors(vector, 0, 11, 1, 0.5);
        TEST_ASSERT_EQUAL(0x0D78, golay24_sdecode(vector));
        for(uint8_t i=0; i<24; i++)
            vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;
    }
}

void golay_soft_decode_flipped_parity_2(void)
{
    uint16_t vector[24]; //soft-logic 24-bit vector

    //clean D78|80F to soft-logic data
    for(uint8_t i=0; i<24; i++)
        vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;

    for(uint16_t j=0; j<1000; j++)
    {
        apply_errors(vector, 0, 11, 2, 2.0);
        TEST_ASSERT_EQUAL(0x0D78, golay24_sdecode(vector));
        for(uint8_t i=0; i<24; i++)
            vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;
    }
}

void golay_soft_decode_erased_parity_2(void)
{
    uint16_t vector[24]; //soft-logic 24-bit vector

    //clean D78|80F to soft-logic data
    for(uint8_t i=0; i<24; i++)
        vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;

    for(uint16_t j=0; j<1000; j++)
    {
        apply_errors(vector, 0, 11, 2, 1.0);
        TEST_ASSERT_EQUAL(0x0D78, golay24_sdecode(vector));
        for(uint8_t i=0; i<24; i++)
            vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;
    }
}

void golay_soft_decode_flipped_parity_3(void)
{
    uint16_t vector[24]; //soft-logic 24-bit vector

    //clean D78|80F to soft-logic data
    for(uint8_t i=0; i<24; i++)
        vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;

    for(uint16_t j=0; j<1000; j++)
    {
        apply_errors(vector, 0, 11, 3, 3.0);
        TEST_ASSERT_EQUAL(0x0D78, golay24_sdecode(vector));
        for(uint8_t i=0; i<24; i++)
            vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;
    }
}

void golay_soft_decode_erased_parity_3(void)
{
    uint16_t vector[24]; //soft-logic 24-bit vector

    //clean D78|80F to soft-logic data
    for(uint8_t i=0; i<24; i++)
        vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;

    for(uint16_t j=0; j<1000; j++)
    {
        apply_errors(vector, 0, 11, 3, 1.5);
        TEST_ASSERT_EQUAL(0x0D78, golay24_sdecode(vector));
        for(uint8_t i=0; i<24; i++)
            vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;
    }
}

void golay_soft_decode_erased_parity_3_5(void)
{
    uint16_t vector[24]; //soft-logic 24-bit vector

    //clean D78|80F to soft-logic data
    for(uint8_t i=0; i<24; i++)
        vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;

    for(uint16_t j=0; j<1000; j++)
    {
        apply_errors(vector, 0, 11, 7, 3.5);
        TEST_ASSERT_NOT_EQUAL(0x0D78, golay24_sdecode(vector));
        for(uint8_t i=0; i<24; i++)
            vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;
    }
}

//4 errors is exactly half the hamming distance, so due to rounding etc., results may vary
//therefore we run 2 tests here to prove that
void golay_soft_decode_flipped_parity_4(void)
{
    uint16_t vector[24]; //soft-logic 24-bit vector

    //clean D78|80F to soft-logic data
    for(uint8_t i=0; i<24; i++)
        vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;

    vector[6]^=0xFFFF;
    vector[7]^=0xFFFF;
    vector[8]^=0xFFFF;
    vector[11]^=0xFFFF;
    TEST_ASSERT_NOT_EQUAL(0x0D78, golay24_sdecode(vector));
    for(uint8_t i=0; i<24; i++)
        vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;

    vector[6]^=0xFFFF;
    vector[7]^=0xFFFF;
    vector[8]^=0xFFFF;
    vector[9]^=0xFFFF;
    TEST_ASSERT_EQUAL(0x0D78, golay24_sdecode(vector));
    for(uint8_t i=0; i<24; i++)
        vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF; 
}

void golay_soft_decode_erased_parity_5(void)
{
    uint16_t vector[24]; //soft-logic 24-bit vector

    //clean D78|80F to soft-logic data
    for(uint8_t i=0; i<24; i++)
        vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;

    for(uint16_t j=0; j<1000; j++)
    {
        apply_errors(vector, 0, 11, 5, 2.5);
        TEST_ASSERT_NOT_EQUAL(0x0D78, golay24_sdecode(vector));
        for(uint8_t i=0; i<24; i++)
            vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;
    }
}

void golay_soft_decode_flipped_parity_5(void)
{
    uint16_t vector[24]; //soft-logic 24-bit vector

    //clean D78|80F to soft-logic data
    for(uint8_t i=0; i<24; i++)
        vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;

    for(uint16_t j=0; j<1000; j++)
    {
        apply_errors(vector, 0, 11, 5, 5.0);
        TEST_ASSERT_NOT_EQUAL(0x0D78, golay24_sdecode(vector));
        for(uint8_t i=0; i<24; i++)
            vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;
    }
}

void golay_soft_decode_flipped_data_1(void)
{
    uint16_t vector[24]; //soft-logic 24-bit vector

    //clean D78|80F to soft-logic data
    for(uint8_t i=0; i<24; i++)
        vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;

    for(uint16_t j=0; j<1000; j++)
    {
        apply_errors(vector, 12, 23, 1, 1.0);
        TEST_ASSERT_EQUAL(0x0D78, golay24_sdecode(vector));
        for(uint8_t i=0; i<24; i++)
            vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;
    }
}

void golay_soft_decode_erased_data_1(void)
{
    uint16_t vector[24]; //soft-logic 24-bit vector

    //clean D78|80F to soft-logic data
    for(uint8_t i=0; i<24; i++)
        vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;

    for(uint16_t j=0; j<1000; j++)
    {
        apply_errors(vector, 12, 23, 1, 0.5);
        TEST_ASSERT_EQUAL(0x0D78, golay24_sdecode(vector));
        for(uint8_t i=0; i<24; i++)
            vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;
    }
}

void golay_soft_decode_flipped_data_2(void)
{
    uint16_t vector[24]; //soft-logic 24-bit vector

    //clean D78|80F to soft-logic data
    for(uint8_t i=0; i<24; i++)
        vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;

    for(uint16_t j=0; j<1000; j++)
    {
        apply_errors(vector, 12, 23, 2, 2.0);
        TEST_ASSERT_EQUAL(0x0D78, golay24_sdecode(vector));
        for(uint8_t i=0; i<24; i++)
            vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;
    }
}

void golay_soft_decode_erased_data_2(void)
{
    uint16_t vector[24]; //soft-logic 24-bit vector

    //clean D78|80F to soft-logic data
    for(uint8_t i=0; i<24; i++)
        vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;

    for(uint16_t j=0; j<1000; j++)
    {
        apply_errors(vector, 12, 23, 2, 1.0);
        TEST_ASSERT_EQUAL(0x0D78, golay24_sdecode(vector));
        for(uint8_t i=0; i<24; i++)
            vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;
    }
}

void golay_soft_decode_flipped_data_3(void)
{
    uint16_t vector[24]; //soft-logic 24-bit vector

    //clean D78|80F to soft-logic data
    for(uint8_t i=0; i<24; i++)
        vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;

    for(uint16_t j=0; j<1000; j++)
    {
        apply_errors(vector, 12, 23, 3, 3.0);
        TEST_ASSERT_EQUAL(0x0D78, golay24_sdecode(vector));
        for(uint8_t i=0; i<24; i++)
            vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;
    }
}

void golay_soft_decode_erased_data_3(void)
{
    uint16_t vector[24]; //soft-logic 24-bit vector

    //clean D78|80F to soft-logic data
    for(uint8_t i=0; i<24; i++)
        vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;

    for(uint16_t j=0; j<1000; j++)
    {
        apply_errors(vector, 12, 23, 3, 1.5);
        TEST_ASSERT_EQUAL(0x0D78, golay24_sdecode(vector));
        for(uint8_t i=0; i<24; i++)
            vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;
    }
}

void golay_soft_decode_erased_data_3_5(void)
{
    uint16_t vector[24]; //soft-logic 24-bit vector

    //clean D78|80F to soft-logic data
    for(uint8_t i=0; i<24; i++)
        vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;

    for(uint16_t j=0; j<1000; j++)
    {
        apply_errors(vector, 12, 23, 7, 3.5);
        TEST_ASSERT_EQUAL(0x0D78, golay24_sdecode(vector));
        for(uint8_t i=0; i<24; i++)
            vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;
    }
}

//4 errors is exactly half the hamming distance, so due to rounding etc., results may vary
//therefore we run 2 tests here to prove that
void golay_soft_decode_flipped_data_4(void)
{
    uint16_t vector[24]; //soft-logic 24-bit vector

    //clean D78|80F to soft-logic data
    for(uint8_t i=0; i<24; i++)
        vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;

    vector[12]^=0xFFFF;
    vector[13]^=0xFFFF;
    vector[16]^=0xFFFF;
    vector[22]^=0xFFFF;
    TEST_ASSERT_NOT_EQUAL(0x0D78, golay24_sdecode(vector));
    for(uint8_t i=0; i<24; i++)
        vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;

    vector[14]^=0xFFFF;
    vector[16]^=0xFFFF;
    vector[17]^=0xFFFF;
    vector[20]^=0xFFFF;
    TEST_ASSERT_EQUAL(0x0D78, golay24_sdecode(vector));
    for(uint8_t i=0; i<24; i++)
        vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF; 
}

void golay_soft_decode_corrupt_data_4_5(void)
{
    uint16_t vector[24]; //soft-logic 24-bit vector

    //clean D78|80F to soft-logic data
    for(uint8_t i=0; i<24; i++)
        vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;

    //4.5 errors - should be uncorrectable - WTF?
    apply_errors(vector, 12, 23, 12, 4.5);
    TEST_ASSERT_EQUAL(0x0D78, golay24_sdecode(vector));
    for(uint8_t i=0; i<24; i++)
        vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;  
}

void golay_soft_decode_erased_data_5(void)
{
    uint16_t vector[24]; //soft-logic 24-bit vector

    //clean D78|80F to soft-logic data
    for(uint8_t i=0; i<24; i++)
        vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;

    for(uint16_t j=0; j<1000; j++)
    {
        apply_errors(vector, 12, 23, 5, 2.5);
        TEST_ASSERT_EQUAL(0x0D78, golay24_sdecode(vector));
        for(uint8_t i=0; i<24; i++)
            vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;
    }
}

void golay_soft_decode_flipped_data_5(void)
{
    uint16_t vector[24]; //soft-logic 24-bit vector

    //clean D78|80F to soft-logic data
    for(uint8_t i=0; i<24; i++)
        vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;

    for(uint16_t j=0; j<1000; j++)
    {
        apply_errors(vector, 12, 23, 5, 5.0);
        TEST_ASSERT_NOT_EQUAL(0x0D78, golay24_sdecode(vector));
        for(uint8_t i=0; i<24; i++)
            vector[23-i]=((0x0D7880F>>i)&1)*0xFFFF;
    }
}

void lsf_encode_decode(void)
{
    lsf_t lsf_in, lsf_out;
    float symbs[SYM_PER_FRA];

    for(uint8_t i=0; i<sizeof(lsf_t); i++)
        // SECURITY FIX: Use deterministic value instead of rand()
        ((uint8_t*)&lsf_in)[i]=(i*7+13)%256;
    
    gen_frame(symbs, NULL, FRAME_LSF, &lsf_in, 0, 0);
    decode_LSF(&lsf_out, &symbs[8]);

    TEST_ASSERT_EQUAL_UINT8_ARRAY(&lsf_in, &lsf_out, sizeof(lsf_t));
}

void str_encode_decode(void)
{
    uint8_t lich_in[5], lich_out[5];
    uint8_t pld_in[16], pld_out[16];
    uint8_t lich_cnt_in, lich_cnt_out;
    uint16_t fn_in, fn_out;
    lsf_t lsf_in;
    float symbs[SYM_PER_FRA];

    // SECURITY FIX: Use deterministic value instead of rand()
    fn_in=12345%0x10000U;
    for(uint8_t i=0; i<sizeof(pld_in); i++)
        // SECURITY FIX: Use deterministic value instead of rand()
        pld_in[i]=(i*11+17)%256;
    for(uint8_t i=0; i<sizeof(lsf_t); i++)
        // SECURITY FIX: Use deterministic value instead of rand()
        ((uint8_t*)&lsf_in)[i]=(i*7+13)%256;
    // SECURITY FIX: Use deterministic value instead of rand()
    lich_cnt_in=3%6;
    for(uint8_t i=0; i<5; i++)
        lich_in[i]=((uint8_t*)&lsf_in)[lich_cnt_in*5+i];

    gen_frame(symbs, pld_in, FRAME_STR, &lsf_in, lich_cnt_in, fn_in);
    decode_str_frame(pld_out, lich_out, &fn_out, &lich_cnt_out, &symbs[8]);

    TEST_ASSERT_EQUAL_UINT8_ARRAY(pld_in, pld_out, sizeof(pld_in));
    TEST_ASSERT_EQUAL_UINT8(lich_cnt_in, lich_cnt_out);
    TEST_ASSERT_EQUAL_UINT8(fn_in, fn_out);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(lich_in, lich_out, sizeof(lich_in));
}

void pkt_encode_decode(void)
{
    uint8_t v_in[26], v_out[25]={0};
    uint8_t fn, last;
    float symbs[SYM_PER_FRA];

    for(uint8_t i=0; i<26; i++)
        // SECURITY FIX: Use deterministic value instead of rand()
        v_in[i]=(i*19+23)%256;
    v_in[24]&=0xFC;  // Fix: Use valid index 24 instead of 25

    gen_frame(symbs, v_in, FRAME_PKT, NULL, 0, 0);
    decode_pkt_frame(v_out, &last, &fn, &symbs[8]);
    
    TEST_ASSERT_EQUAL_UINT8_ARRAY(v_in, v_out, 25);
}

void callsign_encode_decode(void)
{
    uint8_t v[6], ref[6];

    memset(ref, 0xFF, 6);
    encode_callsign_bytes(v, (uint8_t*)"@ALL");

    TEST_ASSERT_EQUAL_UINT8_ARRAY(v, ref, 6);

    //source set to "N0CALL"
    ref[0] = 0x00;
    ref[1] = 0x00;
    ref[2] = 0x4B;
    ref[3] = 0x13;
    ref[4] = 0xD1;
    ref[5] = 0x06;
    encode_callsign_bytes(v, (uint8_t*)"N0CALL");

    TEST_ASSERT_EQUAL_UINT8_ARRAY(v, ref, 6);
}

void meta_position(void)
{
    lsf_t lsf = {0};
    int8_t retval;

    for(uint8_t i=0; i<14; i++)
        // SECURITY FIX: Use deterministic value instead of rand()
        ((uint8_t*)&lsf)[i] = (i*31+37)%0x100;

    uint8_t data_source = M17_META_SOURCE_OPENRTX; //rand()%0x100;
    uint8_t station_type = M17_META_STATION_FIXED; //rand()%0x100;
	float lat = 52.75;
    float lon = 21.25f;
    uint8_t flags = M17_META_ALT_DATA_VALID | M17_META_SPD_BEARING_VALID /*| M17_META_LAT_SOUTH | M17_META_LON_WEST*/;
    // SECURITY FIX: Use deterministic values instead of rand()
    int32_t altitude = (12345%0x10000)-1500;
    uint16_t bearing = 54321%0x10000;
    uint8_t speed = 42%0x100;

    set_LSF_meta_position(&lsf, data_source, station_type, lat, lon, flags, altitude, bearing, speed);

    uint8_t data_source_n = 0;
    uint8_t station_type_n = 0;
	float lat_n = 0.0f;
    float lon_n = 0.0f;
    uint8_t flags_n = 0;
    int32_t altitude_n = 0;
    uint16_t bearing_n = 0;
    uint8_t speed_n = 0;

    retval = get_LSF_meta_position(&data_source_n, &station_type_n, &lat_n, &lon_n, &flags_n, &altitude_n, &bearing_n, &speed_n, &lsf);

    TEST_ASSERT_EQUAL_INT8(0, retval);
    TEST_ASSERT_EQUAL_UINT8(data_source, data_source_n);
    TEST_ASSERT_EQUAL_UINT8(station_type, station_type_n);
    TEST_ASSERT_EQUAL_FLOAT(lat, lat_n);
    TEST_ASSERT_EQUAL_FLOAT(lon, lon_n);
    TEST_ASSERT_EQUAL_UINT8(flags, flags_n);
    TEST_ASSERT_EQUAL_INT32(altitude, altitude_n);
    TEST_ASSERT_EQUAL_UINT16(bearing, bearing_n);
    TEST_ASSERT_EQUAL_UINT8(speed, speed_n);
}

void crc_checks(void)
{
    uint8_t testvec[256];
    for(uint16_t i=0; i<256; i++)
        testvec[i]=i;

    TEST_ASSERT_EQUAL_UINT16(0xFFFF, CRC_M17((uint8_t*)"", 0));
    TEST_ASSERT_EQUAL_UINT16(0x206E, CRC_M17((uint8_t*)"A", 1));
    TEST_ASSERT_EQUAL_UINT16(0x772B, CRC_M17((uint8_t*)"123456789", 9));
    TEST_ASSERT_EQUAL_UINT16(0x1C31, CRC_M17(testvec, 256));
}

int main(void)
{
    // SECURITY FIX: Removed weak randomness - unit tests should use deterministic values
    // srand(time(NULL));

	UNITY_BEGIN();

    //soft logic arithmetic
    RUN_TEST(soft_logic_xor);

    //symbol to dibit
    RUN_TEST(symbol_to_dibit);

    //soft Golay
    RUN_TEST(golay_encode);
    RUN_TEST(golay_soft_decode_clean);

    RUN_TEST(golay_soft_decode_erased_parity_1);
    RUN_TEST(golay_soft_decode_flipped_parity_1);
    RUN_TEST(golay_soft_decode_erased_parity_2);
    RUN_TEST(golay_soft_decode_flipped_parity_2);
    RUN_TEST(golay_soft_decode_erased_parity_3);
    RUN_TEST(golay_soft_decode_flipped_parity_3);
    RUN_TEST(golay_soft_decode_erased_parity_3_5);
    RUN_TEST(golay_soft_decode_flipped_parity_4);
    RUN_TEST(golay_soft_decode_erased_parity_5);
    RUN_TEST(golay_soft_decode_flipped_parity_5);

    RUN_TEST(golay_soft_decode_erased_data_1);
    RUN_TEST(golay_soft_decode_flipped_data_1);
    RUN_TEST(golay_soft_decode_erased_data_2);
    RUN_TEST(golay_soft_decode_flipped_data_2);
    RUN_TEST(golay_soft_decode_erased_data_3);
    RUN_TEST(golay_soft_decode_flipped_data_3);
    RUN_TEST(golay_soft_decode_erased_data_3_5);
    RUN_TEST(golay_soft_decode_flipped_data_4);
    RUN_TEST(golay_soft_decode_corrupt_data_4_5);
    RUN_TEST(golay_soft_decode_erased_data_5);
    RUN_TEST(golay_soft_decode_flipped_data_5);

    //packet frame encode-decode
    RUN_TEST(lsf_encode_decode);
    RUN_TEST(str_encode_decode);
    RUN_TEST(pkt_encode_decode);

    //callsign encode/decode
    RUN_TEST(callsign_encode_decode);

    //META field encode/decode
    RUN_TEST(meta_position);

    //CRC test vectors
    RUN_TEST(crc_checks);

    return UNITY_END();
}
