// **********************************************************************************
// Programmateur Fil Pilote et Suivi Conso
// **********************************************************************************
// Copyright (C) 2014 Thibault Ducret
// Licence MIT
//
// History : 26/07/09 Implémentation du programmateur autonome
//
// **********************************************************************************

#ifndef PROGRAMME_PILOTES_h
#define PROGRAMME_PILOTES_h

#include "remora.h"

#ifdef MOD_PROGRAMME

// Variables exported to other source file
// ========================================

// Function exported for other source file
// =======================================
void initProgrammeFP(void);
int setProg(String command);
int dumpEEPROM(String command);

#endif
#endif
