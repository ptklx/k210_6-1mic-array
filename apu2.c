// file apu2.c
/* Copyright 2018 Canaan Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <printf.h>
#include "apu.h"
#include "apu2.h"
#include "syscalls.h"
#include "sysctl.h"
 
#include <apu.h>
#include "apu2.h"

static void print_fir2(const char *member_name, volatile apu_fir_coef_t *pfir)
{
    pprintf("  for(int i = 0; i < 9; i++){\n");
    for(int i = 0; i < 9; i++)
    {
//        apu_fir_coef_t fir = pfir[i];
        apu_fir_coef_t fir;
        fir = pfir[i];

    pprintf("    apu->%s[%d] = (apu_fir_coef_t){\n", member_name, i);
    pprintf("      .fir_tap0 = 0x%x,\n", fir.fir_tap0);
    pprintf("      .fir_tap1 = 0x%x\n", fir.fir_tap1);
    pprintf("    };\n");
    }
    pprintf("  }\n");
}
void apu_print_setting2(void)
{
    pprintf("void apu_setting(void) {\n");
    apu_ch_cfg_t bf_ch_cfg_reg = apu->bf_ch_cfg_reg;

    pprintf("  apu->bf_ch_cfg_reg = (apu_ch_cfg_t){\n");
    pprintf("    .we_audio_gain = 1, .we_bf_target_dir = 1, .we_bf_sound_ch_en = 1,\n");
    pprintf("    .audio_gain = 0x%x, .bf_target_dir = %d, .bf_sound_ch_en = %d, .data_src_mode = %d\n",
           bf_ch_cfg_reg.audio_gain, bf_ch_cfg_reg.bf_target_dir, bf_ch_cfg_reg.bf_sound_ch_en, bf_ch_cfg_reg.data_src_mode);
    pprintf("  };\n");

    apu_ctl_t bf_ctl_reg = apu->bf_ctl_reg;

    pprintf("  apu->bf_ctl_reg = (apu_ctl_t){\n");
    pprintf("    .we_bf_stream_gen = 1, .we_bf_dir_search_en = 1,\n");
    pprintf("    .bf_stream_gen_en = %d, .bf_dir_search_en = %d\n",
           bf_ctl_reg.bf_stream_gen_en, bf_ctl_reg.bf_dir_search_en);
    pprintf("  };\n");

    pprintf("  for(int i = 0; i < 16; i++){\n");
    for(int i = 0; i < 16; i++)
    {
        apu_dir_bidx_t bidx0 = apu->bf_dir_bidx[i][0];
        apu_dir_bidx_t bidx1 = apu->bf_dir_bidx[i][1];

        pprintf("    apu->bf_dir_bidx[%d][0] = (apu_dir_bidx_t){\n", i);
        pprintf("      .dir_rd_idx0 = 0x%x,\n", bidx0.dir_rd_idx0);
        pprintf("      .dir_rd_idx1 = 0x%x,\n", bidx0.dir_rd_idx1);
        pprintf("      .dir_rd_idx2 = 0x%x,\n", bidx0.dir_rd_idx2);
        pprintf("      .dir_rd_idx3 = 0x%x\n", bidx0.dir_rd_idx3);
        pprintf("    };\n");
        pprintf("    apu->bf_dir_bidx[%d][1] = (apu_dir_bidx_t){\n", i);
        pprintf("      .dir_rd_idx0 = 0x%x,\n", bidx1.dir_rd_idx0);
        pprintf("      .dir_rd_idx1 = 0x%x,\n", bidx1.dir_rd_idx1);
        pprintf("      .dir_rd_idx2 = 0x%x,\n", bidx1.dir_rd_idx2);
        pprintf("      .dir_rd_idx3 = 0x%x\n", bidx1.dir_rd_idx3);
        pprintf("    };\n");
    }
    pprintf("  }\n");

    print_fir2("bf_pre_fir0_coef", apu->bf_pre_fir0_coef);
    print_fir2("bf_post_fir0_coef", apu->bf_post_fir0_coef);
    print_fir2("bf_pre_fir1_coef", apu->bf_pre_fir1_coef);
    print_fir2("bf_post_fir1_coef", apu->bf_post_fir1_coef);

    apu_dwsz_cfg_t bf_dwsz_cfg_reg = apu->bf_dwsz_cfg_reg;

    pprintf("  apu->bf_dwsz_cfg_reg = (apu_dwsz_cfg_t){\n");
    pprintf("    .dir_dwn_siz_rate = %d, .voc_dwn_siz_rate = %d\n",
           bf_dwsz_cfg_reg.dir_dwn_siz_rate, bf_dwsz_cfg_reg.voc_dwn_siz_rate);
    pprintf("  };\n");

    apu_fft_cfg_t bf_fft_cfg_reg = apu->bf_fft_cfg_reg;

    pprintf("  apu->bf_fft_cfg_reg = (apu_fft_cfg_t){\n");
    pprintf("    .fft_enable = %d, .fft_shift_factor = 0x%x\n",
           bf_fft_cfg_reg.fft_enable, bf_fft_cfg_reg.fft_shift_factor);
    pprintf("  };\n");

    apu_int_mask_t bf_int_mask_reg = apu->bf_int_mask_reg;

    pprintf("  apu->bf_int_mask_reg = (apu_int_mask_t){\n");
    pprintf("    .dir_data_rdy_msk = %d, .voc_buf_rdy_msk = %d\n",
           bf_int_mask_reg.dir_data_rdy_msk, bf_int_mask_reg.voc_buf_rdy_msk);
    pprintf("  };\n");

    pprintf("}\n");
}
/*
 * radius mic_num_a_circle: the num of mic per circle; center: 0: no center mic, 1:have center mic
 * center==1: center mic is channel after circle mics; (is channel number given by variable mic_num_a_circle) 
 * center>1: center mic is channel given by variable center (== channel of last mic used)
 * e.g.: circle 0,1,2...5 and center is 7
 */
void apu_set_delay2(float radius, uint8_t mic_num_a_circle, uint8_t center)
{
    uint8_t offsets[16][8];
    int i, j;
    float seta[8], delay[8], hudu_jiao;
    float cm_tick = (float)SOUND_SPEED * 100 / I2S_FS; /*distance per tick (cm)*/
    float min;
    
    if(center == 1) // if center>0, then make sure that center variable is set to channel of last mic used
  center = mic_num_a_circle;

    for(i = 0; i < mic_num_a_circle; ++i)
    {
        seta[i] = 360 * i / mic_num_a_circle;
        hudu_jiao = 2 * M_PI * seta[i] / 360;
        delay[i] = radius * (1 - cos(hudu_jiao)) / cm_tick;
    }
    if(center)
//        delay[mic_num_a_circle] = radius / cm_tick;
        delay[center] = radius / cm_tick;

//    for(i = 0; i < mic_num_a_circle + center; ++i)
    for(i = 0; i < (center==0 ? mic_num_a_circle : center+1); ++i)
    {
        offsets[0][i] = (int)(delay[i] + 0.5);
    }
    for(; i < 8; i++)
        offsets[0][i] = 0;

    for(j = 1; j < DIRECTION_RES; ++j)
    {
        for(i = 0; i < mic_num_a_circle; ++i)
        {
            seta[i] -= 360 / DIRECTION_RES;
            hudu_jiao = 2 * M_PI * seta[i] / 360;
            delay[i] = radius * (1 - cos(hudu_jiao)) / cm_tick;
        }
        if(center)
//            delay[mic_num_a_circle] = radius / cm_tick;
            delay[center] = radius / cm_tick;

        min = 2 * radius;
        for(i = 0; i < mic_num_a_circle; ++i)
        {
            if(delay[i] < min)
                min = delay[i];
        }
        if(min)
        {
//            for(i = 0; i < mic_num_a_circle + center; ++i)
            for(i = 0; i < (center==0 ? mic_num_a_circle : center+1); ++i)
            {
                delay[i] = delay[i] - min;
            }
        }

//        for(i = 0; i < mic_num_a_circle + center; ++i)
        for(i = 0; i < (center==0 ? mic_num_a_circle : center+1); ++i)
        {
            offsets[j][i] = (int)(delay[i] + 0.5);
        }
        for(; i < 8; i++)
            offsets[0][i] = 0;
    }
    for(size_t i = 0; i < DIRECTION_RES; i++)
    {
        apu_set_direction_delay(i, offsets[i]);
    }
}