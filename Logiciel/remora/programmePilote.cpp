// **********************************************************************************
// Programmateur Fil Pilote et Suivi Conso
// **********************************************************************************
// Copyright (C) 2014 Thibault Ducret
// Licence MIT
//
// History : 26/07/09 Implémentation du programmateur autonome
//
// **********************************************************************************

#include "programmePilote.h"

#ifdef MOD_PROGRAMME

uint8_t last_update_hour = 255; // heure de la dernière mise à jour (255 pour forcé la première mise à jour)
ptec_e last_ptec = (ptec_e)0;   // dernière période tarifaire (0 pour forcé la première mise à jour)

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
Function: updateFP
Purpose : met à jour les fils pilotes en fonction du programme
          met à jour le relai en fonction de la période tarifaire
Input   : -
Output  : -
Comments: -
====================================================================== */
void updateFP(_timer_callback_arg)
{
  uint8_t currentHour = Time.hour();

  if (last_update_hour != currentHour)
  { // on ne met à jour les fils pilotes qu'une fois par heure
    uint8_t currentWeekDay = Time.weekday();

    uint16_t offset  = ((currentWeekDay-1) * NB_FILS_PILOTES * 24) + currentHour;
    char cmd[] = "xx" ;

    for (uint8_t i=0; i<NB_FILS_PILOTES; i+=1)
    {
      cmd[0]='1' + i;
      EEPROM.get(4 + offset + i * 24, cmd[1]);
      setfp(cmd);
    }

    if ( ( currentWeekDay == 1 ) && ( currentHour == 3 ) )
    { // si on est un dimanche à 3h, on vérifie le changement d'heure
        bool daylightSavings = IsDST(Time.day(), Time.month(), Time.weekday());
        Time.zone(daylightSavings? +2 : +1);
    }

    last_update_hour = currentHour;
  }

  if (last_ptec != ptec)
  { // on allume le relai que sur changement de période tarifiare
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
  if (EEPROM.length() > 7*NB_FILS_PILOTES*24 + 4)
  {
    uint32_t keyValue;

    EEPROM.get(0x0, keyValue);

    if (keyValue != 0xCAFE0001)
    { // si l'EEPROM n'est pas initialisé
      keyValue = 0xCAFE0001;
      EEPROM.put(0x0, keyValue);

      char defaultValue = 'H';

      // On initialise le programme à Hors-Gel tout le temps pour chaque fils pilotes
      for (uint16_t i=0; i<7*NB_FILS_PILOTES*24; i+=1)
      {
        EEPROM.put(4+i, defaultValue);
      }
    }

    bool daylightSavings = IsDST(Time.day(), Time.month(), Time.weekday());
    Time.zone(daylightSavings? +2 : +1);

    // lancement du timer pour la gestion du programme
    // une latence de maximum 10 seconde est admise pour changer de tranche horaire
    ptProgrammeTimer = new Timer(10000, updateFP );
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
  command.trim();
  command.toUpperCase();

  Serial.print("setProg=");
  Serial.println(command);

  int returnValue = -1;
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
            EEPROM.put(4 + offset + i,  command[i+2]);
        }

        returnValue = 0;
      }
    }
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
  char     charValue[24];

  EEPROM.get(0x0, keyValue);
  Serial.println(keyValue);

  for (uint16_t i=0; i<7; i+=1)
  {
    for (uint16_t j=0; j<NB_FILS_PILOTES; j+=1)
    {
      Serial.print(j+1);
      Serial.print(i+1);
      EEPROM.get(4+(i*NB_FILS_PILOTES+j)*24, charValue);
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
