// file apu2.h
#ifdef ARDUINO
void pprintf(char *fmt, ... );
#endif
#ifndef ARDUINO
   #define pprintf(...) printf(__VA_ARGS__)
#endif

void apu_print_setting2(void);

// default: #define I2S_FS 44100
//#undef I2S_FS
//#define I2S_FS 88200

/**
 * @brief       I2S host beam-forming direction sample ibuffer read index configure register
 *
 * @param[in]   radius               radius
 * @param[in]   mic_num_a_circle     the num of mic per circle
 * @param[in]   center               0: no center mic, 1:have center mic (>1 set channel of center mic; default: last channel after circle mics)
 *
 */
void apu_set_delay2(float radius, uint8_t mic_num_a_circle, uint8_t center);