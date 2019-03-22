//-----------------------------------------------------------------------------
//
// This code is licensed to you under the terms of the GNU GPL, version 2 or,
// at your option, any later version. See the LICENSE.txt file for the text of
// the license.
//-----------------------------------------------------------------------------
// Low frequency Honeywell NexWatch tag commands
// PSK1 RF/16, RF/2, 128 bits long (known)
//-----------------------------------------------------------------------------

#include "cmdlfnexwatch.h"

static int CmdHelp(const char *Cmd);

int detectNexWatch(uint8_t *dest, size_t *size, bool *invert) {

    uint8_t preamble[28]   = {0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    uint8_t preamble_i[28] = {1, 1, 1, 1, 1, 0, 1, 0, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    // sanity check.
    if (*size < sizeof(preamble) + 100) return -1;

    size_t startIdx = 0;

    if (!preambleSearch(DemodBuffer, preamble, sizeof(preamble), size, &startIdx)) {
        // if didn't find preamble try again inverting
        if (!preambleSearch(DemodBuffer, preamble_i, sizeof(preamble_i), size, &startIdx)) return -4;
        *invert ^= 1;
    }

    // size tests?
    return (int) startIdx;
}

int CmdNexWatchDemod(const char *Cmd) {

    if (!PSKDemod("", false)) {
        PrintAndLogEx(DEBUG, "DEBUG: Error - NexWatch can't demod signal");
        return 0;
    }
    bool invert = false;
    size_t size = DemodBufferLen;
    int idx = detectNexWatch(DemodBuffer, &size, &invert);
    if (idx <= 0) {
        if (idx == -1)
            PrintAndLogEx(DEBUG, "DEBUG: Error - NexWatch not enough samples");
        // else if (idx == -2)
        // PrintAndLogEx(DEBUG, "DEBUG: Error - NexWatch only noise found");
        // else if (idx == -3)
        // PrintAndLogEx(DEBUG, "DEBUG: Error - NexWatch problem during PSK demod");
        else if (idx == -4)
            PrintAndLogEx(DEBUG, "DEBUG: Error - NexWatch preamble not found");
        // else if (idx == -5)
        // PrintAndLogEx(DEBUG, "DEBUG: Error - NexWatch size not correct: %d", size);
        else
            PrintAndLogEx(DEBUG, "DEBUG: Error - NexWatch error %d", idx);

        return 0;
    }

    setDemodBuf(DemodBuffer, size, idx + 4);
    setClockGrid(g_DemodClock, g_DemodStartIdx + ((idx + 4)*g_DemodClock));

    idx = 8 + 32; // 8 = preamble, 32 = reserved bits (always 0)

    //get ID
    uint32_t ID = 0;
    for (uint8_t k = 0; k < 4; k++) {
        for (uint8_t m = 0; m < 8; m++) {
            ID = (ID << 1) | DemodBuffer[m + k + (m * 4)];
        }
    }
    //parity check (TBD)

    //checksum check (TBD)

    //output
    PrintAndLogEx(NORMAL, "NexWatch ID: %d", ID);
    if (invert) {
        PrintAndLogEx(NORMAL, "Had to Invert - probably NexKey");
        for (size_t i = 0; i < size; i++)
            DemodBuffer[i] ^= 1;
    }

    CmdPrintDemodBuff("x");
    return 1;
}

//by marshmellow
//see ASKDemod for what args are accepted
int CmdNexWatchRead(const char *Cmd) {
    lf_read(true, 10000);
    return CmdNexWatchDemod(Cmd);
}

static command_t CommandTable[] = {
    {"help",  CmdHelp,          1, "This help"},
    {"demod", CmdNexWatchDemod, 1, "Demodulate a NexWatch tag (nexkey, quadrakey) from the GraphBuffer"},
    {"read",  CmdNexWatchRead,  0, "Attempt to Read and Extract tag data from the antenna"},
    {NULL, NULL, 0, NULL}
};

int CmdLFNEXWATCH(const char *Cmd) {
    clearCommandBuffer();
    CmdsParse(CommandTable, Cmd);
    return 0;
}

int CmdHelp(const char *Cmd) {
    CmdsHelp(CommandTable);
    return 0;
}
