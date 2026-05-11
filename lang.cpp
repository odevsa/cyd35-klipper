#include "lang.h"
#include "config.h"
#include <string.h>

// ─────────────────────────────────────────────────────
// All language tables
// ─────────────────────────────────────────────────────
static const LangStrings L_EN = {
    "HOME",   "PRINT",  "TEMPS",   "MOVE",
    "IDLE",   "STANDBY","PRINTING","PAUSED","COMPLETE","ERROR","OFFLINE",
    "CONNECTED", "OFFLINE",
    "Hotend", "Bed",    "Elapsed", "Remaining", "Height", "Fan",
    "PAUSE",  "RESUME", "CANCEL",  "E-STOP",    "RESTART",
    "Home ALL","XY Home","Z Home",
    "No File", "No active print", "No Preview", "OFF",
    "PRINTER OFFLINE",
    "Turn on your printer.",
    "Connecting to WiFi...",
    "WiFi failed - running offline",
    "Waiting for connection...",
    "FILES", "PRINT", "Print?",
    "Choose File", "File Size", "Print Time", "Filament", "Material", "Loading..."
};

static const LangStrings L_ZH_CN = {
    "JIBEN",  "DAYIN",  "WENDU",   "YIDONG",
    "DAIJI",  "DAIJI",  "DAYIN ZH","ZANTING","WANCHENG","CUOWU","LIXIAN",
    "YIJIE",  "LIXIAN",
    "Hotend", "Chuang", "Shijian", "Shengyu",  "Gaodu",  "Fengshan",
    "ZANTING","JIXU",   "QUXIAO",  "JI TING",  "ZHONGQI",
    "Home ALL","XY Home","Z Home",
    "Wu wenjian", "Wu dayin renwu", "Wu yulan", "GUANBI",
    "DAYINJI LIXIAN",
    "Qing kaiji dayinji.",
    "Lian jie WiFi...",
    "WiFi shibai - lixian",
    "Deng dai lianjie...",
    "WENJIAN", "DAYIN", "Dayin?",
    "Xuan Wj", "Wenjian Daxiao", "Shijian", "Siliao", "Cailiao", "Jiazai..."
};

static const LangStrings L_ES = {
    "INICIO", "IMPRIMIR","TEMP",   "MOVER",
    "INACTIVO","ESPERA", "IMPRIMIENDO","PAUSADO","COMPLETO","ERROR","OFFLINE",
    "CONECTADO","OFFLINE",
    "Hotend", "Cama",   "Tiempo",  "Restante", "Altura", "Ventilador",
    "PAUSAR", "REANUDAR","CANCELAR","PARADA",   "REINICIAR",
    "Home TODO","XY Home","Z Home",
    "Sin archivo", "Sin trabajo", "Sin vista previa", "APAGAR",
    "IMPRESORA OFFLINE",
    "Encienda la impresora.",
    "Conectando WiFi...",
    "Fallo WiFi - offline",
    "Esperando conexion...",
    "ARCHIVOS", "IMPRIMIR", "Imprimir?",
    "Elegir archivo", "Tamaño", "Tiempo Impresión", "Filamento", "Material", "Cargando..."
};

static const LangStrings L_FR = {
    "ACCUEIL","IMPRIMER","TEMP",   "DEPLACER",
    "ATTENTE","VEILLE",  "IMPRESSION","EN PAUSE","TERMINE","ERREUR","HORS LIGNE",
    "CONNECTE","HORS LIGNE",
    "Buse",   "Lit",    "Ecoule",  "Restant",  "Hauteur", "Ventilateur",
    "PAUSE",  "REPRENDRE","ANNULER","ARR. URG.", "REDEMARRER",
    "Home Tout","XY Home","Z Home",
    "Aucun fich.", "Sans travail", "Pas d'apercu", "ARRETER",
    "IMP. HORS LIGNE",
    "Allumez l'imprimante.",
    "Connexion WiFi...",
    "WiFi echoue - hors ligne",
    "Attente connexion...",
    "FICHIERS", "IMPRIMER", "Imprimer?",
    "Choisir fichier", "Taille", "Temps Impression", "Filament", "Materiau", "Chargement..."
};

static const LangStrings L_PT_BR = {
    "INICIO", "IMPRIMIR","TEMP",   "MOVER",
    "OCIOSO", "STANDBY", "IMPRIMINDO","PAUSADO","CONCLUIDO","ERRO","OFFLINE",
    "CONECTADO","OFFLINE",
    "Bico",   "Mesa",   "Decorrido","Restante","Altura","Ventilador",
    "PAUSAR", "RETOMAR","CANCELAR","PARADA",  "REINICIAR",
    "Home Tudo","Home XY","Z Home Z",
    "Sem arquivo", "Sem impressao", "Sem preview", "DESLIGAR",
    "IMPRESSORA OFFLINE",
    "Ligue a impressora.",
    "Conectando ao WiFi...",
    "Falha no WiFi - operando offline",
    "Aguardando conexao...",
    "ARQUIVOS", "IMPRIMIR", "Imprimir?",
    "Escolher arquivo", "Tamanho do arquivo", "Tempo Impressão", "Filamento", "Material", "Carregando..."
};

static const LangStrings L_RU = {
    "NACHAL", "PECHAT", "TEMP",    "DVIZH",
    "V.POKOE","STANDBY","PECHAT",  "PAUZA",  "ZAVRS",  "OSHIBKA","OFLAYN",
    "PODKL.", "OFLAYN",
    "Hotend", "Stol",   "Vremya",  "Ostalos","Vysota", "Vent",
    "PAUZA",  "VOZOB",  "OTMENA",  "AVAR.OS.","PEREZAP.",
    "Home Vse","XY Home","Z Home",
    "Net fayla","Net pechati","Net nabros.", "OTKL",
    "PRINTER OFLAYN",
    "Vklyuchite printer.",
    "Podkl. k WiFi...",
    "Oshibka WiFi",
    "Ozhidaniye podkl...",
    "FAYLI", "PECHAT", "Pechatat?",
    "Vibr. Fayl", "Razmer", "Vremya Pechati", "Filament", "Material", "Zagruzka..."
};

static const LangStrings L_DE = {
    "START",  "DRUCKEN","TEMP",    "BEWEGEN",
    "LEERLAUF","STANDBY","DRUCKT", "PAUSIERT","FERTIG",  "FEHLER","OFFLINE",
    "VERBUNDEN","OFFLINE",
    "Hotend", "Bett",   "Verstr.", "Verblbd.","Hoehe",  "Luefter",
    "PAUSE",  "FORTF.", "ABBRCH.", "NOTAUS",  "NEUSTART",
    "Home Alle","XY Home","Z Home",
    "Keine Datei","Kein Druck","Keine Vorans.","AUS",
    "DRUCKER OFFLINE",
    "Drucker einschalten.",
    "Mit WiFi verbinden...",
    "WiFi fehlgesch. - offline",
    "Warte auf Verbindung...",
    "DATEIEN", "DRUCKEN", "Drucken?",
    "Datei Wahlen", "Dateigrosse", "Druckzeit", "Filament", "Material", "Laden..."
};

static const LangStrings L_IT = {
    "INIZIO", "STAMPA", "TEMP",    "MUOVI",
    "FERMO",  "STANDBY","IN STAMPA","IN PAUSA","FINITO","ERRORE","OFFLINE",
    "CONNESSO","OFFLINE",
    "Hotend", "Piatto", "Trascorso","Rimanente","Altezza","Ventola",
    "PAUSA",  "RIPRENDI","ANNULLA","ARRESTO",  "RIAVVIA",
    "Home Tutto","XY Home","Z Home",
    "Nessun file","Nessuna stampa","Nessuna antepr.","SPEGNI",
    "STAMPANTE OFFLINE",
    "Accendi la stampante.",
    "Connessione WiFi...",
    "WiFi fallito - offline",
    "Attesa connessione...",
    "FILE", "STAMPA", "Stampare?",
    "Scegli file", "Dimensione file", "Tempo Stampa", "Filamento", "Materiale", "Caricamento..."
};

static const LangStrings L_JA = {
    "HOMU",   "INSATSU","ONDO",    "IDO",
    "TAIKI",  "STANDBY","INSATSU CHU","ICHIJI TEISHI","KANRYO","ERAA","OFFLINE",
    "SETSUZOKU","OFFLINE",
    "Hotend", "Beddo",  "Keika",   "Nokori",   "Takasa", "Fan",
    "TEISHI", "SAIKAI", "TORIKESHI","KIN TEISHI","SAIKI DO",
    "Home Zenbu","XY Home","Z Home",
    "Fairu nashi","Insatsu nashi","Purebyu nashi","OFF",
    "PURINTA OFFLINE",
    "Purinta wo tsukete.",
    "WiFi ni setsuzoku...",
    "WiFi shippai - offline",
    "Setsuzoku machi...",
    "FAIRU", "INSATSU", "Insatsu?",
    "Fairu Sentaku", "Fairu Saizu", "Insatsu Jikan", "Firamento", "Zairyo", "Yomikomi..."
};

static const LangStrings L_KO = {
    "HOME",   "INSWE",  "ONDO",    "IDONG",
    "DAEGI",  "STANDBY","INSWEJUNG","ILSI JUNGJI","WANRYO","EREO","OFFLINE",
    "YEONSOL","OFFLINE",
    "Hotend", "Bed",    "Gyeongwa","Namsigan",  "Nopi",   "Fan",
    "JUNGJI", "JAEQAE", "CHISO",   "BIGIANG",   "JAESIJAK",
    "Home Jeonbu","XY Home","Z Home",
    "Pail eobseo","Inswe eomn","Miree eobseo","KKUDA",
    "PEURINTE OFFLINE",
    "Peurintereul kyeo.",
    "WiFi yeonsol...",
    "WiFi silpae - offline",
    "Yeonsol daegi...",
    "PAIEL", "INSWE", "Inswe halsu?",
    "Fail Seontaeg", "Pail Keogi", "Inswe Sigan", "Pailramenteu", "Jaeryo", "Rodingjung..."
};

// ─────────────────────────────────────────────────────
// Runtime language selector
// ─────────────────────────────────────────────────────
static const LangStrings* _selectLang() {
#ifndef LANG
    return &L_EN;
#else
    if (strcmp(LANG, "en")    == 0) return &L_EN;
    if (strcmp(LANG, "pt-BR") == 0) return &L_PT_BR;
    if (strcmp(LANG, "es")    == 0) return &L_ES;
    if (strcmp(LANG, "fr")    == 0) return &L_FR;
    if (strcmp(LANG, "de")    == 0) return &L_DE;
    if (strcmp(LANG, "it")    == 0) return &L_IT;
    if (strcmp(LANG, "ja")    == 0) return &L_JA;
    if (strcmp(LANG, "zh-CN") == 0) return &L_ZH_CN;
    if (strcmp(LANG, "ru")    == 0) return &L_RU;
    if (strcmp(LANG, "ko")    == 0) return &L_KO;
    return &L_EN; // fallback
#endif
}

const LangStrings L = *_selectLang();
