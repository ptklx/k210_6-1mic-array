// file mic_arrays_led.h
// send the pixel data to the mic array using SPI
void write_pixels();

void init_mic_array_lights(void);

void set_light(int light_number, int R, int G, int B);