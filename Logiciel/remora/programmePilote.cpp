// **********************************************************************************
// Programmateur Fil Pilote et Suivi Conso
// **********************************************************************************
// Copyright (C) 2014 Thibault Ducret
// Licence MIT
//
// History : 26/07/2016 Implémentation du programmateur autonome
//
// **********************************************************************************

#include "programmePilote.h"

#ifdef MOD_PROGRAMME

bool         forceUpdate      = true;      // permet de forcer la mise à jour
uint8_t      last_update_hour;             // heure de la dernière mise à jour
ptec_e       last_ptec        = PTEC_HP;   // dernière période tarifaire (initialisé à HP pour ne pas fermer le relai au démarrage)
prog_mode_e  prog_mode        = PM_MANUAL; // par défaut le mode programmation est désactivé
relai_mode_e relai_mode       = RM_HC;     // par défaut le chauffe eau est activé en heure creuse

Timer* ptProgrammeTimer; //Timer pour la mise à jour des fils pilotes en fonction du tableau de programmation

/* ======================================================================
Function: IsDST
Purpose : Fonction pour déterminer si on est en heure d'été - implémentation pour l'heure d'été en France
Input   : -
Output  : retourne vrai si on est en heure d'été
Comments: -
====================================================================== */
bool IsDST(int dayOfMonth, int month, int dayOfWeek)
{
  if (month < 3 || month > 10)
  { // si on n'est pas entre mars et octobre
    return false;
  }
  if (month > 3 && month < 10)
  { // si on est entre mars et octobre
    return true;
  }

  int previousSunday = dayOfMonth - (dayOfWeek - 1) ;
  if (month == 3)
  { // si on est en mars, est-on après le dernier dimanche du mois ?
    return (31 - previousSunday) <= 7;
  }
  //sinon on est en octobre, est-on avant le dernier dimanche du mois ?
  return (31 - previousSunday) > 7;
}

/* ======================================================================
Function: updateProg
Purpose : met à jour les fils pilotes en fonction du programme
          met à jour le relai en fonction de la période tarifaire
Input   : -
Output  : -
Comments: -
====================================================================== */
void updateProg(_timer_callback_arg)
{
  bool isForceUpdate = forceUpdate; // on stock en local le booléen pour pouvoir le remettre à false au plus tôt (évite des problèmes dû au multi-threading)
  forceUpdate=false;
  uint8_t currentHour = Time.hour();

  if (isForceUpdate || (last_update_hour != currentHour))
  { // on ne met à jour les fils pilotes qu'une fois par heure
    uint8_t currentWeekDay = Time.weekday();

    if ((prog_mode == PM_AUTO)
       || (isForceUpdate && (prog_mode == PM_SEMIAUTO))) // fix le bug de non mise à jour des fils pilotes en PM_SEMIAUTO
    {
      uint16_t offset = ((currentWeekDay-1) * NB_FILS_PILOTES * 24) + currentHour;
      char cmd[] = "xx" ;

      for (uint8_t i=0; i<NB_FILS_PILOTES; i+=1)
      {
        cmd[0]='1' + i;
        EEPROM.get(D_PROGRAM_OFFSET + offset + i * 24, cmd[1]);
        setfp(cmd);
      }
    }
    else if (prog_mode == PM_SEMIAUTO)
    {
      uint16_t offset     = ((currentWeekDay-1) * NB_FILS_PILOTES * 24) + currentHour;
      uint16_t prevOffset;

      if (currentHour == 0)
      {// on vient de changer de jour
        if (currentWeekDay == 1) // si on est dimanche, samedi 23h
          prevOffset = (7 /* samedi */     * NB_FILS_PILOTES * 24) + 23 /* 23h */;
        else // sinon le jour d'avant 23h
          prevOffset = ((currentWeekDay-2) * NB_FILS_PILOTES * 24) + 23 /* 23h */;
      }
      else
        prevOffset = offset - 1;

      char prevCmd;
      char cmd[] = "xx" ;

      for (uint8_t i=0; i<NB_FILS_PILOTES; i+=1)
      {
        uint8_t fpOffset = i * 24;
        EEPROM.get(D_PROGRAM_OFFSET + prevOffset + fpOffset, prevCmd);
        EEPROM.get(D_PROGRAM_OFFSET + offset     + fpOffset, cmd[1]);

        if (prevCmd != cmd[1])
        {
          cmd[0]='1' + i;
          setfp(cmd);
        }
      }
    }

    if ( ( currentWeekDay == 1 ) && ( currentHour == 3 ) )
    { // si on est un dimanche à 3h, on vérifie le changement d'heure
        bool daylightSavings = IsDST(Time.day(), Time.month(), Time.weekday());
        Time.zone(daylightSavings? +2 : +1);
    }

    last_update_hour = currentHour;
  }

  if (isForceUpdate || (last_ptec != ptec))
  { // on allume le relai que sur changement de période tarifiare
    if (relai_mode == RM_HC)
    {
      switch (ptec)
      {
        case PTEC_HP :
        { // chauffe eau coupé en heure pleine
          relais("0");
        }
        break;

        case PTEC_HC :
        { // chauffe eau allumé en heure creuse
          relais("1");
        }
        break;
      }
    }
    else
      relais("0");

    last_ptec = ptec;
  }
}

/* ======================================================================
Function: initProgrammeFP
Purpose : si l'EEPROM n'est pas initialisé, on initialise le programme à hors-gel tout le temps sur tout les fils pilotes
Input   : -
Output  : -
Comments: -
====================================================================== */
void initProgrammeFP(void)
{
  if (EEPROM.length() >= 7*NB_FILS_PILOTES*24 + D_PROGRAM_OFFSET)
  {
    uint32_t keyValue;
    uint8_t  tmpProgMode  = PM_MANUAL;
    uint8_t  tmpRelaiMode = RM_HC;

    EEPROM.get(0x0, keyValue);

    if (keyValue != 0xCAFE0002)
    { // si l'EEPROM n'est pas initialisé
      keyValue = 0xCAFE0002;
      EEPROM.put(0x0, keyValue);

      EEPROM.put(D_PROGMODE_OFFSET , tmpProgMode);
      EEPROM.put(D_RELAIMODE_OFFSET, tmpRelaiMode);

      char defaultValue = 'H';

      // On initialise le programme à Hors-Gel tout le temps pour chaque fils pilotes
      for (uint16_t i=0; i<7*NB_FILS_PILOTES*24; i+=1)
      {
        EEPROM.put(D_PROGRAM_OFFSET+i, defaultValue);
      }
    }

    EEPROM.get(D_PROGMODE_OFFSET , tmpProgMode);
    prog_mode  = (prog_mode_e )tmpProgMode;

    EEPROM.get(D_RELAIMODE_OFFSET, tmpRelaiMode);
    relai_mode = (relai_mode_e)tmpRelaiMode;

    bool daylightSavings = IsDST(Time.day(), Time.month(), Time.weekday());
    Time.zone(daylightSavings? +2 : +1);

    // lancement du timer pour la gestion du programme
    // une latence de maximum 10 seconde est admise pour changer de tranche horaire
    ptProgrammeTimer = new Timer(10000, updateProg );
    ptProgrammeTimer->start();
  }
  else
  {
    Serial.println("MOD_PROGRAMME impossible");
  }
}

/* ======================================================================
Function: setProg
Purpose : défini le programme d'un fil pilote pour une journée
Input   : la fonction s'attend à avoir une chaine de 26 caractère composé du fil pilote, suivi du jour de la semaine, puis l'ordre fil pilote pour chaque heure
          le jour de la semaine va de 1 à 7 où 1 = dimanche et 7 = samedi
          ex : le fil pilote 1 sera en confort de 9h à 18h le mardi, eco le reste du temps => 13EEEEEEEEECCCCCCCCCEEEEEE
Output  : -
Comments: -
====================================================================== */
int setProg(String command)
{
  int returnValue = -1;

  command.trim();
  command.toUpperCase();

  Serial.print("setProg=");
  Serial.println(command);

  if (command.length() == 26)
  {
    uint8_t fp = command[0]-'0';
    uint8_t weekDay = command[1]-'0';

    if ( (fp < 1 || fp > NB_FILS_PILOTES))
    {
      Serial.println("Argument incorrect");
    }
    else if ( (weekDay < 1 || weekDay > 7))
    {
      Serial.println("Argument incorrect");
    }
    else
    {
      bool noError = true;
      char cOrdre;

      for (uint8_t i=0; i<24 && noError; i+=1)
      {
        cOrdre = command[i+2];
        if (cOrdre!='C' && cOrdre!='E' && cOrdre!='H' && cOrdre!='A' && cOrdre!='1' && cOrdre!='2' && cOrdre!='-' )
        {
          noError = false;
          Serial.println("Argument incorrect");
        }
      }

      if (noError)
      {
        uint16_t offset = (((weekDay-1) * NB_FILS_PILOTES) + (fp-1)) * 24;

        for (uint8_t i=0; i<24; i+=1)
        {
          if (command[i+2] != '-' )
            EEPROM.put(D_PROGRAM_OFFSET + offset + i,  command[i+2]);
        }

        forceUpdate = true;

        returnValue = 0;
      }
    }
  }

  return returnValue;
}

/* ======================================================================
Function: setProgMode
Purpose : défini le mode de fonctionnement du pilotage des fils pilotes en fonction du programme en EEPROM
Input   : "MANUAL"   => le timer ne met jamais à jour les fils pilotes
          "SEMIAUTO" => le timer met à jour les fils pilotes en fonction du programme en EEPROM uniquement si la valeur de la tranche horaire est différente de la précédante
          "AUTO"     => le timer met à jour les fils pilotes en fonction du programme en EEPROM
Output  : -
Comments: -
====================================================================== */
int setProgMode(String command)
{
  int returnValue = -1;

  command.trim();
  command.toUpperCase();

  Serial.print("setProgMode=");
  Serial.println(command);

  if (command == "MANUAL")
  {
    uint8_t tmpProgMode = prog_mode = PM_MANUAL;
    EEPROM.put(D_PROGMODE_OFFSET, tmpProgMode);
    returnValue = 0;
  }
  else if (command == "SEMIAUTO")
  {
    uint8_t tmpProgMode = prog_mode = PM_SEMIAUTO;
    EEPROM.put(D_PROGMODE_OFFSET, tmpProgMode);
    forceUpdate = true;
    returnValue = 0;
  }
  else if (command == "AUTO")
  {
    uint8_t tmpProgMode = prog_mode = PM_AUTO;
    EEPROM.put(D_PROGMODE_OFFSET, tmpProgMode);
    forceUpdate = true;
    returnValue = 0;
  }

  return returnValue;
}

/* ======================================================================
Function: setRelaiMode
Purpose : défini le mode de fonctionnement du pilotage du relai pour le chauffe eau
Input   : "NONE" => le relai est ouvert à chaque changement tarifaire (permet de le forcer pendant juste une tranche tarifaire)
          "HC"   => le relai est fermé au passage HP => HC et ouvert au passage HC => HP
Output  : -
Comments: -
====================================================================== */
int setRelaiMode(String command)
{
  int returnValue = -1;

  command.trim();
  command.toUpperCase();

  Serial.print("setRelaiMode=");
  Serial.println(command);

  if (command == "NONE")
  {
    uint8_t tmpRelaiMode = relai_mode = RM_NONE;
    EEPROM.put(D_RELAIMODE_OFFSET, tmpRelaiMode);
    forceUpdate = true;
    returnValue = 0;
  }
  else if (command == "HC")
  {
    uint8_t tmpRelaiMode = relai_mode = RM_HC;
    EEPROM.put(D_RELAIMODE_OFFSET, tmpRelaiMode);
    forceUpdate = true;
    returnValue = 0;
  }

  return returnValue;
}

/* ======================================================================
Function: dumpEEPROM
Purpose : affiche sur le lien série le contenu de l'EEPROM
Input   : -
Output  : -
Comments: -
====================================================================== */
int dumpEEPROM(String command)
{
  uint32_t keyValue;
  uint8_t  uint8Value;
  char     charValue[24];

  EEPROM.get(0x0, keyValue);
  Serial.println(keyValue);
  EEPROM.get(D_PROGMODE_OFFSET, uint8Value);
  Serial.println(uint8Value);
  EEPROM.get(D_RELAIMODE_OFFSET, uint8Value);
  Serial.println(uint8Value);

  for (uint16_t i=0; i<7; i+=1)
  {
    for (uint16_t j=0; j<NB_FILS_PILOTES; j+=1)
    {
      Serial.print(j+1);
      Serial.print(i+1);
      EEPROM.get(D_PROGRAM_OFFSET+(i*NB_FILS_PILOTES+j)*24, charValue);
      for (uint16_t k=0; k<24; k+=1)
      {
        Serial.print(charValue[k]);
      }
      Serial.println();
    }
  }

  return 0;
}

#endif
