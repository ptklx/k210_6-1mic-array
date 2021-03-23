// file init.c
#include "init.h"
#include <stddef.h>
#include <stdio.h>
#include "gpio.h"
#include "apu.h"
#include "apu2.h"
#include "mic_array_led.h"
//#define APU_DMA_ENABLE 1

uint64_t dir_logic_count;
uint64_t voc_logic_count;

#if APU_FFT_ENABLE
uint32_t APU_DIR_FFT_BUFFER[APU_DIR_CHANNEL_MAX]
        [APU_DIR_CHANNEL_SIZE]
  __attribute__((aligned(128)));
uint32_t APU_VOC_FFT_BUFFER[APU_VOC_CHANNEL_SIZE]
  __attribute__((aligned(128)));
#else
int16_t APU_DIR_BUFFER[APU_DIR_CHANNEL_MAX][APU_DIR_CHANNEL_SIZE]
  __attribute__((aligned(128)));
int16_t APU_VOC_BUFFER[APU_VOC_CHANNEL_SIZE]
  __attribute__((aligned(128)));
#endif


int int_apu(void *ctx)
{
  apu_int_stat_t rdy_reg = apu->bf_int_stat_reg;

  if (rdy_reg.dir_search_data_rdy) {
    apu_dir_clear_int_state();

#if APU_FFT_ENABLE
    static int ch;

    ch = (ch + 1) % 16;
    for (uint32_t i = 0; i < 512; i++) { //
      uint32_t data = apu->sobuf_dma_rdata;

      APU_DIR_FFT_BUFFER[ch][i] = data;
    }
    if (ch == 0) { //
      dir_logic_count++;
    }
#else

    for (uint32_t ch = 0; ch < APU_DIR_CHANNEL_MAX; ch++) {
      for (uint32_t i = 0; i < 256; i++) { //
        uint32_t data = apu->sobuf_dma_rdata;

        APU_DIR_BUFFER[ch][i * 2 + 0] =
          data & 0xffff;
        APU_DIR_BUFFER[ch][i * 2 + 1] =
          (data >> 16) & 0xffff;
      }
    }

    dir_logic_count++;
#endif

  } else if (rdy_reg.voc_buf_data_rdy) {
    apu_voc_clear_int_state();

#if APU_FFT_ENABLE
    for (uint32_t i = 0; i < 512; i++) { //
      uint32_t data = apu->vobuf_dma_rdata;

      APU_VOC_FFT_BUFFER[i] = data;
    }
#else
    /*
    for (uint32_t i = 0; i < 256; i++) { //
      uint32_t data = apu->vobuf_dma_rdata;

      APU_VOC_BUFFER[i * 2 + 0] = data & 0xffff; // right
      APU_VOC_BUFFER[i * 2 + 1] = (data >> 16) & 0xffff;  // left
    }
    */
#if APU_DMA_ENABLE

#else
    // use dma to fetch voc data from apu hardware and write it into buffer
    /*int16_t buf_zero[]={ // used to verify sample rate: last 56 of 256 samples set to zero
      0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0,0,
    };*/
    i2s_data_t data = (i2s_data_t) {
        .rx_channel = APU_VOC_DMA_CHANNEL,
        .rx_buf = (uint32_t *)APU_VOC_BUFFER,
        .rx_len = 256,
        .transfer_mode = I2S_RECEIVE
    };
    // i2s_handle_data_dma(I2S_DEVICE_APU_VOC, data, NULL); // not defined for APU_VOC, so copy code from sdk lib/drivers/i2s.c
//    dmac_wait_idle(data.rx_channel); // gets called inside dmac_set_single mode after channel reset (lib/drivers/dmac.c)
    sysctl_dma_select((sysctl_dma_channel_t)data.rx_channel, SYSCTL_DMA_SELECT_I2S0_BF_VOICE_REQ);
    dmac_set_single_mode(data.rx_channel, (void *)(&apu->vobuf_dma_rdata), data.rx_buf, DMAC_ADDR_NOCHANGE, DMAC_ADDR_INCREMENT,
                                 DMAC_MSIZE_1, DMAC_TRANS_WIDTH_32, data.rx_len);
    dmac_wait_done(data.rx_channel);
    
    // use dma to write buffer data to i2s output channel                 
    i2s_set_dma_divide_16(I2S_DEVICE_2, 1);
      i2s_send_data_dma(I2S_DEVICE_2, APU_VOC_BUFFER, 256, DMAC_CHANNEL0);
      //i2s_send_data_dma(I2S_DEVICE_2, APU_VOC_BUFFER, 200, DMAC_CHANNEL0); // used to verify sample rate
      //i2s_send_data_dma(I2S_DEVICE_2, buf_zero, 56, DMAC_CHANNEL0); // used to verify sample rate
    
#endif
#endif
    voc_logic_count++;

  } else { //
    pprintf("[waring]: unknown %s interrupt cause.\n", __func__);
  }
  return 0;
}

#if APU_DMA_ENABLE
static void dmac_chanel_interrupt_clear(dmac_channel_number_t channel_num)
{
    writeq(0xffffffff, &dmac->channel[channel_num].intclear);
}


int int_apu_dir_dma(void *ctx)
{
  uint64_t chx_intstatus =
    dmac->channel[APU_DIR_DMA_CHANNEL].intstatus;
  if (chx_intstatus & 0x02) {
    dmac_chanel_interrupt_clear(APU_DIR_DMA_CHANNEL);

#if APU_FFT_ENABLE
    static int ch;

    ch = (ch + 1) % 16;
    dmac->channel[APU_DIR_DMA_CHANNEL].dar =
      (uint64_t)APU_DIR_FFT_BUFFER[ch];
#else
    dmac->channel[APU_DIR_DMA_CHANNEL].dar =
      (uint64_t)APU_DIR_BUFFER;
#endif

    dmac->chen = 0x0101 << APU_DIR_DMA_CHANNEL;

#if APU_FFT_ENABLE
    if (ch == 0) { //
      dir_logic_count++;
    }
#else
    dir_logic_count++;
#endif

  } else {
    pprintf("[warning] unknown dma interrupt. %lx %lx\n",
           dmac->intstatus, dmac->com_intstatus);
    pprintf("dir intstatus: %lx\n", chx_intstatus);

    dmac_chanel_interrupt_clear(APU_DIR_DMA_CHANNEL);
  }
  return 0;
}


int int_apu_voc_dma(void *ctx)
{
  uint64_t chx_intstatus =
    dmac->channel[APU_VOC_DMA_CHANNEL].intstatus;

  if (chx_intstatus & 0x02) {
    dmac_chanel_interrupt_clear(APU_VOC_DMA_CHANNEL);

#if APU_FFT_ENABLE
    dmac->channel[APU_VOC_DMA_CHANNEL].dar =
      (uint64_t)APU_VOC_FFT_BUFFER;
#else
    dmac->channel[APU_VOC_DMA_CHANNEL].dar =
      (uint64_t)APU_VOC_BUFFER;
#endif

    dmac->chen = 0x0101 << APU_VOC_DMA_CHANNEL;


    voc_logic_count++;

  } else {
    pprintf("[warning] unknown dma interrupt. %lx %lx\n",
           dmac->intstatus, dmac->com_intstatus);
    pprintf("voc intstatus: %lx\n", chx_intstatus);

    dmac_chanel_interrupt_clear(APU_VOC_DMA_CHANNEL);
  }
  return 0;
}
#endif

void init_fpioa(void)
{
  pprintf("init fpioa.\n");
  fpioa_init();
  // mic
//  fpioa_set_function(47, FUNC_GPIOHS4);
  fpioa_set_function(23, FUNC_I2S0_IN_D0);
  fpioa_set_function(22, FUNC_I2S0_IN_D1);
  fpioa_set_function(21, FUNC_I2S0_IN_D2);
  fpioa_set_function(20, FUNC_I2S0_IN_D3);
  fpioa_set_function(19, FUNC_I2S0_WS);
  fpioa_set_function(18, FUNC_I2S0_SCLK);
  // dac
  fpioa_set_function(24, FUNC_I2S2_OUT_D1);
  fpioa_set_function(25, FUNC_I2S2_SCLK);
  fpioa_set_function(33, FUNC_I2S2_WS);


}

// copied from lib/drivers/i2s.c
#include <math.h>
uint32_t i2s_set_sample_rate2(i2s_device_number_t device_num, uint32_t sample_rate)
{
    ccr_t u_ccr;
    uint32_t pll2_clock = 0;
    pll2_clock = sysctl_pll_get_freq(SYSCTL_PLL2);

    u_ccr.reg_data = readl(&i2s[device_num]->ccr);
    /* 0x0 for 16sclk cycles, 0x1 for 24 sclk cycles 0x2 for 32 sclk */
    uint32_t v_clk_word_size = (u_ccr.ccr.clk_word_size + 2) * 8;
    uint32_t threshold = round(pll2_clock / (sample_rate * 2.0 * v_clk_word_size * 2.0) - 1.5);
    sysctl_clock_set_threshold(SYSCTL_THRESHOLD_I2S0 + device_num, threshold);
    return sysctl_clock_get_freq(SYSCTL_CLOCK_I2S0 + device_num);
}

void init_i2s(void)
{
  // hardware limitation? 
  // i2s0 clock frequencies of 8.4MHz (4 stereo channels, sample 44.1kHz, 24bit) seems to induce noise of high frequency
  // whereas i2s0 clocked with 5.6MHz (4 steroe channels, sample 44.1kHz, 16bit) does not show this noise
  // conclusion: 
  // either use lower sample rates and get higher microphone sensitivity (using 24bit and apu_set_smpl_shift(n) with n=8,7,6...)
  // or use 44.1kHz and accept lower sensitivity (max value of gain is 1.9 apu_set_audio_gain(0x7ff))
  // or accept noise ...
  // (lower input sample rates probably degrade beamforming because beamforming uses time delays
  // and probably apu logic internally uses delays which are multiples of sample ticks: delta t = 1/sample frequency)
  //
  // do not use i2s_set_sample_rate because of rounding errors!!!
  // (which make it impossible to set input and output sample rates precisely (they have to match exactly))
  // instead directly set threshold of pll2 for i2s (clock of i2sX is 2*(threshold + 1), as used in lib/drivers/i2s.c)
  // to adjust the sample rate in small steps change the base frequency of pll2, e.g. sysctl_pll_set_freq(SYSCTL_PLL2, 67737602UL)
  //
  // downsampling apu_voc output can be used as lowpass for voice output
  // apu_set_down_size(0, 3); // dir, voc: 0: /1, 1: /2, 2: /3, 3: /4 ... 15: /16 (first argument is down sampling of apu_dir)
  // (i2s sample rate for output , that is threshold used for i2s output has to be set accordingly)

  uint32_t real_clock, real_clock_source, pll2_threshold;
  pprintf("init i2s.\n");

  /* I2s init */
  // SCLK_CYCLES refers to whole device and has to be >= biggest channel resolution (of channels of this device)
    i2s_init(I2S_DEVICE_0, I2S_RECEIVER, 0x3);  // bits 0x3 set: use interrupts - else
    i2s_init(I2S_DEVICE_2, I2S_TRANSMITTER, 0x3); // 0x3
    // 
  // either RESOLUTION 16 and apu sample shift 0, or res 24 and apu sample shift 8 (or 7, 6, 5 ... to increase sensitivity)
  // (sys cycles has to be >= resolution)
    i2s_rx_channel_config(I2S_DEVICE_0, I2S_CHANNEL_0,
            RESOLUTION_24_BIT, SCLK_CYCLES_24,
            TRIGGER_LEVEL_4, STANDARD_MODE); // has to be standard mode
    i2s_rx_channel_config(I2S_DEVICE_0, I2S_CHANNEL_1,
            RESOLUTION_24_BIT, SCLK_CYCLES_24,
            TRIGGER_LEVEL_4, STANDARD_MODE);
    i2s_rx_channel_config(I2S_DEVICE_0, I2S_CHANNEL_2,
            RESOLUTION_24_BIT, SCLK_CYCLES_24,
            TRIGGER_LEVEL_4, STANDARD_MODE);
    i2s_rx_channel_config(I2S_DEVICE_0, I2S_CHANNEL_3,
            RESOLUTION_24_BIT, SCLK_CYCLES_24,
            TRIGGER_LEVEL_4, STANDARD_MODE);
// input uses 4 stereo channels, output uses 1 stereo channel, so clock of input must be 4 times higher than output clock
// (apart from rounding errors produced by i2s_set_sample_rate(...) the rate argument has to be multiplied by the number of channels used)
//    real_clock = 2*i2s_set_sample_rate(I2S_DEVICE_0, 176400); // seems to produce lots of noise unless SCLK_CYCLES is reduced to 16
  sysctl_clock_set_threshold(SYSCTL_THRESHOLD_I2S0 + I2S_DEVICE_0, 5); // pll2 divisor is (threshold + 1)
  // sample rate == (pll2_freq/pll2_divisor)/(32*4) in case of 16 sys cycles (for left and right channel; using 4 stereo channels)
    real_clock = 2*sysctl_clock_get_freq(SYSCTL_CLOCK_I2S0); //clock serves 2 channels, sys_clock_get_freq gets bit rate per channel not i2s clock
    real_clock_source = 2*sysctl_clock_get_freq(SYSCTL_CLOCK_I2S0)*(1+sysctl_clock_get_threshold(SYSCTL_THRESHOLD_I2S0));
  pll2_threshold = sysctl_clock_get_threshold(SYSCTL_THRESHOLD_I2S0);
    pprintf("I2S DEV 0 real clock freq: %u (source %u, threshold %u, divisor %u)\n", real_clock, real_clock_source, pll2_threshold, 1+pll2_threshold);

    i2s_tx_channel_config(I2S_DEVICE_2, I2S_CHANNEL_1,
                          RESOLUTION_16_BIT, SCLK_CYCLES_24, // 24 or 32 sys cycles works as well, if i2s clock is incremented accordingly
                          TRIGGER_LEVEL_4,
                          RIGHT_JUSTIFYING_MODE); // needed, PT8211 dual 16bit dac
//    real_clock = 2*i2s_set_sample_rate2(I2S_DEVICE_2, 44000);
//    sysctl_clock_set_threshold(SYSCTL_THRESHOLD_I2S0 + I2S_DEVICE_2, 23); // pll2 divisor is 2*(threshold + 1); no downsampling: broken apu_voc output
  sysctl_clock_set_threshold(SYSCTL_THRESHOLD_I2S0 + I2S_DEVICE_2, 47); // pll2 divisor is (threshold + 1); downsampling by factor 2
//    sysctl_clock_set_threshold(SYSCTL_THRESHOLD_I2S0 + I2S_DEVICE_2, 71); // pll2 divisor is (threshold + 1); downsampling by factor 3
//    sysctl_clock_set_threshold(SYSCTL_THRESHOLD_I2S0 + I2S_DEVICE_2, 95); // pll2 divisor is (threshold + 1); downsampling by factor 4
  // sample rate == (pll2_freq/pll2_divisor)/32 in case of 16 sys cycles (for left and right channel; using 1 stereo channels)
    real_clock = 2*sysctl_clock_get_freq(SYSCTL_CLOCK_I2S2); //clock serves 2 channels, sys_clock_get_freq gets bit rate per channel not i2s clock
    real_clock_source = 2*sysctl_clock_get_freq(SYSCTL_CLOCK_I2S2)*(1+sysctl_clock_get_threshold(SYSCTL_THRESHOLD_I2S2));
  pll2_threshold = sysctl_clock_get_threshold(SYSCTL_THRESHOLD_I2S2);
    pprintf("I2S DEV 2 real clock freq: %u (source %u, threshold %u, divisor %u)\n", real_clock, real_clock_source, pll2_threshold, 1+pll2_threshold);

    // power on audio amplifier
    fpioa_set_function(32, FUNC_GPIO0); 
    gpio_init();
    gpio_set_drive_mode(0, GPIO_DM_OUTPUT);
    gpio_set_pin(0, GPIO_PV_HIGH);
}

void init_bf(void)
{
  pprintf("init bf.\n");
  uint16_t fir_prev_t[] = {
    0x020b, 0x0401, 0xff60, 0xfae2, 0xf860, 0x0022,
    0x10e6, 0x22f1, 0x2a98, 0x22f1, 0x10e6, 0x0022,
    0xf860, 0xfae2, 0xff60, 0x0401, 0x020b,
  };
  uint16_t fir_post_t[] = {
    0xf649, 0xe59e, 0xd156, 0xc615, 0xd12c, 0xf732,
    0x2daf, 0x5e03, 0x7151, 0x5e03, 0x2daf, 0xf732,
    0xd12c, 0xc615, 0xd156, 0xe59e, 0xf649,
  };
  uint16_t fir_one[] = { // 32767
    0x7fff, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  };
  uint16_t fir_neg_one[] = { // -32768
    0x8000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  };
  uint16_t fir_half[] = { // 16384
    0x4000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  };
  int16_t fir_lowpass_signed[] = { // http://t-filter.engineerjs.com/ 44100 Hz 17 taps pass 0-5000, stop 10000-22050
      110,
      -93,
      -755,
      -1561,
      -1447,
      641,
      4552,
      8508,
      10184,
      8508,
      4552,
      641,
      -1447,
      -1561,
      -755,
      -93,
      110
    };
  int16_t fir_lowpass2_signed[] = { // http://t-filter.engineerjs.com/ 44100 Hz 17 taps pass 0-2500, stop 5000-22050
        62,
         698,
         1151,
         1911,
         2762,
         3617,
         4351,
         4847,
         5022,
         4847,
         4351,
         3617,
         2762,
         1911,
         1151,
         698,
         62
    };
  int16_t fir_bandpass_signed[] = {
      /*

      FIR filter designed with
      http://t-filter.appspot.com

      sampling frequency: 22050 Hz

      fixed point precision: 16 bits

      * 0 Hz - 12 Hz
        gain = 0
        desired attenuation = -20 dB
        actual attenuation = n/a

      * 18 Hz - 1000 Hz
        gain = 1
        desired ripple = 5 dB
        actual ripple = n/a

      * 2000 Hz - 11025 Hz
        gain = 0
        desired attenuation = -20 dB
        actual attenuation = n/a
      */
        -5173,
        -466,
        -109,
        457,
        1170,
        1907,
        2545,
        2979,
        3134,
        2979,
        2545,
        1907,
        1170,
        457,
        -109,
        -466,
        -5173
      };
//  apu_dir_set_prev_fir(fir_one);
//  apu_dir_set_post_fir(fir_one);
//  apu_voc_set_prev_fir(fir_neg_one);
//  apu_voc_set_post_fir(fir_neg_one);
  apu_voc_set_prev_fir((uint16_t *)fir_lowpass2_signed);
  apu_voc_set_post_fir((uint16_t *)fir_bandpass_signed);
//      apu_voc_set_prev_fir((uint16_t *)fir_prev_t);
//      apu_voc_set_post_fir((uint16_t *)fir_post_t);
//      apu_voc_set_prev_fir((uint16_t *)fir_one);
//      apu_voc_set_post_fir((uint16_t *)fir_one);
  apu_dir_set_prev_fir((uint16_t *)fir_lowpass_signed);
  apu_dir_set_post_fir((uint16_t *)fir_lowpass_signed);

  // lib/drivers/include/apu.h has hardcoded I2S rate of 44100: #define I2S_FS 44100; 
  // lib/drivers/apu.c: float cm_tick = (float)SOUND_SPEED * 100 / I2S_FS; /*distance per tick (cm)*/
  // redefine I2S_FS in apu2.h using #undef I2S_FS and #define I2S_FS 22050
  // apu2.h and apu2.c define apu_set_delay2 which allows a center mic on an arbitrary i2s channel (required for maix go)
//  apu_set_delay2(4, 6, 0); // radius of mic circle (cm), # mics in circle, no center mic (0/1)
//  apu_set_delay2(4, 6, 1); // radius of mic circle (cm), # mics in circle, with center mic (0/1)
  apu_set_delay2(4, 6, 7); // radius of mic circle (cm), # mics in circle 0,1...5, with center mic as mic number 7
//  apu_set_smpl_shift(APU_SMPL_SHIFT); // 0 corresponds to i2s input 16bit
//  apu_set_smpl_shift(8); // corresponds to i2s input 24bit instead of 16 bit: using 7,6... increases sensitivity
  apu_set_smpl_shift(6); // corresponds to i2s input 24bit instead of 16 bit: using 7,6... increases sensitivity: useful with bandpass filter
  apu_voc_set_saturation_limit(APU_SATURATION_VPOS_DEBUG, // 0x07ff
            APU_SATURATION_VNEG_DEBUG); // 0xf800
  apu_set_audio_gain(APU_AUDIO_GAIN_TEST); // 1 << 10 == 1.0
//  apu_set_audio_gain(0x200);  // 0.5
//  apu_set_audio_gain(0x100);  // 0.25
//    apu_set_audio_gain(0x7ff);  // 1.9 max gain
//  apu_set_channel_enabled(0x3f); // circle mics 0,1...5; no center mic
//  apu_set_channel_enabled(0x15); // only circle mics 0,3,5; no center mic
//  apu_set_channel_enabled(0x7f);
  apu_set_channel_enabled(0xbf); // circle mics 0,1...5; center mic 7: requires apu_set_delay2
//  apu_set_down_size(0, 0); // dir, voc: 0: /1, 1: /2, 2: /3 ... 15: /16
  apu_set_down_size(0, 1); // downsampling of apu_voc; output i2s rate for apu_voc has to be divided accordingly;

  // current status: rather high sensity; no sampling distortions (scope: broken curves with missing pieces); 440hz beamforming seems to work: 100% in preferred direction, 50% about 30 degrees away; 440hz sine is accompagnied by 30hz(??) humming sound, 440hz sine waves are wobbling, like frequency modulated

#if APU_FFT_ENABLE
  apu_set_fft_shift_factor(1, 0xaa);
#else
  apu_set_fft_shift_factor(0, 0);
#endif

  apu_set_interrupt_mask(APU_DMA_ENABLE, APU_DMA_ENABLE);
#if APU_DIR_ENABLE
  apu_dir_enable();
#endif
#if APU_VOC_ENABLE
  apu_voc_enable(1);
#else
  apu_voc_enable(0);
#endif
}

#if APU_DMA_ENABLE
void init_dma(void)
{
  pprintf("%s\n", __func__);
  // dmac enable dmac and interrupt
//  union dmac_cfg_u dmac_cfg;
  dmac_cfg_u_t dmac_cfg;

  dmac_cfg.data = readq(&dmac->cfg);
  dmac_cfg.cfg.dmac_en = 1;
  dmac_cfg.cfg.int_en = 1;
  writeq(dmac_cfg.data, &dmac->cfg);

  sysctl_dma_select(SYSCTL_DMA_CHANNEL_0 + APU_DIR_DMA_CHANNEL,
        SYSCTL_DMA_SELECT_I2S0_BF_DIR_REQ);
  sysctl_dma_select(SYSCTL_DMA_CHANNEL_0 + APU_VOC_DMA_CHANNEL,
        SYSCTL_DMA_SELECT_I2S0_BF_VOICE_REQ);
}
#endif

void init_dma_ch(int ch, volatile uint32_t *src_reg, void *buffer,
     size_t size_of_byte)
{
  pprintf("%s %d\n", __func__, ch);

  dmac->channel[ch].sar = (uint64_t)src_reg;
  dmac->channel[ch].dar = (uint64_t)buffer;
  dmac->channel[ch].block_ts = (size_of_byte / 4) - 1;
  dmac->channel[ch].ctl =
    (((uint64_t)1 << 47) | ((uint64_t)15 << 48)
     | ((uint64_t)1 << 38) | ((uint64_t)15 << 39)
     | ((uint64_t)3 << 18) | ((uint64_t)3 << 14)
     | ((uint64_t)2 << 11) | ((uint64_t)2 << 8) | ((uint64_t)0 << 6)
     | ((uint64_t)1 << 4) | ((uint64_t)1 << 2) | ((uint64_t)1));
  /*
   * dmac->channel[ch].ctl = ((  wburst_len_en  ) |
   *                        (    wburst_len   ) |
   *                        (  rburst_len_en  ) |
   *                        (    rburst_len   ) |
   *                        (one transaction:d) |
   *                        (one transaction:s) |
   *                        (    dst width    ) |
   *                        (    src width   ) |
   *                        (    dinc,0 inc  )|
   *                        (  sinc:1,no inc ));
   */

  dmac->channel[ch].cfg = (((uint64_t)1 << 49) | ((uint64_t)ch << 44)
         | ((uint64_t)ch << 39) | ((uint64_t)2 << 32));
  /*
   * dmac->channel[ch].cfg = ((     prior       ) |
   *                         (      dst_per    ) |
   *                         (     src_per     )  |
   *           (    peri to mem  ));
   *  01: Reload
   */

  dmac->channel[ch].intstatus_en = 0x2; // 0xFFFFFFFF;
  dmac->channel[ch].intclear = 0xFFFFFFFF;

  dmac->chen = 0x0101 << ch;
}


void init_interrupt(void)
{
  plic_init();
  // bf
  plic_set_priority(IRQN_I2S0_INTERRUPT, 4);
  plic_irq_enable(IRQN_I2S0_INTERRUPT);
  plic_irq_register(IRQN_I2S0_INTERRUPT, int_apu, NULL);

#if APU_DMA_ENABLE
  // dma
  plic_set_priority(IRQN_DMA0_INTERRUPT + APU_DIR_DMA_CHANNEL, 4);
  plic_irq_register(IRQN_DMA0_INTERRUPT + APU_DIR_DMA_CHANNEL,
        int_apu_dir_dma, NULL);
  plic_irq_enable(IRQN_DMA0_INTERRUPT + APU_DIR_DMA_CHANNEL);
  // dma
  plic_set_priority(IRQN_DMA0_INTERRUPT + APU_VOC_DMA_CHANNEL, 4);
  plic_irq_register(IRQN_DMA0_INTERRUPT + APU_VOC_DMA_CHANNEL,
        int_apu_voc_dma, NULL);
  plic_irq_enable(IRQN_DMA0_INTERRUPT + APU_VOC_DMA_CHANNEL);
#endif
}
/*
void init_ws2812b(void)
{
  gpiohs->output_en.bits.b4 = 1;
  gpiohs->output_val.bits.b4 = 0;
}
*/
void init_all(void)
{
  init_fpioa();
//  init_pll();
  init_interrupt();
  init_i2s();
  init_bf();

  if (APU_DMA_ENABLE) {
    #if APU_DMA_ENABLE
    init_dma();
    #endif
#if APU_FFT_ENABLE
    init_dma_ch(APU_DIR_DMA_CHANNEL,
          &apu->sobuf_dma_rdata,
          APU_DIR_FFT_BUFFER[0], 512 * 4);
    init_dma_ch(APU_VOC_DMA_CHANNEL,
          &apu->vobuf_dma_rdata, APU_VOC_FFT_BUFFER,
          512 * 4);
#else
    init_dma_ch(APU_DIR_DMA_CHANNEL,
          &apu->sobuf_dma_rdata, APU_DIR_BUFFER,
          512 * 16 * 2);
    init_dma_ch(APU_VOC_DMA_CHANNEL,
          &apu->vobuf_dma_rdata, APU_VOC_BUFFER,
          512 * 2);
#endif
  }
//  init_ws2812b();
  init_mic_array_lights();
  for (int l=0; l<12; l++) set_light(l, 0, 0, 0);
  write_pixels();
//  apu_print_setting();
}