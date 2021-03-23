// file apu6.ino in arduino ide, file main.c in standalone sdk
// use K210 apu to read from 6+1 mic array, auto detect direction, send sound to speaker
// https://blog.spblinux.de/2019/07/sipeed-maix-go-k210-beam-forming/

// switching from kendryte standalone sdk to arduino ide (which is based on this sdk):
// Using arduino ide:
// - stdout not defined by default, thus replace printf and printk by pprintf (defined below)
// - arduino works with c++ compiler, sdk works with c compiler
//   - ino-files (shown without extension in arduino ide) are handled as c++ files
//   - additional files (=tabs in arduino ide) are handled as c files (*.h, *.c) or c++ (*.hpp, *.cpp)
//   - extern "C"{...}; allows to include c files from c++ files
#ifdef __cplusplus
extern "C"{
#endif
#include "init.h"
// only required for standalone sdk
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <printf.h>
// end only ...
#include <apu.h>
#include "apu2.h"
#include "mic_array_led.h"
#ifdef __cplusplus
};
#endif
 

#ifdef ARDUINO
// in case of standalone sdk pprintf is defined as macro in apu2.h: #define pprintf(...) printf(__VA_ARGS__)
void pprintf(char *fmt, ... ){
        char buf[128]; // resulting string limited to 128 chars
        va_list args;
        va_start (args, fmt );
        vsnprintf(buf, 128, fmt, args);
        va_end (args);
        Serial.print(buf);
}
#endif

int count;
int assert_state;
int32_t dir_max_prev = 0;
uint16_t contex_prev = APU_DIR_CHANNEL_MAX;
uint16_t contex_prev_prev = APU_DIR_CHANNEL_MAX;

int dir_logic(void)
{
  int32_t dir_sum_array[APU_DIR_CHANNEL_MAX];
  int32_t dir_sum = 0;
  int32_t dir_sum_all = 0;
  int32_t dir_max = 0;
  uint16_t contex = 0;

  for (size_t ch = 0; ch < APU_DIR_CHANNEL_MAX; ch++) { // 
      dir_sum = 0;
      for (size_t i = 0; i < APU_DIR_CHANNEL_SIZE; i++) {
         dir_sum += (int32_t)APU_DIR_BUFFER[ch][i] * (int32_t)APU_DIR_BUFFER[ch][i];
      }
      dir_sum_array[ch] = dir_sum / APU_DIR_CHANNEL_SIZE; // averaged square sum of one dir channel
  }
  for (size_t ch = 0; ch < APU_DIR_CHANNEL_MAX; ch++) { // dir_sum_all: dir_sum + 50% of dir_sum of both adjacent neighbours
    dir_sum_all = dir_sum_array[ch]
       + (dir_sum_array[(ch+1) % APU_DIR_CHANNEL_MAX]+dir_sum_array[(ch+APU_DIR_CHANNEL_MAX-1) % APU_DIR_CHANNEL_MAX])/2;
    if(dir_sum_all > dir_max){
        dir_max = dir_sum_all;
        contex = ch;
    }
//      pprintf("%d  ", dir_sum);
  }
//    pprintf("   %d\n", contex);
//    pprintf("\n");

  if(contex == contex_prev && contex_prev == contex_prev_prev) { // only use dir channel "contex" if three consecutive times unchanged
    for (int l=0; l<12; l++) {
        if(l==(int)((12*contex)/16.))
        set_light(l, dir_max/APU_DIR_CHANNEL_SIZE, dir_max/APU_DIR_CHANNEL_SIZE, 0);
  //        set_light(l, 0, 0, 1);
        else
        set_light(l, 0, 0, 1);
    }
    write_pixels();
#ifdef ARDUINO
    apu_voc_set_direction((en_bf_dir)contex); // use direction deteced by APU_DIR
#endif
#ifndef ARDUINO
    apu_voc_set_direction(contex); // use direction deteced by APU_DIR
#endif
  }
//  apu_voc_set_direction(6); // use fixed direction for testing
//  set_light((int)((12*6)/16.), 32, 32, 0);
//  write_pixels();


  apu_dir_enable(); // if commented out: direction gets determined (APU_DIR) and set (APU_VOC) only once
  dir_max_prev = dir_max;
  contex_prev_prev = contex_prev;
  contex_prev = contex;
  return 0;
}

int voc_logic(void)
{
  return 0;
}

void setup(void)
{
   // Start the UART
#ifdef ARDUINO
   Serial.begin(115200) ;
   delay(100);
#endif
    //apu_print_setting2();
  uint32_t real_freq, real_freq_source;
  real_freq = sysctl_cpu_get_freq();
  sysctl_pll_set_freq(SYSCTL_PLL0, 320000000UL);
  sysctl_pll_set_freq(SYSCTL_PLL1, 160000000UL);
  uarths_init();
  pprintf("CPU real freq: %u\n", real_freq);
//  real_freq = sysctl_pll_set_freq(SYSCTL_PLL2, 34000000UL); // 44.17kHz sampl., 4 chann. 16bit and threshold 5 (or 1 chan, thresh 47, 22.14kHz)
  real_freq = sysctl_pll_set_freq(SYSCTL_PLL2, 50375000UL); // 43.73kHz sampl., 4 chann. 24bit and threshold 5 (or 1 chan, thresh 47, 21.86kHz)
//  real_freq = sysctl_pll_set_freq(SYSCTL_PLL2, 68000000UL); // 88.54kHz sampl., 4 chann. 16bit and threshold 5 (or 1 chan, thresh 47, 44.27kHz)
  real_freq_source = sysctl_clock_get_freq(SYSCTL_CLOCK_PLL2);
  pprintf("PLL2 real freq: %u (source %u)\n", real_freq, real_freq_source);
  pprintf("git id: %u\n", sysctl->git_id.git_id);
  pprintf("init start.\n");
  clear_csr(mie, MIP_MEIP);
  init_all();
  pprintf("init done.\n");
  set_csr(mie, MIP_MEIP);
  set_csr(mstatus, MSTATUS_MIE);
}

void loop() {
  // put your main code here, to run repeatedly:
  while (1) {
    if (dir_logic_count > 0) {
      dir_logic();
      while (--dir_logic_count != 0) {
        pprintf("d");
//        pprintf("[warning]: %s, restart before prev callback has end\n",
//               "dir_logic");
      }
    }
    if (voc_logic_count > 0) {
      voc_logic();
      while (--voc_logic_count != 0) {
        pprintf("v");
//        pprintf("[warning]: %s, restart before prev callback has end\n",
//               "voc_logic");
      }
    }

  }
}

#ifndef ARDUINO
int main(void)
{
  setup();
  loop();
}
#endif