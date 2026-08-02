#include "locale.h"
#include <string.h>
#include <grrlib.h>
#include <wiiuse/wpad.h>
#include <ogc/conf.h>

static Language s_activeLang = LANG_ENGLISH;

/* ---------------------------------------------------------------------------
 * Translation table: [language][stringID]
 * ------------------------------------------------------------------------- */

static const char* s_table[LANG_COUNT][STR_COUNT] = {
    /* ENGLISH */
    {
        "Plants vs. Zombies",                  /* STR_TITLE */
        "Adventure",                           /* STR_ADVENTURE */
        "Minigames",                           /* STR_MINIGAMES */
        "Options",                             /* STR_OPTIONS */
        "Exit",                                /* STR_EXIT */
        "Language",                            /* STR_LANGUAGE */
        "Volume",                              /* STR_VOLUME */
        "Controls",                            /* STR_CONTROLS */
        "Back",                                /* STR_BACK */
        "Resume",                              /* STR_RESUME */
        "Restart Level",                       /* STR_RESTART_LEVEL */
        "Close",                               /* STR_CLOSE */
        "Sun",                                 /* STR_SUN */
        "Wave",                                /* STR_WAVE */
        "Zombies incoming...",                 /* STR_ZOMBIES_INCOMING */
        "Choose your plants",                  /* STR_CHOOSE_PLANTS */
        "Selected",                            /* STR_SELECTED */
        "A: select/deselect  |  START: begin", /* STR_SELECT_DESELECT */
        "Begin",                               /* STR_BEGIN */
        "Level Complete!",                     /* STR_LEVEL_COMPLETE */
        "Game Over",                           /* STR_GAME_OVER */
        "The zombies got in...",               /* STR_ZOMBIES_GOT_IN */
        "Big Trouble Little Zombie",           /* STR_MG_BIG_TROUBLE */
        "Zombie Nimble Zombie Quick",          /* STR_MG_NIMBLE_QUICK */
        "Column Like You See 'Em",             /* STR_MG_COLUMN_LIKE */
        "Invisi-ghoul",                        /* STR_MG_INVISI_GHOUL */
        "Last Stand",                          /* STR_MG_LAST_STAND */
        "ZomBotany",                           /* STR_MG_ZOMBOTANY */
        "Wall-nut Bowling",                    /* STR_MG_WALLNUT_BOWLING */
        "Slot Machine",                        /* STR_MG_SLOT_MACHINE */
        "It's Raining Seeds",                  /* STR_MG_RAINING_SEEDS */
        "Beghouled",                           /* STR_MG_BEGHOULED */
        "Seeing Stars",                        /* STR_MG_SEEING_STARS */
        "Zombiquarium",                        /* STR_MG_ZOMBIQUARIUM */
        "Beghouled Twist",                     /* STR_MG_BEGHOULED_TWIST */
        "Portal Combat",                       /* STR_MG_PORTAL_COMBAT */
        "Bobsled Bonanza",                     /* STR_MG_BOBSLED */
        "Whack a Zombie",                      /* STR_MG_WHACK_ZOMBIE */
        "ZomBotany 2",                         /* STR_MG_ZOMBOTANY_2 */
        "Wall-nut Bowling 2",                  /* STR_MG_WALLNUT_BOWLING_2 */
        "Pogo Party",                          /* STR_MG_POGO_PARTY */
        "Dr. Zomboss's Revenge",               /* STR_MG_ZOMBOSS_REVENGE */
        /* STR_SURVIVAL */
        "Survival",
        /* STR_SURVIVAL_DAY_EASY through STR_SURVIVAL_ENDLESS */
        "Survival: Day (Easy)",
        "Survival: Day (Hard)",
        "Survival: Night (Easy)",
        "Survival: Night (Hard)",
        "Survival: Pool (Easy)",
        "Survival: Pool (Hard)",
        "Survival: Fog (Easy)",
        "Survival: Fog (Hard)",
        "Survival: Roof (Easy)",
        "Survival: Roof (Hard)",
        "Survival: Endless",
    },
    /* FRENCH */
    {
        "Plants vs. Zombies",
        "Aventure",
        "Mini-jeux",
        "Options",
        "Quitter",
        "Langue",
        "Volume",
        "Controles",
        "Retour",
        "Reprendre",
        "Recommencer le niveau",
        "Fermer",
        "Soleil",
        "Vague",
        "Zombies en approche...",
        "Choisissez vos plantes",
        "Selectionnees",
        "A: selectionner  |  START: commencer",
        "Commencer",
        "Niveau termine !",
        "Partie terminee",
        "Les zombies sont entres...",
        "Gros Probleme Petit Zombie",
        "Zombie Agile Zombie Rapide",
        "Colonne Comme Tu Vois",
        "Invisi-fantome",
        "Derniere Position",
        "ZomBotanie",
        "Bowling Noix",
        "Machine a sous",
        "Il pleut des graines",
        "Beghouled",
        "Voir des Etoiles",
        "Zombiquarium",
        "Beghouled Twist",
        "Combat de Portails",
        "Bonanza Bobsled",
        "Frappe un Zombie",
        "ZomBotanie 2",
        "Bowling Noix 2",
        "Pogo Party",
        "La Revanche du Dr. Zomboss",
        "Survie",
        "Survie: Jour (Facile)",
        "Survie: Jour (Difficile)",
        "Survie: Nuit (Facile)",
        "Survie: Nuit (Difficile)",
        "Survie: Piscine (Facile)",
        "Survie: Piscine (Difficile)",
        "Survie: Brouillard (Facile)",
        "Survie: Brouillard (Difficile)",
        "Survie: Toit (Facile)",
        "Survie: Toit (Difficile)",
        "Survie: Infini",
    },
    /* GERMAN */
    {
        "Plants vs. Zombies",
        "Abenteuer",
        "Minispiele",
        "Optionen",
        "Beenden",
        "Sprache",
        "Lautstarke",
        "Steuerung",
        "Zuruck",
        "Fortsetzen",
        "Level neustarten",
        "Schliessen",
        "Sonne",
        "Welle",
        "Zombies nahern sich...",
        "Wahle deine Pflanzen",
        "Ausgewahlt",
        "A: wahlen  |  START: beginnen",
        "Beginnen",
        "Level abgeschlossen!",
        "Spiel vorbei",
        "Die Zombies sind eingedrungen...",
        "Grosser Arger Kleiner Zombie",
        "Zombie Flink Zombie Schnell",
        "Reihe Wie Du Siehst",
        "Invisi-Geist",
        "Letzter Widerstand",
        "ZomBotanik",
        "Wall-nut Bowling",
        "Spielautomat",
        "Es regnet Samen",
        "Beghouled",
        "Sterne Sehen",
        "Zombiquarium",
        "Beghouled Twist",
        "Portal Kampf",
        "Bobsled Bonanza",
        "Hau den Zombie",
        "ZomBotanik 2",
        "Wall-nut Bowling 2",
        "Pogo Party",
        "Dr. Zomboss' Rache",
        "Uberleben",
        "Uberleben: Tag (Leicht)",
        "Uberleben: Tag (Schwer)",
        "Uberleben: Nacht (Leicht)",
        "Uberleben: Nacht (Schwer)",
        "Uberleben: Pool (Leicht)",
        "Uberleben: Pool (Schwer)",
        "Uberleben: Nebel (Leicht)",
        "Uberleben: Nebel (Schwer)",
        "Uberleben: Dach (Leicht)",
        "Uberleben: Dach (Schwer)",
        "Uberleben: Endlos",
    },
    /* SPANISH */
    {
        "Plants vs. Zombies",
        "Aventura",
        "Minijuegos",
        "Opciones",
        "Salir",
        "Idioma",
        "Volumen",
        "Controles",
        "Atras",
        "Reanudar",
        "Reiniciar nivel",
        "Cerrar",
        "Sol",
        "Oleada",
        "Zombis acercandose...",
        "Elige tus plantas",
        "Seleccionadas",
        "A: seleccionar  |  START: empezar",
        "Empezar",
        "Nivel completado!",
        "Juego terminado",
        "Los zombis entraron...",
        "Gran Problema Pequeno Zombi",
        "Zombi Agil Zombi Rapido",
        "Columna Como Ves",
        "Invisi-fantasma",
        "Ultima Posicion",
        "ZomBotania",
        "Bolos Noz",
        "Tragamonedas",
        "Llueven semillas",
        "Beghouled",
        "Viendo Estrellas",
        "Zombiquarium",
        "Beghouled Twist",
        "Combate de Portales",
        "Bobsled Bonanza",
        "Golpea al Zombi",
        "ZomBotania 2",
        "Bolos Noz 2",
        "Fiesta Pogo",
        "La Venganza del Dr. Zomboss",
        "Supervivencia",
        "Supervivencia: Dia (Facil)",
        "Supervivencia: Dia (Dificil)",
        "Supervivencia: Noche (Facil)",
        "Supervivencia: Noche (Dificil)",
        "Supervivencia: Piscina (Facil)",
        "Supervivencia: Piscina (Dificil)",
        "Supervivencia: Niebla (Facil)",
        "Supervivencia: Niebla (Dificil)",
        "Supervivencia: Tejado (Facil)",
        "Supervivencia: Tejado (Dificil)",
        "Supervivencia: Sin fin",
    },
    /* ITALIAN */
    {
        "Plants vs. Zombies",
        "Avventura",
        "Minigiochi",
        "Opzioni",
        "Esci",
        "Lingua",
        "Volume",
        "Controlli",
        "Indietro",
        "Continua",
        "Riavvia livello",
        "Chiudi",
        "Sole",
        "Ondata",
        "Zombi in arrivo...",
        "Scegli le tue piante",
        "Selezionate",
        "A: selezionare  |  START: iniziare",
        "Inizia",
        "Livello completato!",
        "Partita finita",
        "Gli zombi sono entrati...",
        "Gran Problema Piccolo Zombi",
        "Zombi Agile Zombi Veloce",
        "Colonna Come Vedi",
        "Invisi-spettro",
        "Ultima Resistenza",
        "ZomBotania",
        "Bowling Noce",
        "Slot Machine",
        "Piove semi",
        "Beghouled",
        "Vedere Stelle",
        "Zombiquarium",
        "Beghouled Twist",
        "Combattimento Portali",
        "Bobsled Bonanza",
        "Colpisci lo Zombi",
        "ZomBotania 2",
        "Bowling Noce 2",
        "Festa Pogo",
        "La Vendetta del Dr. Zomboss",
        "Sopravvivenza",
        "Sopravvivenza: Giorno (Facile)",
        "Sopravvivenza: Giorno (Difficile)",
        "Sopravvivenza: Notte (Facile)",
        "Sopravvivenza: Notte (Difficile)",
        "Sopravvivenza: Piscina (Facile)",
        "Sopravvivenza: Piscina (Difficile)",
        "Sopravvivenza: Nebbia (Facile)",
        "Sopravvivenza: Nebbia (Difficile)",
        "Sopravvivenza: Tetto (Facile)",
        "Sopravvivenza: Tetto (Difficile)",
        "Sopravvivenza: Infinita",
    },
    /* DUTCH */
    {
        "Plants vs. Zombies",
        "Avontuur",
        "Minigames",
        "Opties",
        "Afsluiten",
        "Taal",
        "Volume",
        "Bediening",
        "Terug",
        "Hervatten",
        "Level opnieuw",
        "Sluiten",
        "Zon",
        "Golf",
        "Zombies onderweg...",
        "Kies je planten",
        "Geselecteerd",
        "A: selecteren  |  START: beginnen",
        "Beginnen",
        "Level voltooid!",
        "Spel voorbij",
        "De zombies zijn binnen...",
        "Groot Probleem Kleine Zombie",
        "Zombie Behendig Zombie Snel",
        "Kolom Zoals Je Ziet",
        "Invisi-spook",
        "Laatste Strijd",
        "ZomBotanie",
        "Wall-nut Bowling",
        "Gokkast",
        "Het regent zaden",
        "Beghouled",
        "Sterren Zien",
        "Zombiquarium",
        "Beghouled Twist",
        "Portaal Gevecht",
        "Bobsled Bonanza",
        "Sla de Zombie",
        "ZomBotanie 2",
        "Wall-nut Bowling 2",
        "Pogo Feest",
        "Dr. Zomboss' Wraak",
        "Overleven",
        "Overleven: Dag (Makkelijk)",
        "Overleven: Dag (Moeilijk)",
        "Overleven: Nacht (Makkelijk)",
        "Overleven: Nacht (Moeilijk)",
        "Overleven: Zwembad (Makkelijk)",
        "Overleven: Zwembad (Moeilijk)",
        "Overleven: Mist (Makkelijk)",
        "Overleven: Mist (Moeilijk)",
        "Overleven: Dak (Makkelijk)",
        "Overleven: Dak (Moeilijk)",
        "Overleven: Oneindig",
    },
};

/* ---------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------- */

Language Locale_DetectSystemLanguage(void) {
    s32 sysLang = CONF_GetLanguage();
    switch (sysLang) {
        case CONF_LANG_ENGLISH:     return LANG_ENGLISH;
        case CONF_LANG_FRENCH:      return LANG_FRENCH;
        case CONF_LANG_GERMAN:      return LANG_GERMAN;
        case CONF_LANG_SPANISH:     return LANG_SPANISH;
        case CONF_LANG_ITALIAN:     return LANG_ITALIAN;
        case CONF_LANG_DUTCH:       return LANG_DUTCH;
        default:                    return LANG_ENGLISH;
    }
}

void Locale_SetLanguage(Language lang) {
    if (lang >= 0 && lang < LANG_COUNT)
        s_activeLang = lang;
}

Language Locale_GetLanguage(void) {
    return s_activeLang;
}

const char* Locale_LanguageName(Language lang) {
    switch (lang) {
        case LANG_ENGLISH: return "English";
        case LANG_FRENCH:  return "Francais";
        case LANG_GERMAN:  return "Deutsch";
        case LANG_SPANISH: return "Espanol";
        case LANG_ITALIAN: return "Italiano";
        case LANG_DUTCH:   return "Nederlands";
        default:           return "?";
    }
}

int Locale_LanguageCount(void) {
    return LANG_COUNT;
}

const char* Locale_Str(StringID id) {
    if (id < 0 || id >= STR_COUNT)
        return "?";
    return s_table[s_activeLang][id];
}
