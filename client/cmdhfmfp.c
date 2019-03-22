//-----------------------------------------------------------------------------
// Copyright (C) 2018 Merlok
// Copyright (C) 2018 drHatson
//
// This code is licensed to you under the terms of the GNU GPL, version 2 or,
// at your option, any later version. See the LICENSE.txt file for the text of
// the license.
//-----------------------------------------------------------------------------
// High frequency MIFARE  Plus commands
//-----------------------------------------------------------------------------

#include "cmdhfmfp.h"

#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "comms.h"
#include "cmdmain.h"
#include "util.h"
#include "ui.h"
#include "cmdhf14a.h"
#include "mifare.h"
#include "mifare/mifare4.h"
#include "mifare/mad.h"
#include "mifare/ndef.h"
#include "cliparser/cliparser.h"
#include "crypto/libpcrypto.h"
#include "emv/dump.h"

static const uint8_t DefaultKey[16] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

static int CmdHelp(const char *Cmd);

int CmdHFMFPInfo(const char *cmd) {

    if (cmd && strlen(cmd) > 0)
        PrintAndLogEx(WARNING, "command don't have any parameters.\n");

    // info about 14a part
    CmdHF14AInfo("");

    // Mifare Plus info
    UsbCommand c = {CMD_READER_ISO_14443a, {ISO14A_CONNECT | ISO14A_NO_DISCONNECT, 0, 0}};
    SendCommand(&c);

    UsbCommand resp;
    WaitForResponse(CMD_ACK, &resp);

    iso14a_card_select_t card;
    memcpy(&card, (iso14a_card_select_t *)resp.d.asBytes, sizeof(iso14a_card_select_t));

    uint64_t select_status = resp.arg[0]; // 0: couldn't read, 1: OK, with ATS, 2: OK, no ATS, 3: proprietary Anticollision

    if (select_status == 1 || select_status == 2) {
        PrintAndLogEx(NORMAL, "----------------------------------------------");
        PrintAndLogEx(NORMAL, "Mifare Plus info:");

        // MIFARE Type Identification Procedure
        // https://www.nxp.com/docs/en/application-note/AN10833.pdf
        uint16_t ATQA = card.atqa[0] + (card.atqa[1] << 8);
        if (ATQA == 0x0004) PrintAndLogEx(INFO, "ATQA: Mifare Plus 2k 4bUID");
        if (ATQA == 0x0002) PrintAndLogEx(INFO, "ATQA: Mifare Plus 4k 4bUID");
        if (ATQA == 0x0044) PrintAndLogEx(INFO, "ATQA: Mifare Plus 2k 7bUID");
        if (ATQA == 0x0042) PrintAndLogEx(INFO, "ATQA: Mifare Plus 4k 7bUID");

        uint8_t SLmode = 0xff;
        if (card.sak == 0x08) {
            PrintAndLogEx(INFO, "SAK: Mifare Plus 2k 7bUID");
            if (select_status == 2) SLmode = 1;
        }
        if (card.sak == 0x18) {
            PrintAndLogEx(INFO, "SAK: Mifare Plus 4k 7bUID");
            if (select_status == 2) SLmode = 1;
        }
        if (card.sak == 0x10) {
            PrintAndLogEx(INFO, "SAK: Mifare Plus 2k");
            if (select_status == 2) SLmode = 2;
        }
        if (card.sak == 0x11) {
            PrintAndLogEx(INFO, "SAK: Mifare Plus 4k");
            if (select_status == 2) SLmode = 2;
        }
        if (card.sak == 0x20) {
            PrintAndLogEx(INFO, "SAK: Mifare Plus SL0/SL3 or Mifare desfire");
            if (card.ats_len > 0) {
                SLmode = 3;

                // check SL0
                uint8_t data[250] = {0};
                int datalen = 0;
                // https://github.com/Proxmark/proxmark3/blob/master/client/scripts/mifarePlus.lua#L161
                uint8_t cmd[3 + 16] = {0xa8, 0x90, 0x90, 0x00};
                int res = ExchangeRAW14a(cmd, sizeof(cmd), false, false, data, sizeof(data), &datalen);
                if (!res && datalen > 1 && data[0] == 0x09) {
                    SLmode = 0;
                }
            }
        }

        if (SLmode != 0xff)
            PrintAndLogEx(INFO, "Mifare Plus SL mode: SL%d", SLmode);
        else
            PrintAndLogEx(WARNING, "Mifare Plus SL mode: unknown(");
    } else {
        PrintAndLogEx(INFO, "Mifare Plus info not available.");
    }

    DropField();

    return 0;
}

int CmdHFMFPWritePerso(const char *cmd) {
    uint8_t keyNum[64] = {0};
    int keyNumLen = 0;
    uint8_t key[64] = {0};
    int keyLen = 0;

    CLIParserInit("hf mfp wrp",
                  "Executes Write Perso command. Can be used in SL0 mode only.",
                  "Usage:\n\thf mfp wrp 4000 000102030405060708090a0b0c0d0e0f -> write key (00..0f) to key number 4000 \n"
                  "\thf mfp wrp 4000 -> write default key(0xff..0xff) to key number 4000");

    void *argtable[] = {
        arg_param_begin,
        arg_lit0("vV",  "verbose", "show internal data."),
        arg_str1(NULL,  NULL,      "<HEX key number (2b)>", NULL),
        arg_strx0(NULL,  NULL,     "<HEX key (16b)>", NULL),
        arg_param_end
    };
    CLIExecWithReturn(cmd, argtable, true);

    bool verbose = arg_get_lit(1);
    CLIGetHexWithReturn(2, keyNum, &keyNumLen);
    CLIGetHexWithReturn(3, key, &keyLen);
    CLIParserFree();

    mfpSetVerboseMode(verbose);

    if (!keyLen) {
        memmove(key, DefaultKey, 16);
        keyLen = 16;
    }

    if (keyNumLen != 2) {
        PrintAndLogEx(ERR, "Key number length must be 2 bytes instead of: %d", keyNumLen);
        return 1;
    }
    if (keyLen != 16) {
        PrintAndLogEx(ERR, "Key length must be 16 bytes instead of: %d", keyLen);
        return 1;
    }

    uint8_t data[250] = {0};
    int datalen = 0;

    int res = MFPWritePerso(keyNum, key, true, false, data, sizeof(data), &datalen);
    if (res) {
        PrintAndLogEx(ERR, "Exchange error: %d", res);
        return res;
    }

    if (datalen != 3) {
        PrintAndLogEx(ERR, "Command must return 3 bytes instead of: %d", datalen);
        return 1;
    }

    if (data[0] != 0x90) {
        PrintAndLogEx(ERR, "Command error: %02x %s", data[0], mfpGetErrorDescription(data[0]));
        return 1;
    }
    PrintAndLogEx(INFO, "Write OK.");

    return 0;
}

uint16_t CardAddresses[] = {0x9000, 0x9001, 0x9002, 0x9003, 0x9004, 0xA000, 0xA001, 0xA080, 0xA081, 0xC000, 0xC001};

int CmdHFMFPInitPerso(const char *cmd) {
    int res;
    uint8_t key[256] = {0};
    int keyLen = 0;
    uint8_t keyNum[2] = {0};
    uint8_t data[250] = {0};
    int datalen = 0;

    CLIParserInit("hf mfp initp",
                  "Executes Write Perso command for all card's keys. Can be used in SL0 mode only.",
                  "Usage:\n\thf mfp initp 000102030405060708090a0b0c0d0e0f -> fill all the keys with key (00..0f)\n"
                  "\thf mfp initp -vv -> fill all the keys with default key(0xff..0xff) and show all the data exchange");

    void *argtable[] = {
        arg_param_begin,
        arg_litn("vV",  "verbose", 0, 2, "show internal data."),
        arg_strx0(NULL,  NULL,      "<HEX key (16b)>", NULL),
        arg_param_end
    };
    CLIExecWithReturn(cmd, argtable, true);

    bool verbose = arg_get_lit(1);
    bool verbose2 = arg_get_lit(1) > 1;
    CLIGetHexWithReturn(2, key, &keyLen);
    CLIParserFree();

    if (keyLen && keyLen != 16) {
        PrintAndLogEx(ERR, "Key length must be 16 bytes instead of: %d", keyLen);
        return 1;
    }

    if (!keyLen)
        memmove(key, DefaultKey, 16);

    mfpSetVerboseMode(verbose2);
    for (uint16_t sn = 0x4000; sn < 0x4050; sn++) {
        keyNum[0] = sn >> 8;
        keyNum[1] = sn & 0xff;
        res = MFPWritePerso(keyNum, key, (sn == 0x4000), true, data, sizeof(data), &datalen);
        if (!res && (datalen == 3) && data[0] == 0x09) {
            PrintAndLogEx(INFO, "2k card detected.");
            break;
        }
        if (res || (datalen != 3) || data[0] != 0x90) {
            PrintAndLogEx(ERR, "Write error on address %04x", sn);
            break;
        }
    }

    mfpSetVerboseMode(verbose);
    for (int i = 0; i < sizeof(CardAddresses) / 2; i++) {
        keyNum[0] = CardAddresses[i] >> 8;
        keyNum[1] = CardAddresses[i] & 0xff;
        res = MFPWritePerso(keyNum, key, false, true, data, sizeof(data), &datalen);
        if (!res && (datalen == 3) && data[0] == 0x09) {
            PrintAndLogEx(WARNING, "Skipped[%04x]...", CardAddresses[i]);
        } else {
            if (res || (datalen != 3) || data[0] != 0x90) {
                PrintAndLogEx(ERR, "Write error on address %04x", CardAddresses[i]);
                break;
            }
        }
    }

    DropField();

    if (res)
        return res;

    PrintAndLogEx(INFO, "Done.");

    return 0;
}

int CmdHFMFPCommitPerso(const char *cmd) {
    CLIParserInit("hf mfp commitp",
                  "Executes Commit Perso command. Can be used in SL0 mode only.",
                  "Usage:\n\thf mfp commitp ->  \n");

    void *argtable[] = {
        arg_param_begin,
        arg_lit0("vV",  "verbose", "show internal data."),
        arg_int0(NULL,  NULL,      "SL mode", NULL),
        arg_param_end
    };
    CLIExecWithReturn(cmd, argtable, true);

    bool verbose = arg_get_lit(1);
    CLIParserFree();

    mfpSetVerboseMode(verbose);

    uint8_t data[250] = {0};
    int datalen = 0;

    int res = MFPCommitPerso(true, false, data, sizeof(data), &datalen);
    if (res) {
        PrintAndLogEx(ERR, "Exchange error: %d", res);
        return res;
    }

    if (datalen != 3) {
        PrintAndLogEx(ERR, "Command must return 3 bytes instead of: %d", datalen);
        return 1;
    }

    if (data[0] != 0x90) {
        PrintAndLogEx(ERR, "Command error: %02x %s", data[0], mfpGetErrorDescription(data[0]));
        return 1;
    }
    PrintAndLogEx(INFO, "Switch level OK.");

    return 0;
}

int CmdHFMFPAuth(const char *cmd) {
    uint8_t keyn[250] = {0};
    int keynlen = 0;
    uint8_t key[250] = {0};
    int keylen = 0;

    CLIParserInit("hf mfp auth",
                  "Executes AES authentication command for Mifare Plus card",
                  "Usage:\n\thf mfp auth 4000 000102030405060708090a0b0c0d0e0f -> executes authentication\n"
                  "\thf mfp auth 9003 FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF -v -> executes authentication and shows all the system data\n");

    void *argtable[] = {
        arg_param_begin,
        arg_lit0("vV",  "verbose", "show internal data."),
        arg_str1(NULL,  NULL,     "<Key Num (HEX 2 bytes)>", NULL),
        arg_str1(NULL,  NULL,     "<Key Value (HEX 16 bytes)>", NULL),
        arg_param_end
    };
    CLIExecWithReturn(cmd, argtable, true);

    bool verbose = arg_get_lit(1);
    CLIGetHexWithReturn(2, keyn, &keynlen);
    CLIGetHexWithReturn(3, key, &keylen);
    CLIParserFree();

    if (keynlen != 2) {
        PrintAndLogEx(ERR, "ERROR: <Key Num> must be 2 bytes long instead of: %d", keynlen);
        return 1;
    }

    if (keylen != 16) {
        PrintAndLogEx(ERR, "ERROR: <Key Value> must be 16 bytes long instead of: %d", keylen);
        return 1;
    }

    return MifareAuth4(NULL, keyn, key, true, false, verbose);
}

int CmdHFMFPRdbl(const char *cmd) {
    uint8_t keyn[2] = {0};
    uint8_t key[250] = {0};
    int keylen = 0;

    CLIParserInit("hf mfp rdbl",
                  "Reads several blocks from Mifare Plus card.",
                  "Usage:\n\thf mfp rdbl 0 000102030405060708090a0b0c0d0e0f -> executes authentication and read block 0 data\n"
                  "\thf mfp rdbl 1 -v -> executes authentication and shows sector 1 data with default key 0xFF..0xFF and some additional data\n");

    void *argtable[] = {
        arg_param_begin,
        arg_lit0("vV",  "verbose", "show internal data."),
        arg_int0("nN",  "count",   "blocks count (by default 1).", NULL),
        arg_lit0("bB",  "keyb",    "use key B (by default keyA)."),
        arg_lit0("pP",  "plain",   "plain communication mode between reader and card."),
        arg_int1(NULL,  NULL,      "<Block Num (0..255)>", NULL),
        arg_str0(NULL,  NULL,      "<Key Value (HEX 16 bytes)>", NULL),
        arg_param_end
    };
    CLIExecWithReturn(cmd, argtable, false);

    bool verbose = arg_get_lit(1);
    int blocksCount = arg_get_int_def(2, 1);
    bool keyB = arg_get_lit(3);
    int plain = arg_get_lit(4);
    uint32_t blockn = arg_get_int(5);
    CLIGetHexWithReturn(6, key, &keylen);
    CLIParserFree();

    mfpSetVerboseMode(verbose);

    if (!keylen) {
        memmove(key, DefaultKey, 16);
        keylen = 16;
    }

    if (blockn > 255) {
        PrintAndLogEx(ERR, "<Block Num> must be in range [0..255] instead of: %d", blockn);
        return 1;
    }

    if (keylen != 16) {
        PrintAndLogEx(ERR, "<Key Value> must be 16 bytes long instead of: %d", keylen);
        return 1;
    }

    // 3 blocks - wo iso14443-4 chaining
    if (blocksCount > 3) {
        PrintAndLogEx(ERR, "blocks count must be less than 3 instead of: %d", blocksCount);
        return 1;
    }

    if (blocksCount > 1 && mfIsSectorTrailer(blockn)) {
        PrintAndLog("WARNING: trailer!");
    }

    uint8_t sectorNum = mfSectorNum(blockn & 0xff);
    uint16_t uKeyNum = 0x4000 + sectorNum * 2 + (keyB ? 1 : 0);
    keyn[0] = uKeyNum >> 8;
    keyn[1] = uKeyNum & 0xff;
    if (verbose)
        PrintAndLogEx(INFO, "--block:%d sector[%d]:%02x key:%04x", blockn, mfNumBlocksPerSector(sectorNum), sectorNum, uKeyNum);

    mf4Session session;
    int res = MifareAuth4(&session, keyn, key, true, true, verbose);
    if (res) {
        PrintAndLogEx(ERR, "Authentication error: %d", res);
        return res;
    }

    uint8_t data[250] = {0};
    int datalen = 0;
    uint8_t mac[8] = {0};
    res = MFPReadBlock(&session, plain, blockn & 0xff, blocksCount, false, false, data, sizeof(data), &datalen, mac);
    if (res) {
        PrintAndLogEx(ERR, "Read error: %d", res);
        return res;
    }

    if (datalen && data[0] != 0x90) {
        PrintAndLogEx(ERR, "Card read error: %02x %s", data[0], mfpGetErrorDescription(data[0]));
        return 6;
    }

    if (datalen != 1 + blocksCount * 16 + 8 + 2) {
        PrintAndLogEx(ERR, "Error return length:%d", datalen);
        return 5;
    }

    int indx = blockn;
    for (int i = 0; i < blocksCount; i++)  {
        PrintAndLogEx(INFO, "data[%03d]: %s", indx, sprint_hex(&data[1 + i * 16], 16));
        indx++;
        if (mfIsSectorTrailer(indx) && i != blocksCount - 1) {
            PrintAndLogEx(INFO, "data[%03d]: ------------------- trailer -------------------", indx);
            indx++;
        }
    }

    if (memcmp(&data[blocksCount * 16 + 1], mac, 8)) {
        PrintAndLogEx(WARNING, "WARNING: mac not equal...");
        PrintAndLogEx(WARNING, "MAC   card: %s", sprint_hex(&data[blocksCount * 16 + 1], 8));
        PrintAndLogEx(WARNING, "MAC reader: %s", sprint_hex(mac, 8));
    } else {
        if (verbose)
            PrintAndLogEx(INFO, "MAC: %s", sprint_hex(&data[blocksCount * 16 + 1], 8));
    }

    return 0;
}

int CmdHFMFPRdsc(const char *cmd) {
    uint8_t keyn[2] = {0};
    uint8_t key[250] = {0};
    int keylen = 0;

    CLIParserInit("hf mfp rdsc",
                  "Reads one sector from Mifare Plus card.",
                  "Usage:\n\thf mfp rdsc 0 000102030405060708090a0b0c0d0e0f -> executes authentication and read sector 0 data\n"
                  "\thf mfp rdsc 1 -v -> executes authentication and shows sector 1 data with default key 0xFF..0xFF and some additional data\n");

    void *argtable[] = {
        arg_param_begin,
        arg_lit0("vV",  "verbose", "show internal data."),
        arg_lit0("bB",  "keyb",    "use key B (by default keyA)."),
        arg_lit0("pP",  "plain",   "plain communication mode between reader and card."),
        arg_int1(NULL,  NULL,      "<Sector Num (0..255)>", NULL),
        arg_str0(NULL,  NULL,      "<Key Value (HEX 16 bytes)>", NULL),
        arg_param_end
    };
    CLIExecWithReturn(cmd, argtable, false);

    bool verbose = arg_get_lit(1);
    bool keyB = arg_get_lit(2);
    bool plain = arg_get_lit(3);
    uint32_t sectorNum = arg_get_int(4);
    CLIGetHexWithReturn(5, key, &keylen);
    CLIParserFree();

    mfpSetVerboseMode(verbose);

    if (!keylen) {
        memmove(key, DefaultKey, 16);
        keylen = 16;
    }

    if (sectorNum > 39) {
        PrintAndLogEx(ERR, "<Sector Num> must be in range [0..39] instead of: %d", sectorNum);
        return 1;
    }

    if (keylen != 16) {
        PrintAndLogEx(ERR, "<Key Value> must be 16 bytes long instead of: %d", keylen);
        return 1;
    }

    uint16_t uKeyNum = 0x4000 + sectorNum * 2 + (keyB ? 1 : 0);
    keyn[0] = uKeyNum >> 8;
    keyn[1] = uKeyNum & 0xff;
    if (verbose)
        PrintAndLogEx(INFO, "--sector[%d]:%02x key:%04x", mfNumBlocksPerSector(sectorNum), sectorNum, uKeyNum);

    mf4Session session;
    int res = MifareAuth4(&session, keyn, key, true, true, verbose);
    if (res) {
        PrintAndLogEx(ERR, "Authentication error: %d", res);
        return res;
    }

    uint8_t data[250] = {0};
    int datalen = 0;
    uint8_t mac[8] = {0};
    for (int n = mfFirstBlockOfSector(sectorNum); n < mfFirstBlockOfSector(sectorNum) + mfNumBlocksPerSector(sectorNum); n++) {
        res = MFPReadBlock(&session, plain, n & 0xff, 1, false, true, data, sizeof(data), &datalen, mac);
        if (res) {
            PrintAndLogEx(ERR, "Read error: %d", res);
            DropField();
            return res;
        }

        if (datalen && data[0] != 0x90) {
            PrintAndLogEx(ERR, "Card read error: %02x %s", data[0], mfpGetErrorDescription(data[0]));
            DropField();
            return 6;
        }
        if (datalen != 1 + 16 + 8 + 2) {
            PrintAndLogEx(ERR, "Error return length:%d", datalen);
            DropField();
            return 5;
        }

        PrintAndLogEx(INFO, "data[%03d]: %s", n, sprint_hex(&data[1], 16));

        if (memcmp(&data[1 + 16], mac, 8)) {
            PrintAndLogEx(WARNING, "WARNING: mac on block %d not equal...", n);
            PrintAndLogEx(WARNING, "MAC   card: %s", sprint_hex(&data[1 + 16], 8));
            PrintAndLogEx(WARNING, "MAC reader: %s", sprint_hex(mac, 8));
        } else {
            if (verbose)
                PrintAndLogEx(INFO, "MAC: %s", sprint_hex(&data[1 + 16], 8));
        }
    }
    DropField();

    return 0;
}

int CmdHFMFPWrbl(const char *cmd) {
    uint8_t keyn[2] = {0};
    uint8_t key[250] = {0};
    int keylen = 0;
    uint8_t datain[250] = {0};
    int datainlen = 0;

    CLIParserInit("hf mfp wrbl",
                  "Writes one block to Mifare Plus card.",
                  "Usage:\n\thf mfp wrbl 1 ff0000000000000000000000000000ff 000102030405060708090a0b0c0d0e0f -> writes block 1 data\n"
                  "\thf mfp wrbl 2 ff0000000000000000000000000000ff -v -> writes block 2 data with default key 0xFF..0xFF and some additional data\n");

    void *argtable[] = {
        arg_param_begin,
        arg_lit0("vV",  "verbose", "show internal data."),
        arg_lit0("bB",  "keyb",    "use key B (by default keyA)."),
        arg_int1(NULL,  NULL,      "<Block Num (0..255)>", NULL),
        arg_str1(NULL,  NULL,      "<Data (HEX 16 bytes)>", NULL),
        arg_str0(NULL,  NULL,      "<Key (HEX 16 bytes)>", NULL),
        arg_param_end
    };
    CLIExecWithReturn(cmd, argtable, false);

    bool verbose = arg_get_lit(1);
    bool keyB = arg_get_lit(2);
    uint32_t blockNum = arg_get_int(3);
    CLIGetHexWithReturn(4, datain, &datainlen);
    CLIGetHexWithReturn(5, key, &keylen);
    CLIParserFree();

    mfpSetVerboseMode(verbose);

    if (!keylen) {
        memmove(key, DefaultKey, 16);
        keylen = 16;
    }

    if (blockNum > 255) {
        PrintAndLogEx(ERR, "<Block Num> must be in range [0..255] instead of: %d", blockNum);
        return 1;
    }

    if (keylen != 16) {
        PrintAndLogEx(ERR, "<Key> must be 16 bytes long instead of: %d", keylen);
        return 1;
    }

    if (datainlen != 16) {
        PrintAndLogEx(ERR, "<Data> must be 16 bytes long instead of: %d", datainlen);
        return 1;
    }

    uint8_t sectorNum = mfSectorNum(blockNum & 0xff);
    uint16_t uKeyNum = 0x4000 + sectorNum * 2 + (keyB ? 1 : 0);
    keyn[0] = uKeyNum >> 8;
    keyn[1] = uKeyNum & 0xff;
    if (verbose)
        PrintAndLogEx(INFO, "--block:%d sector[%d]:%02x key:%04x", blockNum & 0xff, mfNumBlocksPerSector(sectorNum), sectorNum, uKeyNum);

    mf4Session session;
    int res = MifareAuth4(&session, keyn, key, true, true, verbose);
    if (res) {
        PrintAndLogEx(ERR, "Authentication error: %d", res);
        return res;
    }

    uint8_t data[250] = {0};
    int datalen = 0;
    uint8_t mac[8] = {0};
    res = MFPWriteBlock(&session, blockNum & 0xff, datain, false, false, data, sizeof(data), &datalen, mac);
    if (res) {
        PrintAndLogEx(ERR, "Write error: %d", res);
        DropField();
        return res;
    }

    if (datalen != 3 && (datalen != 3 + 8)) {
        PrintAndLogEx(ERR, "Error return length:%d", datalen);
        DropField();
        return 5;
    }

    if (datalen && data[0] != 0x90) {
        PrintAndLogEx(ERR, "Card write error: %02x %s", data[0], mfpGetErrorDescription(data[0]));
        DropField();
        return 6;
    }

    if (memcmp(&data[1], mac, 8)) {
        PrintAndLogEx(WARNING, "WARNING: mac not equal...");
        PrintAndLogEx(WARNING, "MAC   card: %s", sprint_hex(&data[1], 8));
        PrintAndLogEx(WARNING, "MAC reader: %s", sprint_hex(mac, 8));
    } else {
        if (verbose)
            PrintAndLogEx(INFO, "MAC: %s", sprint_hex(&data[1], 8));
    }

    DropField();
    PrintAndLogEx(INFO, "Write OK.");
    return 0;
}

int CmdHFMFPMAD(const char *cmd) {

    CLIParserInit("hf mfp mad",
                  "Checks and prints Mifare Application Directory (MAD)",
                  "Usage:\n\thf mfp mad -> shows MAD if exists\n"
                  "\thf mfp mad -a 03e1 -k d3f7d3f7d3f7d3f7d3f7d3f7d3f7d3f7 -> shows NDEF data if exists\n");

    void *argtable[] = {
        arg_param_begin,
        arg_lit0("vV",  "verbose",  "show technical data"),
        arg_str0("aA",  "aid",      "print all sectors with aid", NULL),
        arg_str0("kK",  "key",      "key for printing sectors", NULL),
        arg_lit0("bB",  "keyb",     "use key B for access printing sectors (by default: key A)"),
        arg_param_end
    };
    CLIExecWithReturn(cmd, argtable, true);

    bool verbose = arg_get_lit(1);
    uint8_t aid[2] = {0};
    int aidlen;
    CLIGetHexWithReturn(2, aid, &aidlen);
    uint8_t key[16] = {0};
    int keylen;
    CLIGetHexWithReturn(3, key, &keylen);
    bool keyB = arg_get_lit(4);

    CLIParserFree();

    if (aidlen != 2 && keylen > 0) {
        PrintAndLogEx(WARNING, "do not need a key without aid.");
    }

    uint8_t sector0[16 * 4] = {0};
    uint8_t sector10[16 * 4] = {0};

    if (mfpReadSector(MF_MAD1_SECTOR, MF_KEY_A, (uint8_t *)g_mifarep_mad_key, sector0, verbose)) {
        PrintAndLogEx(NORMAL, "");
        PrintAndLogEx(ERR, "read sector 0 error. card don't have MAD or don't have MAD on default keys.");
        return 2;
    }

    if (verbose) {
        for (int i = 0; i < 4; i ++)
            PrintAndLogEx(NORMAL, "[%d] %s", i, sprint_hex(&sector0[i * 16], 16));
    }

    bool haveMAD2 = false;
    MAD1DecodeAndPrint(sector0, verbose, &haveMAD2);

    if (haveMAD2) {
        if (mfpReadSector(MF_MAD2_SECTOR, MF_KEY_A, (uint8_t *)g_mifarep_mad_key, sector10, verbose)) {
            PrintAndLogEx(NORMAL, "");
            PrintAndLogEx(ERR, "read sector 0x10 error. card don't have MAD or don't have MAD on default keys.");
            return 2;
        }

        MAD2DecodeAndPrint(sector10, verbose);
    }

    if (aidlen == 2) {
        uint16_t aaid = (aid[0] << 8) + aid[1];
        PrintAndLogEx(NORMAL, "\n-------------- AID 0x%04x ---------------", aaid);

        uint16_t mad[7 + 8 + 8 + 8 + 8] = {0};
        size_t madlen = 0;
        if (MADDecode(sector0, sector10, mad, &madlen)) {
            PrintAndLogEx(ERR, "can't decode mad.");
            return 10;
        }

        uint8_t akey[16] = {0};
        memcpy(akey, g_mifarep_ndef_key, 16);
        if (keylen == 16) {
            memcpy(akey, key, 16);
        }

        for (int i = 0; i < madlen; i++) {
            if (aaid == mad[i]) {
                uint8_t vsector[16 * 4] = {0};
                if (mfpReadSector(i + 1, keyB ? MF_KEY_B : MF_KEY_A, akey, vsector, false)) {
                    PrintAndLogEx(NORMAL, "");
                    PrintAndLogEx(ERR, "read sector %d error.", i + 1);
                    return 2;
                }

                for (int j = 0; j < (verbose ? 4 : 3); j ++)
                    PrintAndLogEx(NORMAL, " [%03d] %s", (i + 1) * 4 + j, sprint_hex(&vsector[j * 16], 16));
            }
        }
    }

    return 0;
}

int CmdHFMFPNDEF(const char *cmd) {

    CLIParserInit("hf mfp ndef",
                  "Prints NFC Data Exchange Format (NDEF)",
                  "Usage:\n\thf mfp ndef -> shows NDEF data\n"
                  "\thf mfp ndef -a 03e1 -k d3f7d3f7d3f7d3f7d3f7d3f7d3f7d3f7 -> shows NDEF data with custom AID and key\n");

    void *argtable[] = {
        arg_param_begin,
        arg_litn("vV",  "verbose",  0, 2, "show technical data"),
        arg_str0("aA",  "aid",      "replace default aid for NDEF", NULL),
        arg_str0("kK",  "key",      "replace default key for NDEF", NULL),
        arg_lit0("bB",  "keyb",     "use key B for access sectors (by default: key A)"),
        arg_param_end
    };
    CLIExecWithReturn(cmd, argtable, true);

    bool verbose = arg_get_lit(1);
    bool verbose2 = arg_get_lit(1) > 1;
    uint8_t aid[2] = {0};
    int aidlen;
    CLIGetHexWithReturn(2, aid, &aidlen);
    uint8_t key[16] = {0};
    int keylen;
    CLIGetHexWithReturn(3, key, &keylen);
    bool keyB = arg_get_lit(4);

    CLIParserFree();

    uint16_t ndefAID = 0x03e1;
    if (aidlen == 2)
        ndefAID = (aid[0] << 8) + aid[1];

    uint8_t ndefkey[16] = {0};
    memcpy(ndefkey, g_mifarep_ndef_key, 16);
    if (keylen == 16) {
        memcpy(ndefkey, key, 16);
    }

    uint8_t sector0[16 * 4] = {0};
    uint8_t sector10[16 * 4] = {0};
    uint8_t data[4096] = {0};
    int datalen = 0;

    PrintAndLogEx(NORMAL, "");

    if (mfpReadSector(MF_MAD1_SECTOR, MF_KEY_A, (uint8_t *)g_mifarep_mad_key, sector0, verbose)) {
        PrintAndLogEx(ERR, "read sector 0 error. card don't have MAD or don't have MAD on default keys.");
        return 2;
    }

    bool haveMAD2 = false;
    int res = MADCheck(sector0, NULL, verbose, &haveMAD2);
    if (res) {
        PrintAndLogEx(ERR, "MAD error %d.", res);
        return res;
    }

    if (haveMAD2) {
        if (mfpReadSector(MF_MAD2_SECTOR, MF_KEY_A, (uint8_t *)g_mifarep_mad_key, sector10, verbose)) {
            PrintAndLogEx(ERR, "read sector 0x10 error. card don't have MAD or don't have MAD on default keys.");
            return 2;
        }
    }

    uint16_t mad[7 + 8 + 8 + 8 + 8] = {0};
    size_t madlen = 0;
    if (MADDecode(sector0, (haveMAD2 ? sector10 : NULL), mad, &madlen)) {
        PrintAndLogEx(ERR, "can't decode mad.");
        return 10;
    }

    printf("data reading:");
    for (int i = 0; i < madlen; i++) {
        if (ndefAID == mad[i]) {
            uint8_t vsector[16 * 4] = {0};
            if (mfpReadSector(i + 1, keyB ? MF_KEY_B : MF_KEY_A, ndefkey, vsector, false)) {
                PrintAndLogEx(ERR, "read sector %d error.", i + 1);
                return 2;
            }

            memcpy(&data[datalen], vsector, 16 * 3);
            datalen += 16 * 3;

            printf(".");
        }
    }
    printf(" OK\n");

    if (!datalen) {
        PrintAndLogEx(ERR, "no NDEF data.");
        return 11;
    }

    if (verbose2) {
        PrintAndLogEx(NORMAL, "NDEF data:");
        dump_buffer(data, datalen, stdout, 1);
    }

    NDEFDecodeAndPrint(data, datalen, verbose);

    return 0;
}

static command_t CommandTable[] = {
    {"help",             CmdHelp,                 1, "This help"},
    {"info",             CmdHFMFPInfo,            0, "Info about Mifare Plus tag"},
    {"wrp",              CmdHFMFPWritePerso,      0, "Write Perso command"},
    {"initp",            CmdHFMFPInitPerso,       0, "Fills all the card's keys"},
    {"commitp",          CmdHFMFPCommitPerso,     0, "Move card to SL1 or SL3 mode"},
    {"auth",             CmdHFMFPAuth,            0, "Authentication"},
    {"rdbl",             CmdHFMFPRdbl,            0, "Read blocks"},
    {"rdsc",             CmdHFMFPRdsc,            0, "Read sectors"},
    {"wrbl",             CmdHFMFPWrbl,            0, "Write blocks"},
    {"mad",              CmdHFMFPMAD,             0, "Checks and prints MAD"},
    {"ndef",             CmdHFMFPNDEF,            0, "Prints NDEF records from card"},
    {NULL,               NULL,                    0, NULL}
};

int CmdHFMFP(const char *Cmd) {
    (void)WaitForResponseTimeout(CMD_ACK, NULL, 100);
    CmdsParse(CommandTable, Cmd);
    return 0;
}

int CmdHelp(const char *Cmd) {
    CmdsHelp(CommandTable);
    return 0;
}
