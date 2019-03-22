//-----------------------------------------------------------------------------
// Copyright (C) 2010 iZsh <izsh at fail0verflow.com>
//
// This code is licensed to you under the terms of the GNU GPL, version 2 or,
// at your option, any later version. See the LICENSE.txt file for the text of
// the license.
//-----------------------------------------------------------------------------
// Low frequency commands
//-----------------------------------------------------------------------------

#ifndef CMDLF_H__
#define CMDLF_H__
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include "proxmark3.h"
#include "lfdemod.h"        // device/client demods of LF signals
#include "util.h"           // for parsing cli command utils
#include "ui.h"             // for show graph controls
#include "graph.h"          // for graph data
#include "cmdparser.h"      // for getting cli commands included in cmdmain.h
#include "cmdmain.h"        // for sending cmds to device. GetFromBigBuf
#include "cmddata.h"        // for `lf search`
#include "cmdlfawid.h"      // for awid menu
#include "cmdlfem4x.h"      // for em4x menu
#include "cmdlfhid.h"       // for hid menu
#include "cmdlfhitag.h"     // for hitag menu
#include "cmdlfio.h"        // for ioprox menu
#include "cmdlft55xx.h"     // for t55xx menu
#include "cmdlfti.h"        // for ti menu
#include "cmdlfpresco.h"    // for presco menu
#include "cmdlfpcf7931.h"   // for pcf7931 menu
#include "cmdlfpyramid.h"   // for pyramid menu
#include "cmdlfviking.h"    // for viking menu
#include "cmdlfguard.h"     // for GuardAll menu
#include "cmdlfnedap.h"     // for NEDAP menu
#include "cmdlfjablotron.h" // for JABLOTRON menu
#include "cmdlfvisa2000.h"  // for VISA2000 menu
#include "cmdlfnoralsy.h"   // for NORALSY meny
#include "cmdlffdx.h"       // for FDX-B meny
#include "cmdlfcotag.h"     // for COTAG meny
#include "cmdlfindala.h"    // for indala menu
#include "cmdlfguard.h"     // for gproxii menu
#include "cmdlffdx.h"       // for fdx-b menu
#include "cmdlfparadox.h"   // for paradox menu
#include "cmdlfnexwatch.h"  // for nexwatch menu
#include "cmdlfsecurakey.h" // for securakey menu
#include "cmdlfpac.h"       // for pac menu
#include "cmdlfkeri.h"      // for keri menu

#define T55XX_WRITE_TIMEOUT 1500

int CmdLF(const char *Cmd);

extern int CmdLFSetConfig(const char *Cmd);

extern int CmdLFCommandRead(const char *Cmd);
extern int CmdFlexdemod(const char *Cmd);
extern int CmdLFRead(const char *Cmd);
extern int CmdLFSim(const char *Cmd);
extern int CmdLFaskSim(const char *Cmd);
extern int CmdLFfskSim(const char *Cmd);
extern int CmdLFpskSim(const char *Cmd);
extern int CmdLFSimBidir(const char *Cmd);
extern int CmdLFSniff(const char *Cmd);
extern int CmdVchDemod(const char *Cmd);
extern int CmdLFfind(const char *Cmd);

extern bool lf_read(bool silent, uint32_t samples);

// usages helptext
extern int usage_lf_cmdread(void);
extern int usage_lf_read(void);
extern int usage_lf_sniff(void);
extern int usage_lf_config(void);
extern int usage_lf_simfsk(void);
extern int usage_lf_simask(void);
extern int usage_lf_simpsk(void);
#endif
