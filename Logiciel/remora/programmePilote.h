// **********************************************************************************
// Programmateur Fil Pilote et Suivi Conso
// **********************************************************************************
// Copyright (C) 2014 Thibault Ducret
// Licence MIT
//
// History : 26/07/2016 Implémentation du programmateur autonome
//
// **********************************************************************************

#ifndef PROGRAMME_PILOTES_h
#define PROGRAMME_PILOTES_h

#include "remora.h"

#ifdef MOD_PROGRAMME

#define D_PROGMODE_OFFSET   4
#define D_RELAIMODE_OFFSET  5
#define D_PROGRAM_OFFSET   32

enum prog_mode_e  { PM_MANUAL = 1, PM_SEMIAUTO = 2, PM_AUTO = 3 };
enum relai_mode_e { RM_NONE = 1  , RM_HC = 2 };

// Variables exported to other source file
// ========================================

// Function exported for other source file
// =======================================
void initProgrammeFP(void);
int setProg(String command);
int setProgMode(String command);
int setRelaiMode(String command);
int dumpEEPROM(String command);

#endif
#endif
