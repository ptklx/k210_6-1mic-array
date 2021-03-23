#include "init.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <printf.h>
#include "apu.h"
#include "sipeed_sk9822.h"

int count;
int assert_state;

int set_led( const int *pos_args )
{
    int index, brightness[12] = {0,4,0,0,4,5,5,0,8,9,10,11}, led_color[12] = {0}, color[3] = {0,0,255};

    for(index= 0; index < 12; index++)
        brightness[index] = pos_args[index];
    // for(index = 0; index < 3; index++)
    //     color[index] = pos_args[12+index];

    //rgb
    uint32_t set_color = (color[2] << 16) | (color[1] << 8) | (color[0]);

    for (index = 0; index < 12; index++)
    {
        led_color[index] = (brightness[index] / 2) > 1 ? (((0xe0 | (brightness[index] * 2)) << 24) | set_color) : 0xe0000000;
    }

	//FIXME close irq?
	sysctl_disable_irq();
    sk9822_start_frame();
    for (index = 0; index < 12; index++)
    {
        sk9822_send_data(led_color[index]);
    }
    sk9822_stop_frame();

	sysctl_enable_irq();

    return 0;
}


int32_t dir_max_prev = 0;
uint16_t contex_prev = APU_DIR_CHANNEL_MAX;
uint16_t contex_prev_prev = APU_DIR_CHANNEL_MAX;

int dir_logic(void)
{
	int32_t dir_sum = 0;
	int32_t dir_max = 0;
	uint16_t contex = 0;

	for (size_t ch = 0; ch < APU_DIR_CHANNEL_MAX; ch++) { //

			for (size_t i = 0; i < APU_DIR_CHANNEL_SIZE; i++) { //
				 dir_sum += (int32_t)APU_DIR_BUFFER[ch][i] * (int32_t)APU_DIR_BUFFER[ch][i];
			}
			dir_sum = dir_sum / APU_DIR_CHANNEL_SIZE;
			if(dir_sum > dir_max){
				dir_max = dir_sum;
				contex = ch;
			}
			printf("%d	", dir_sum);
		}
		printf("   %d\n", contex);

	if(contex == contex_prev && contex_prev == contex_prev_prev) { // only use dir channel "contex" if three consecutive times unchanged
		int dirc[15] ={0};
		dirc[(int)((12*contex)/16.)]= dir_max/APU_DIR_CHANNEL_SIZE;
		set_led( dirc );
	// apu_voc_set_direction(contex); 
	}


	apu_dir_enable();

	dir_max_prev = dir_max;
	contex_prev_prev = contex_prev;
	contex_prev = contex;
	return 0;
}

int voc_logic(void)
{

	return 0;
}

int event_loop(void)
{
	while (1) {
		if (dir_logic_count > 0) {
			dir_logic();
			while (--dir_logic_count != 0) {
				printk("[warning]: %s, restart before prev callback has end\n",
				       "dir_logic");
			}
		}
		if (voc_logic_count > 0) {
			voc_logic();
			while (--voc_logic_count != 0) {
				printk("[warning]: %s, restart before prev callback has end\n",
				       "voc_logic");
			}
		}
	}
	return 0;
}

int main(void)
{
	sysctl_pll_set_freq(SYSCTL_PLL2, 45158400UL);
	printk("git id: %u\n", sysctl->git_id.git_id);
	printk("init start.\n");
	clear_csr(mie, MIP_MEIP);
	init_all();
	sipeed_init_mic_array_led();
	printk("init done.\n");
	set_csr(mie, MIP_MEIP);
	set_csr(mstatus, MSTATUS_MIE);

	event_loop();

}
