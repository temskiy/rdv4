//-----------------------------------------------------------------------------
// Merlok, May 2011
// Many authors, that makes it possible
//
// This code is licensed to you under the terms of the GNU GPL, version 2 or,
// at your option, any later version. See the LICENSE.txt file for the text of
// the license.
//-----------------------------------------------------------------------------
// code for work with mifare cards.
//-----------------------------------------------------------------------------

#ifndef __MIFAREUTIL_H
#define __MIFAREUTIL_H

#include "proxmark3.h"
#include "apps.h"
#include "parity.h"
#include "util.h"
#include "string.h"
#include "iso14443crc.h"
#include "iso14443a.h"
#include "crapto1/crapto1.h"
#include "des.h"
#include "random.h" // fast_prand, prand

// mifare authentication
#define CRYPT_NONE    0
#define CRYPT_ALL     1
#define CRYPT_REQUEST 2
#define AUTH_FIRST    0
#define AUTH_NESTED   2

#define AUTHENTICATION_TIMEOUT 848      // card times out 1ms after wrong authentication (according to NXP documentation)
#define PRE_AUTHENTICATION_LEADTIME 400 // some (non standard) cards need a pause after select before they are ready for first authentication

// mifare 4bit card answers
#define CARD_ACK      0x0A  // 1010 - ACK
#define CARD_NACK_NA  0x04  // 0100 - NACK, not allowed (command not allowed)
#define CARD_NACK_TR  0x05  // 0101 - NACK, transmission error



//mifare emulator states
#define MFEMUL_NOFIELD      0
#define MFEMUL_IDLE         1
#define MFEMUL_SELECT1      2
#define MFEMUL_SELECT2      3
#define MFEMUL_SELECT3      4
#define MFEMUL_AUTH1        5
#define MFEMUL_AUTH2        6
#define MFEMUL_WORK         7
#define MFEMUL_WRITEBL2     8
#define MFEMUL_INTREG_INC   9
#define MFEMUL_INTREG_DEC   10
#define MFEMUL_INTREG_REST  11
#define MFEMUL_HALTED       12

#define cardSTATE_TO_IDLE() cardSTATE = MFEMUL_IDLE; LED_B_OFF(); LED_C_OFF();

//functions
int mifare_sendcmd(uint8_t cmd, uint8_t *data, uint8_t data_size, uint8_t *answer, uint8_t *answer_parity, uint32_t *timing);
int mifare_sendcmd_short(struct Crypto1State *pcs, uint8_t crypted, uint8_t cmd, uint8_t data, uint8_t *answer, uint8_t *answer_parity, uint32_t *timing);

// mifare classic
int mifare_classic_auth(struct Crypto1State *pcs, uint32_t uid, uint8_t blockNo, uint8_t keyType, uint64_t ui64Key, uint8_t isNested);
int mifare_classic_authex(struct Crypto1State *pcs, uint32_t uid, uint8_t blockNo, uint8_t keyType, uint64_t ui64Key, uint8_t isNested, uint32_t *ntptr, uint32_t *timing);
int mifare_classic_readblock(struct Crypto1State *pcs, uint32_t uid, uint8_t blockNo, uint8_t *blockData);
int mifare_classic_halt(struct Crypto1State *pcs, uint32_t uid);
int mifare_classic_halt_ex(struct Crypto1State *pcs);
int mifare_classic_writeblock(struct Crypto1State *pcs, uint32_t uid, uint8_t blockNo, uint8_t *blockData);

// Ultralight/NTAG...
int mifare_ul_ev1_auth(uint8_t *key, uint8_t *pack);
int mifare_ultra_auth(uint8_t *key);
int mifare_ultra_readblock(uint8_t blockNo, uint8_t *blockData);
//int mifare_ultra_writeblock_compat(uint8_t blockNo, uint8_t *blockData);
int mifare_ultra_writeblock(uint8_t blockNo, uint8_t *blockData);
int mifare_ultra_halt();

// desfire
int mifare_sendcmd_special(struct Crypto1State *pcs, uint8_t crypted, uint8_t cmd, uint8_t *data, uint8_t *answer, uint8_t *answer_parity, uint32_t *timing);
int mifare_sendcmd_special2(struct Crypto1State *pcs, uint8_t crypted, uint8_t cmd, uint8_t *data, uint8_t *answer, uint8_t *answer_parity, uint32_t *timing);
int mifare_desfire_des_auth1(uint32_t uid, uint8_t *blockData);
int mifare_desfire_des_auth2(uint32_t uid, uint8_t *key, uint8_t *blockData);

// crypto functions
void mf_crypto1_decrypt(struct Crypto1State *pcs, uint8_t *receivedCmd, int len);
void mf_crypto1_decryptEx(struct Crypto1State *pcs, uint8_t *data_in, int len, uint8_t *data_out);
void mf_crypto1_encrypt(struct Crypto1State *pcs, uint8_t *data, uint16_t len, uint8_t *par);
uint8_t mf_crypto1_encrypt4bit(struct Crypto1State *pcs, uint8_t data);

// Mifare memory structure
uint8_t NumBlocksPerSector(uint8_t sectorNo);
uint8_t FirstBlockOfSector(uint8_t sectorNo);

// emulator functions
void emlClearMem(void);
void emlSetMem(uint8_t *data, int blockNum, int blocksCount);
void emlSetMem_xt(uint8_t *data, int blockNum, int blocksCount, int blockBtWidth);
void emlGetMem(uint8_t *data, int blockNum, int blocksCount);
void emlGetMemBt(uint8_t *data, int bytePtr, int byteCount);
uint64_t emlGetKey(int sectorNum, int keyType);
int emlGetValBl(uint32_t *blReg, uint8_t *blBlock, int blockNum);
int emlSetValBl(uint32_t blReg, uint8_t blBlock, int blockNum);
int emlCheckValBl(int blockNum);

#endif
