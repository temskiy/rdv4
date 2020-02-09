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

#define MAX_IND 16 // 4 LEDs - 2^4 binary
#define CLOCK 64 //for 125kHz

// low & high - array for storage IDs. Its length must be equal. If you want 
// to set predefined IDs set its in low. 
uint64_t low[] = {0x565AF781C7,0x540053E4E2,0,0};
uint32_t high[] = {0,0,0,0};
uint8_t *bba,slots_count;
int buflen;


void ModInfo(void) {
    DbpString("  LF EM4100 read/emulate standalone V1");
}

uint64_t ReversQuads(uint64_t bits){
  uint64_t result = 0;
  for (int i = 0; i < 16; i++){
    result += ((bits >> (60 - 4 *i)) & 0xf) << (4 * i);
      }
  return result >> 24;
}

void FillBuff(uint8_t bit) {
    memset (bba + buflen, bit, CLOCK / 2);
	buflen += (CLOCK / 2);
	memset (bba + buflen, bit^1,CLOCK / 2);
	buflen += (CLOCK / 2);
}
void ConstructEM410xEmulBuf(uint64_t id) {
	
    int i, j, binary[4], parity[4];
 	buflen = 0;
    for (i = 0; i < 9; i++)
        FillBuff(1);
    parity[0] = parity[1] = parity[2] = parity[3] = 0;
    for (i = 0; i < 10; i++) {
        for (j = 3; j >= 0; j--, id /= 2)
            binary[j] = id % 2;
        for (j = 0; j < 4; j++)
			FillBuff(binary[j]);
        FillBuff(binary[0] ^ binary[1] ^ binary[2] ^ binary[3]);
        for (j = 0; j < 4; j++)
			parity[j] ^= binary[j];
    }
	for (j = 0; j < 4; j++)
    	FillBuff(parity[j]);
    FillBuff(0);
}

void LED_Slot(int i) {
	if (slots_count > 4) {
		LED(i % MAX_IND, 0); //binary indication, usefully for slots > 4
	} else {
		LED(2^i,0); //simple indication for slots <=4
	}
}

void RunMod() {
    StandAloneMode();
    FpgaDownloadAndGo(FPGA_BITSTREAM_LF);
	int selected = 0;
	uint8_t state = 0; //0 - select, 1 - read, 2 - sim
	slots_count = sizeof(low)/sizeof(low[0]);
	bba = BigBuf_get_addr();
	LED_Slot(selected);
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
					LED_Slot(selected);
					Dbprintf("[=] selected slot %x", selected);
				} 
			break;
			case 1:
				CmdEM410xdemod(1, &high[selected], &low[selected], 0);
                Dbprintf("[+] recorded to slot %x", selected);
				FlashLEDs(100,5);
				DbpString("[+] select mode");
				state = 0;
			break;
			case 2:
				if (button_pressed > 0) {
					SpinDown(100);
					SpinOff(100);
					LED_Slot(selected);
					Dbprintf("[<] read to slot %x", selected);
					state = 1;
				} else if (button_pressed < 0) {
					LED_Slot(selected);
					Dbprintf("[>] prepare for sim from slot %x", selected);
					ConstructEM410xEmulBuf(ReversQuads(low[selected]));
					Dbprintf("[>] buffer generated from slot %x", selected);
					FlashLEDs(100,5);
					Dbprintf("[>] simulate from slot %x", selected);
					SimulateTagLowFrequency(buflen, 0, 1);
				}
			break;
		}
	}
}
