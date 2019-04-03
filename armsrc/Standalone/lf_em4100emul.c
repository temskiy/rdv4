
//-----------------------------------------------------------------------------
// 
// 
//
// This code is licensed to you under the terms of the GNU GPL, version 2 or,
// at your option, any later version. See the LICENSE.txt file for the text of
// the license.
//-----------------------------------------------------------------------------
// main code for LF ....
//-----------------------------------------------------------------------------
#include "lf_em4100emul.h"


void RunMod() {
	StandAloneMode();
	FpgaDownloadAndGo(FPGA_BITSTREAM_LF);

	uint64_t low[OPTS];
	uint32_t high[OPTS];
	int selected = 0;
	int state = 0; //0 - idle, 1 - read, 2 - sim

	DbpString("[+] now in ListenReaderField mode");
	ListenReaderField(1);

	LED(selected + 1, 0);
	DbpString("[+] now in select mode");
	for (;;) {		
		WDT_HIT();

		if (usb_poll_validate_length()) break;
		SpinDelay(300);
		
		switch (state){
			case 0:
				if (BUTTON_HELD(1000) > 0) {
					SpinUp(100);
					SpinOff(100);
					state = 2;
				} else if (BUTTON_HELD(100) > 0) {
					selected = (selected + 1) % OPTS;
					LEDsoff();
					LED(selected + 1, 0);
					Dbprintf("[+] selected %x slot", selected);
				}
			break;
			case 1:
				CmdEM410xdemod(1, &high[selected], &low[selected], 0);
				Dbprintf("[++] recorded to %x slot %x %x", selected, high[selected], low[selected]);
				FlashLEDs(100,3);
				state = 0;
			break;
			case 2:
				if (BUTTON_HELD(2000) > 0) {
					SpinDown(100);
					SpinOff(100);
					LED(selected + 1, 0);
					Dbprintf("[+] read to %x slot", selected);
					state = 1;
				} else if (BUTTON_HELD(100) > 0) {
					LED(selected + 1, 0);
					Dbprintf("[+] sim from %x slot", selected);


				}
			break;


		}

	}
}