//-----------------------------------------------------------------------------
// Artyom Gnatyuk, 2020
//
// This code is licensed to you under the terms of the GNU GPL, version 2 or,
// at your option, any later version. See the LICENSE.txt file for the text of
// the license.
//-----------------------------------------------------------------------------
// main code for LF emul V1
//-----------------------------------------------------------------------------
#include "standalone.h" // standalone definitions
#include "proxmark3_arm.h"
#include "appmain.h"
#include "fpgaloader.h"
#include "lfops.h"
#include "util.h"
#include "dbprint.h"
#include "ticks.h"
#include "string.h"
#include "BigBuf.h"

#define MAX_IND 16
#define CLOCK 64

uint64_t low[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
uint32_t high[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
uint8_t *bba,slots_count;
int buflen;

void ModInfo(void) {
    DbpString("  LF EM4100 emulation standalone V1");
}

void FillBuff(uint8_t bit) {
    memset (bba + buflen, bit, CLOCK / 2);
	buflen += (CLOCK / 2);
	memset (bba + buflen, bit^1,CLOCK / 2);
	buflen += (CLOCK / 2);
}
void ConstructEM410xEmulBuf(uint64_t id) {
	Dbprintf("buf: %i", id);
    int i, j, binary[4], parity[4];
 	buflen = 0;
    for (i = 0; i < 9; i++)
        FillBuff(1);
    parity[0] = parity[1] = parity[2] = parity[3] = 0;
    for (i = 0; i < 10; i++) {
        for (j = 3; j >= 0; j--, id /= 2)
            binary[j] = id % 2;
        FillBuff(binary[0]);
        FillBuff(binary[1]);
        FillBuff(binary[2]);
        FillBuff(binary[3]);
        FillBuff(binary[0] ^ binary[1] ^ binary[2] ^ binary[3]);
        parity[0] ^= binary[0];
        parity[1] ^= binary[1];
        parity[2] ^= binary[2];
        parity[3] ^= binary[3];
    }
    FillBuff(parity[0]);
    FillBuff(parity[1]);
    FillBuff(parity[2]);
    FillBuff(parity[3]);
    FillBuff(0);
}

uint64_t ReversBits(uint64_t bits){
	uint64_t result = 0;
	for (int i = 0; i < 64; i++){
		result *=2;
		result += bits % 2;
		bits /= 2;
	}
	return result;
}

void RunMod() {
    StandAloneMode();
    FpgaDownloadAndGo(FPGA_BITSTREAM_LF);
	int selected = 0;
	uint8_t state = 0; //0 - idle, 1 - read, 2 - sim
	slots_count = sizeof(low)/sizeof(low[0]);
	bba = BigBuf_get_addr();
	Dbprintf("BigBuf addr: %i", bba );
	LED(selected, 0);
	DbpString("[+] now in select mode");
	for (;;) {		
		WDT_HIT();
        if (data_available()) break;
		int button_pressed = BUTTON_HELD(1000);
		SpinDelay(300);
		switch (state){
			case 0:
				if (button_pressed == 1) {
					SpinUp(100);
					SpinOff(100);
					DbpString("[+] record/simulate mode");
					state = 2;
				} else if (button_pressed < 0) {
					selected = (selected + 1) % slots_count;
					LEDsoff();
					LED(selected % MAX_IND, 0);
					Dbprintf("[=] selected %x slot", selected);
				} 
			break;
			case 1:
				CmdEM410xdemod(1, &high[selected], &low[selected], 0);
				// sprintf(str, "%02lx%08lx", (uint32_t) (low[selected] >> 32), (uint32_t) low[selected]);
                Dbprintf("[+] recorded to %i slot", selected);
				FlashLEDs(100,5);
				DbpString("[+] select mode");
				BigBuf_Clear();
				state = 0;
			break;
			case 2:
				if (button_pressed > 0) {
					SpinDown(100);
					SpinOff(100);
					LED(selected % MAX_IND, 0);
					Dbprintf("[<] read to %x slot", selected);
					state = 1;
				} else if (button_pressed < 0) {
					LED(selected % MAX_IND, 0);
					Dbprintf("[>] prepare for sim from %x slot", selected);
					ConstructEM410xEmulBuf(ReversBits(low[selected]));
					Dbprintf("[>] buffer generated from %x slot", selected);
					FlashLEDs(100,5);
					Dbprintf("[>] simulate from %x slot", selected);
					SimulateTagLowFrequency(buflen, 0, 1);
				}
			break;
		}
	}
}
