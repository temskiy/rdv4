//-----------------------------------------------------------------------------
// Samy Kamkar, 2012
// Christian Herrmann, 2017
//
// This code is licensed to you under the terms of the GNU GPL, version 2 or,
// at your option, any later version. See the LICENSE.txt file for the text of
// the license.
//-----------------------------------------------------------------------------
// main code for LF emul
//-----------------------------------------------------------------------------
#include "standalone.h" // standalone definitions
#include "proxmark3_arm.h"
#include "appmain.h"
#include "fpgaloader.h"
#include "lfops.h"
#include "util.h"
#include "dbprint.h"
#include "ticks.h"

#include "BigBuf.h"
#define OPTS 16
#define CLOCK 64
uint64_t low[OPTS];
uint32_t high[OPTS];
char str[11];
int buflen;



void ModInfo(void) {
    DbpString("  LF EM4100 emulation standalone");
}

void FillBuff(int bit) {
    int i;
    //set first half the clock bit (all 1's or 0's for a 0 or 1 bit)
    for (i = 0; i < (int)(CLOCK / 2); ++i) {
        BigBuf[buflen++] = bit ;
		Dbprintf("buf: %i, %i", buflen,BigBuf[buflen-1]);
	}
    //set second half of the clock bit (all 0's or 1's for a 0 or 1 bit)
    for (i = (int)(CLOCK / 2); i < CLOCK; ++i){
        BigBuf[buflen++] = bit ^ 1;
		Dbprintf("buf: %i, %i", buflen,BigBuf[buflen-1]);
	}
	Dbprintf("Fillbuf buflen: %i, %i", buflen, bit);
}

void ConstructEM410xEmulBuf(const char *uid) {
	Dbprintf("ConstructEM410xEmulBuf");
    int i, j, binary[4], parity[4];
    uint32_t n;
    /* clear BigBuf */
    // BigBuf_free();
    // BigBuf_Clear();
    // clear_trace();
    // set_tracing(false);
	buflen = 0;
    /* write 9 start bits */
    for (i = 0; i < 9; i++)
        FillBuff(1);
    /* for each hex char */
    parity[0] = parity[1] = parity[2] = parity[3] = 0;
    for (i = 0; i < 10; i++) {
        /* read each hex char */
        // sscanf(&uid[i], "%1lx", &n);
		n = hex2int(uid[i]);
        for (j = 3; j >= 0; j--, n /= 2)
            binary[j] = n % 2;
        /* append each bit */
        FillBuff(binary[0]);
        FillBuff(binary[1]);
        FillBuff(binary[2]);
        FillBuff(binary[3]);
        /* append parity bit */
        FillBuff(binary[0] ^ binary[1] ^ binary[2] ^ binary[3]);
        /* keep track of column parity */
        parity[0] ^= binary[0];
        parity[1] ^= binary[1];
        parity[2] ^= binary[2];
        parity[3] ^= binary[3];
    }
    /* parity columns */
    FillBuff(parity[0]);
    FillBuff(parity[1]);
    FillBuff(parity[2]);
    FillBuff(parity[3]);
    /* stop bit */
    FillBuff(0);
	// BigBuf_free();
}

void RunMod() {
    StandAloneMode();
    FpgaDownloadAndGo(FPGA_BITSTREAM_LF);
    int selected = 0;
	int state = 0; //0 - idle, 1 - read, 2 - sim
	
	// DbpString("[+] now in ListenReaderField mode");
	// ListenReaderField(1);
	Dbprintf("%i", BigBuf_get_addr());
	LED(selected, 0);
	DbpString("[+] now in select mode");
	for (;;) {		
		WDT_HIT();

        // exit from SamyRun,   send a usbcommand.
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
					selected = (selected + 1) % OPTS;
					LEDsoff();
					LED(selected, 0);
					Dbprintf("[=] selected %x slot", selected);
				} 
			break;
			case 1:
				CmdEM410xdemod(1, &high[selected], &low[selected], 0);
				// sprintf(str, "%02lx%08lx", (uint32_t) (low[selected] >> 32), (uint32_t) low[selected]);
				// Dbprintf("[+] recorded to %i slot %s", selected, str);
                Dbprintf("[+] recorded to %i slot", selected);
				FlashLEDs(100,5);
				DbpString("[+] select mode");
				state = 0;
			break;
			case 2:
				if (button_pressed > 0) {
					SpinDown(100);
					SpinOff(100);
					LED(selected, 0);
					Dbprintf("[<] read to %x slot", selected);
					state = 1;
				} else if (button_pressed < 0) {
					LED(selected + 1, 0);
					Dbprintf("[>] prepare for sim from %x slot", selected);
					ConstructEM410xEmulBuf(str);
					// buflen = 4096;
					Dbprintf("[>] buffer generated from %x slot", selected);
					FlashLEDs(100,5);
					Dbprintf("[>] simulate from %x slot", selected);
					// Dbprintf("%i", BigBuf_get_addr());
					// buflen = 4096;
					SimulateTagLowFrequency(buflen, 0, 1);
				}
			break;
		}
	}
}
