/*********************************************************************

jupiter.c: jupiter speech adapter.

Copyright (C) Karl Dahlke, 2014.
This software may be freely distributed under the GPL, general public license,
as articulated by the Free Software Foundation.
*********************************************************************/

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <locale.h>

#include <linux/vt.h>

#include "tp.h"

#define stringEqual !strcmp


/*********************************************************************
argv[0] is the absolute path of the running program, but only
if it was invoked with an absolute path. If invoked as jupiter,
and then down $PATH, then argv[0] is simply jupiter.
On many systems, $_ holds the absolute path, so let's try that.
*********************************************************************/
static const char *program_file;
static char **argvector;

/* Speech command structure, one instance for each jupiter command. */
struct cmd {
	const char *desc; // description
	const char brief[12]; // brief name for the command
	char cact; /* some kind of reading cursor action */
	char nonempty; // buffer must be nonempty and perhaps a valid cursor
	char alone; // must be alone in a command sequence
	char nextchar; // needs next key or string to complete command
};

/* the available speech commands */
static const struct cmd speechcommands[] = {
	{0,""}, // 0 is not a function
	{"clear buffer","clbuf"},
	{"visual cursor","cursor",1,1},
	{"start of buffer","sbuf",1,1},
	{"end of buffer","ebuf",1,1},
	{"start of line","sline",1,3},
	{"end of line","eline",1,3},
	{"start of word","sword",1,3},
	{"end of word","eword",1,3},
	{"left spaces","lspc",1,3},
	{"right spaces","rspc",1,3},
	{"back one character","back",1,3},
	{"forward one character","for",1,3},
	{"preivious row","prow",1,3},
	{"next row","nrow",1,3},
	{"reed the current character as a nato word","asword",1,3},
	{"reed the current character","char",1,3},
	{"reed the current character","capchar",1,3},
	{"current column number","colnum",1,3},
	{"reed the current word","word",1,3},
	{"start reeding","read",1,3},
	{"stop speaking","shutup",0,0,1},
	{"pass next character through","bypass",0,0,1},
	{"clear bighnary mode","clmode",0,0,0,1},
	{"set bighnary mode","stmode",0,0,0,1},
	{"toggle bighnary mode","toggle",0,0,0,1},
	{"search up","searchu",1,3,0,2},
	{"search down","searchd",1,3,0,2},
	{"set volume","volume",0,0,0,1},
	{"increase volume", "incvol"},
	{"decrease volume", "decvol"},
	{"set speed","speed",0,0,0,1},
	{"increase speed", "incspd"},
	{"decrease speed", "decspd"},
	{"set pitch","pitch",0,0,0,1},
	{"increase pitch", "incpch"},
	{"decrease pitch", "decpch"},
	{"set voice", "voice",0,0,0, 1},
	{"key binding","bind",0,0,1,2},
	{"last complete line","lcline",1,1},
	{"mark left", "markl",1,3},
	{"mark right", "markr",1,3, 0, 1},
	{"obsolete", "x@y`"},
	{"label", "label",1,3, 0, 1},
	{"jump", "jump",1, 1, 0, 1},
	{"restart the adapter","reexec",0,0,1},
	{"reload the config file","reload",0,0,1,2},
	{"dump buffer","dump",0,0,1},
	{"suspend the adapter","suspend",0,0,1},
	{"chromatic scale","step",0,0,0,2},
	{"char plus asword","casw",1,3},
	{"speak now","speak",0,0,0,2},
	{"current word length","wordlen",1,3},
	{0,""}
};

#define CMD_SUSPEND 48

/*********************************************************************
Strings that are folded into the output text.
There is one structure of strings for each language supported.
*********************************************************************/

struct OUTWORDS {
const char *usage;
const char *openConfig;
const char *openDriver;
const char *openSerial;
const char *execSoft;
const char *busyDriver;
const char *permDriver;
const char *makeDriver;
const char *configError[10];
const char *bufword;
const char *lineword;
const char *yesword;
const char *noword;
const char *consword;
const char *readyword;
const char *setvolword;
const char *louderword;
const char *softerword;
const char *setrateword;
const char *fasterword;
const char *slowerword;
const char *setpitchword;
const char *higherword;
const char *lowerword;
const char *helloword;
const char *reloadword;
const char *okword;
const char *markword;
const char *cutword;
const char *modeword;
const char *endword;
const char *inputword;
const char *errorword;
const char *topword;
const char *bottomword;
const char *outsideword;
const char *overflowword;
};

static const struct OUTWORDS const outwords[6] = {

{ /* no output words for the zero language */
0

},{ /* English */
"usage:  jupiter [-d] [-c configfile] synthesizer port\n"
"-d is daemon mode, run in background.\n"
"Synthesizer is: dbe = doubletalk external,\n"
"dte = dectalk external, dtp = dectalk pc,\n"
"bns = braille n speak, ace = accent, esp = espeakup.\n"
"port is 0 through 7 for the serial device, or u0 through u7 for usb serial.\n"
"jupiter tc    to test the configuration file.\n",
"cannot open config file %s\n",
"cannot open the device driver %s;\n%s.\n",
"cannot open the serial port %s\n",
"cannot execute command %s\n",
"Acsint can only be opened by one program at a time.\n"
"Another program is currently using the acsint device driver.\n",
"Check the permissions on /dev/vcsa and %s\n",
"Did you make %s character special,\n"
"and did you install the acsint module?\n",
{0, "syntax error",
"%s cannot be in the middle of a composite speech command",
"%s must be followed by a letter or digit",
"%s is not a recognized speech command",
"%s cannot be mixed with any other commands",
"dictionary word or replacement word is too long",
"too many words in the replacement dictionary",
"cannot leave a punctuation or unicode with no pronunciation",
"cannot set the pronunciation of a letter, digit, or low or high unicode",
},
"buffer", "line", "yes", "no", "console", "jupiter ready",
"set volume", "louder", "softer",
"set rate", "faster", "slower",
"set pitch", "higher", "lower",
"hello there", "reload", "o k",
"mark", "cut", "mode", "boundary", "input", "error",
"top", "bottom",
"outside",
"buffer overflow",

},{ /* German */

"Verwendung: jupiter [-d] [-c Konfiguration] Port-des-Synthesizers\n"
"-d startet im Dienstmodus im Hintergrund.\n"
"Synthesizer sind: dbe = doubletalk extern,\n"
"dte = dectalk extern, dtp = dectalk pc,\n"
"bns = braille n speak, ace = accent, esp = espeakup.\n"
"Port ist entweder 0 1 2 oder 3, für serielle Geräte.\n"
"Jupiter tc um die Konfigurationsdatei zu testen\n",
"kann Konfigurationsdatei %s nicht öffnen\n",
"kannn Gerätetreiber %s nicht öffnen;\n%s.\n",
"kann seriellen Port %s nicht öffnen\n",
"kann Kommando %s nicht ausführen\n",
"Acsint kann nur von einem Programm gleichzeitig geöffnet werden.\n"
"Ein anderes Programm verwendet bereits den Acsint-Gerätetreiber.\n",
"Überprüfen Sie die Berechtigungen für /dev/vcsa und %s.\n",
"Ist von Ihnen %s als „Character Special” gekennzeichnet worden,\n"
"und haben Sie das Acsint-Modul installiert?\n",
{0, "Xyntaxfehler",
"%s kann nicht in der Mitte eines zusammengesetzten Sprachkommandos sein",
"auf %s muss ein Buchstabe oder eine Zahl folgen",
"%s ist kein bekanntes Sprachkommando",
"%s kann nicht mit anderen Kommandos vermischt werden",
"Wörterbuchbegriff oder Ersatzbegriff ist zu lang",
"zu viele Begriffe im Ersetzungswörterbuch",
"unmöglich einen Unicodepunkt oder Satzzeichen ohne Aussprache zu belassen",
"unmöglich die Aussprache von Zeichen, Ziffern oder hoch/niedrig Unicodepunkten zu setzen",
},
"Puffer", "Zeile", "ja", "nein", "Konsole", "jupiter bereit",
"ändere Lautstärrke", "lauter", "leiser",
"ändere Geschwindigkeit", "schneller", "langsamer",
"setze Tonhöhe", "höher", "niedriger",
"hallo", "erneut laden", "ok",
"Markieren", "ausschneiden", "Modus", "Ende", "Kommando", "Fehler",
"Anfang", "Ende",
"draussen",
"buffer overflow",

},{ /* Brazilian Portuguese */

"uso: jupiter [-d] [-c arq. de config.] sintetizador porta\n"
"-d é modo daemon, roda em segundo plano.\n"
"Sintetizador é: dbe = doubletalk externo,\n"
"dte = dectalk externo, dtp = dectalk pc,\n"
"bns = braille n speak, ace = accent, esp = espeakup.\n"
"porta é 0 1 2 ou 3, para o dispositivo serial.\n"
"jupiter tc    para testar o arquivo de configuração.\n",
"impossível abrir arquivo de configuração %s\n",
"impossível abrir arquivo de dráiver do dispositivo %s;\n%s.\n",
"impossível abrir arquivo de porta do serial %s\n",
"impossível executar comando %s\n",
"O acsint só pode ser aberto por um programa de cada vez.\n"
"Outro programa está usando o dispositivo acsint no momento.\n",
"Verifique as permissões para /dev/vcsa e %s\n",
"Já criou o dispositivo especial de caractere %s\n"
"e instalou o módulo acsint?\n",
{0, "erro de sintaxe",
"%s não pode estar no meio dum comando de fala composto",
"%s tem que ser seguido por uma letra ou dígito",
"%s não é um comando de fala conhecido",
"%s não pode ser misturado com outros comandos",
"palavra original ou palavra substituta longa demais",
"palavras demais no dicionário de substituição",
"não é possível deixar uma pontuação ou unicode sem pronúncia",
"não é possível determinar a pronúncia duma letra, algarismo, ou unicode baixo ou alto",
},
"buffer", "linha", "sim", "não", "console", "jupiter ativado",
"determinar volume", "mais alto", "mais baixo",
"determinar velocidade", "mais rápido", "mais lento",
"determinar tom", "mais alto", "mais baixo",
"olá", "recarregar", "o k",
"marcar", "cortar", "modo", "limite", "inserir", "erro",
"topo", "fundo",
"outside",
"buffer overflow",

},{ /* French */

"usage:  jupiter [-d] [-c configfile] synthesizer port\n"
"-d est le mode démon, lancé en tâche de fond.\n"
"Synthesizer vaut: dbe = doubletalk externe,\n"
"dte = dectalk externe, dtp = dectalk pc,\n"
"bns = braille n speak, ace = accent, esp = espeakup.\n"
"port vaut 0 1 2 ou 3, pour le périphérique série.\n"
"jupiter tc    pour tester le fichier de configuration.\n",

"impossible d'ouvrir fichier de configuration %s\n",
"impossible d'ouvrir pilote %s;\n%s.\n",
"impossible d'ouvrir port série %s\n",
"impossible de lancer commande %s\n",

"Acsint ne peut être ouvert que par un seul programme à la fois.\n"
"Un autre programme utilise déjà le pilote acsint.\n",
"Vérifier les permissions de /dev/vcsa et %s\n",
"Avez-vous créé %s en tant que fichier spécial en mode caractère,\n"
"et avez-vous installé le module acsint ?\n",
{0, "erreur de syntaxe",
"%s ne peut pas se trouver au milieu d'une commande vocale composite",
"%s doit être suivi d'une lettre ou d'un chiffre",
"%s n'est pas une commande vocale connue",
"%s ne peut être associée à aucune autre commande",
"le mot de dictionnaire ou le mot de remplacement est trop long",
"trop de mots dans le dictionnaire de remplacement",
"impossible de laisser un caractere ponctuation ou unicode sans prononciation",
"impossible de modifier la prononciation d'une letter, d'un chiffre, ou d'un unicode bas ou haut",
},
"tampon", "ligne", "oui", "non", "console", "jupiter prêt",
"volume", "plus fort", "moins fort",
"vitesse", "plus vite", "moins vite",
"pitch", "plus haut", "plus bas",
"bonjour", "rechargement", "o k",
"marquer", "couper", "mode", "limite", "entrer", "erreur",
"début", "fin",
"outside",
"buffer overflow",

},{ /* Slovak */
"Použitie:  jupiter [-d] [-c súbor] hlas.výstup port\n"
"-d režim démona, beh na pozadí.\n"
"hlasový výstup je: dbe = doubletalk external,\n"
"dte = dectalk external, dtp = dectalk pc,\n"
"bns = braille n speak, ace = accent, esp = espeakup.\n"
"port je 0 1 2 alebo 3, podľa sériového zariadenia.\n"
"jupiter tc    test konfiguračného súboru.\n",
"Nie je možné otvoriť konfiguračný súbor %s\n",
"Nie je možné otvoriť ovládač zariadenia %s;\n%s.\n",
"Nie je možné otvoriť sériový port %s\n",
"Nie je možné vykonať príkaz %s\n",
"Zariadenie acsint môže byť otvorené len jediným programom.\n"
"Iný program práve používa ovládač zariadenia acsint.\n",
"Overte oprávnenia pre /dev/vcsa a %s\n",
"Nastavili ste špeciálne znakové zariadenie %s,\n"
"a nainštalovali ste modul acsint?\n",
{0, "syntaktická chyba",
"%s nemôže byť v strede zloženého rečového príkazu",
"%s musí byť nasledované písmenom alebo číslicou",
"%s nie je známy rečový príkaz",
"%s nemôže byť pomiešaný s ostatnými príkazmi",
"slovo slovníka alebo nahrádzajúce slovo je priveľmi dlhé",
"veľmi veľa slov v slovníku nahradiť",
"nie je možné nechať interpunkčný znak alebo znak v systéme unicode bez výslovnosti",
"Nie je možné nastaviť výslovnosť písmena, číslice, alebo znaku v systéme unicode",
},
"buffer", "riadok", "áno", "nie", "konzola", "jupiter pripravený",
"nastaviť hlasitosť", "hlasnejšie", "tichšie",
"nastaviť tempo", "rýchlejšie", "pomalšie",
"nastaviť výšku", "nižšie", "vyššie",
"dobrý deň", "načítať znovu", "oukey",
"mark", "cut", "mode", "boundary", "input", "error",
"top", "bottom",
"outside",
"buffer overflow",

/* no more */

}};

static const struct OUTWORDS *o;

static void
usage(void)
{
fprintf(stderr, "%s", o->usage);
exit(1);
}

// derive a command code from its brief name
static int
cmdByName(const char *name)
{
	const char *t;
	const struct cmd *cmdp = &speechcommands[1];

	while(*(t = cmdp->brief)) {
		if(stringEqual(name, t)) return cmdp - speechcommands;
		++cmdp;
}

	return 0;
}

// Return the composite status of an atomic command.
//  1  must end the composite.
//  2  requires follow-on char.
//  4  requires follow-on string -- stands alone.
static unsigned
compStatus(int cmd)
{
	unsigned compstat;
	const struct cmd *cmdp = speechcommands + cmd;
compstat = cmdp->nextchar;
	if(cmdp->alone) compstat |= 12;
	return compstat;
}

static char last_atom[12];
static int cfg_syntax(char *s)
{
char *v = s;
char *s0 = s;
char *t;
char mustend = 0, nextchar = 0;
char c;
int cmd;
struct cmd *cmdp;
unsigned compstat;
int l;

	// look up each command designator
	while(c = *s) {
if(c == ' ' || c == '\t') { ++s; continue; }

if(nextchar == 2 && *s == '"')
t = strchr(++s, '"');
else
		t = strpbrk(s, " \t");
		if(t) *t = 0;

		if(nextchar == 1) {
			if(c < 0 || !isalnum(c) || s[1]) return -3;
*v++ = c;
			nextchar = 0;
} else if(nextchar == 2) {

l = strlen(s);
memmove(v, s, l);
v += l;
*v++ = '"';
nextchar = 0;
		} else {

		if(mustend) return -2;

			cmd = cmdByName(s);
strcpy(last_atom, s);
			if(!cmd) return -4;

			compstat = compStatus(cmd);
			if(compstat&8 && v > s0) return -5;
			if(compstat & 4) mustend = 1;
nextchar = (compstat & 3);
*v++ = cmd;
		}

		if(!t) break;
		s = ++t;
	}

*v = 0;
	return 0;
}

/* prepend directory /etc/jupiter */
#define SUPPORTLEN 200
static char jfile[SUPPORTLEN+20];
static void etcjup(const char *s)
{
if(*s == '/') {
strcpy(jfile, s);
} else {
strcpy(jfile, "/etc/jupiter/");
strcat(jfile, s);
}
}

static const char *acsdriver = "/dev/acsint";

/* cut&paste macros; preserve these across all reloads */
static char *cp_macro[26];
static char cutbuf[10000];

/* configure the jupiter system from a config file. */
static const char default_config[] = "/etc/jupiter/start.cfg";
static const char *start_config = default_config;

static char * cloneString(const char *s)
{
int l = strlen(s);
char *t = malloc(l+1);
strcpy(t, s);
return t;
}

static char modeValue;
static void runSpeechCommand(int input, const char *cmdlist);
static void
j_configure(const char *my_config, int docolon)
{
FILE *f;
char line[SUPPORTLEN];
char *s;
int i, lineno, rc;
char filename[SUPPORTLEN+20];

/* everything has been cleared; start with the cut&paste strings */
for(i=0; i<26; ++i) {
if(!cp_macro[i]) continue;
sprintf(cutbuf, "@%c<%s", 'a'+i, cp_macro[i]);
acs_line_configure(cutbuf, 0);
}

strcpy(filename, my_config);
f = fopen(filename, "r");
if(!f) {
fprintf(stderr, o->openConfig, filename);
return;
}

lineno = 0;
while(fgets(line, sizeof(line), f)) {
++lineno;

// strip off crlf
s = line + strlen(line);
if(s > line && s[-1] == '\n') --s;
if(s > line && s[-1] == '\r') --s;
*s = 0;

// special execute now code
if(line[0] == ':' && line[1] == ':') {
rc = cfg_syntax(line+2);
if(rc) goto syn_error;
else if(docolon) runSpeechCommand(0, line+2);
continue;
}

// include another file
if(line[0] == '<' && line[1] == '<') {
s = line + 2;
while(*s == ' ' || *s == '\t') ++s;
if(!*s) continue;
etcjup(s);
j_configure(jfile, docolon);
continue;
}

if(rc = acs_line_configure(line, cfg_syntax)) {
syn_error:
fprintf(stderr, "%s %s %d: ",
filename, o->lineword, lineno);
fprintf(stderr, o->configError[-rc], last_atom);
fprintf(stderr, "\n");
}
}

fclose(f);
}


/*********************************************************************
Set, clear, or toggle a binary mode based on the follow-on character.
Also generate the appropriate sound.
For instance, ^G beep if the character doesn't correspond
to a boolean mode in the system.
*********************************************************************/

static char soundsOn = 1; // sounds on, over all */
static char clickTTY = 1; // clicks accompany tty output
static char autoRead = 1; // read new text automatically
static char oneLine; /* read one line at a time */
static char ctrack; /* track visual cursor in screen mode */
static char overrideSignals = 0; // don't rely on cts rts etc
static char keyInterrupt;
static char goRead, goRead2; /* read the next sentence */
/* for cut&paste */
#define markleft acs_mb->marks[26]
static unsigned int *markright;
static char screenMode = 0;
static char smlist[MAX_NR_CONSOLES+1];
static char *cfglist[MAX_NR_CONSOLES+1];
static char jdebug;
static char cc_buffer = 0; // control chars in the buffer
static char echoMode; // echo keys as they are typed
static char suspended;
static char suspendClicks;
static char suspendlist[MAX_NR_CONSOLES+1];
/* buffer for cut&paste */

static void binmode(int action, int c, int quiet)
{
	char *p;
static int save_postprocess;

	switch(c) {
	case 'e': p = &echoMode; break;
	case 't': p = &clickTTY; break;
	case 'n': p = &soundsOn; break;
	case 'a': p = &autoRead; break;
	case '1': p = &oneLine; break;
	case 'o': p = &overrideSignals; break;
case 's': markleft = 0; p = &screenMode; break;
	case 'c': p = &cc_buffer; break;
	case 'd': p = &jdebug; break;
	case 'l': p = &tp_readLiteral; break;
	case 'm': p = &tp_numStyle; break;
case 'i': p = &keyInterrupt; break;
	default: acs_bell(); return;
} // switch

	if(action == 0) *p = 0;
	if(action == 1) *p = 1;
	if(action == 2) *p ^= 1;
	modeValue = *p;
	if(!quiet) {
if(soundsOn || c == 'n')
acs_tone_onoff(*p);
else
acs_say_string(*p ? o->yesword : o->noword);
}

switch(c) {
case 'n':
acs_sounds(*p);
/* If turning sounds on, then the previous tone didn't take. */
if(!quiet && *p) acs_tone_onoff(1);
break;
case 't':
acs_tty_clicks(*p);
break;
case 'o': acs_serial_flow(1-*p); break;
case 's':
if(acs_screenmode(*p)) {
/* failure to switch to screen mode */
screenMode = 0;
acs_buzz();
}
smlist[acs_fgc] = screenMode;
ctrack = 1;
/* this line is really important; don't leave the temp cursor in the other world. */
acs_cursorset();
break;
case 'd':
acs_debug = jdebug;
if(jdebug) truncate("/var/log/acslog", 0);
break;
case 'c':
if(*p) save_postprocess = acs_postprocess, acs_postprocess = 0;
else acs_postprocess = save_postprocess;
break;
} // switch
}

/*********************************************************************
An event is interrupting speech.
Key command, echoed character, console switch.
Stop any reading in progress.
*********************************************************************/

static void interrupt(void)
{
acs_rb = 0;
goRead = 0;
if(acs_stillTalking())
acs_shutup();
}

#define readNextMark acs_rb->marks[27]

static void
readNextPart(void)
{
int gsprop;
int i;
unsigned int *end; /* the end of the sentence */
unsigned int first; /* first character of the sentence */
static int flip = 1; /* flip between two ranges of numbers */

acs_refresh(); /* whether we need to or not */
/* on console switch acs_rb could drop to 0 */
	if(!acs_rb) return;

if(readNextMark) {
/* could be past the buffer */
if(readNextMark >= acs_rb->end) {
readNextMark = 0;
acs_rb = 0;
return;
}
acs_rb->cursor = readNextMark;
readNextMark = 0;
}

if(!acs_rb->cursor) {
// lots of text has pushed the reading cursor off the edge.
if(soundsOn) acs_buzz();
else acs_say_string(o->overflowword);
acs_rb = 0;
return;
}

gsprop = ACS_GS_REPEAT;
if(oneLine | soundsOn)
gsprop |= ACS_GS_STOPLINE;
else
gsprop |= ACS_GS_NLSPACE;

top:
/* grab something to read */
acs_log("nextpart 0x%x\n", acs_rb->cursor[0]);
tp_in->buf[0] = 0;
tp_in->offset[0] = 0;
acs_getsentence(tp_in->buf+1, 120, tp_in->offset+1, gsprop);

if(!tp_in->buf[1]) {
/* Empty sentence, nothing else to read. */
acs_log("empty done\n");
acs_rb = 0;
return;
}

first = tp_in->buf[1];
if(first == '\n' || first == '\7') {
/* starts out with newline or bell, which is usually associated with a sound */
/* This will swoop/beep with clicks on, or say the word newline or bell with clicks off */
speakChar(tp_in->buf[1], 1, soundsOn, 0);

if(oneLine && first == '\n') {
acs_log("newline done\n");
acs_rb = 0;
return;
}

/* Find the next token/offset, which should be the next character */
for(i=2; !tp_in->offset[i]; ++i)  ;
acs_rb->cursor += tp_in->offset[i];
/* but don't leave it there if you have run off the end of the buffer */
if(acs_rb->cursor >= acs_rb->end) {
acs_rb->cursor = acs_rb->end-1;
acs_log("eof done\n");
acs_rb = 0;
return;
}

/* The following line is bad if there are ten thousand bells, no way to interrupt */
/* I'll deal with that case later. */
goto top;
}

if(jdebug) {
char *w = acs_uni2utf8(tp_in->buf+1);
if(w) {
acs_log("insentence %s\n", w);
free(w);
}
}

tp_in->len = 1 + acs_unilen(tp_in->buf+1);
/* If the sentence runs all the way to the end of the buffer,
 * then we might be in the middle of printing a word.
 * We don't want to read half the word, then come back and refresh
 * and read the other half.  So back up to the beginning of the word.
 * Nor do we want to hear the word return, when newline is about to follow.
 * Despite this code, it is still possible to hear part of a word,
 * or the cr in crlf, if the output is delayed for some reason. */
end = tp_in->buf + tp_in->len - 1;
if(*end == '\r') {
if(tp_in->len > 2 && tp_in->offset[tp_in->len-1])
*end = 0, --tp_in->len;
} else if(acs_isalnum(*end)) {
for(--end; *end; --end)
if(!acs_isalnum(*end)) break;
if(*end++ && tp_in->offset[end-tp_in->buf]) {
*end = 0;
tp_in->len = end - tp_in->buf;
}
}

prepTTS();

/* Cut the text at a logical sentence, as indicated by newline.
 * If newline wasn't already present in the input, this has been
 * set for you by prepTTS. */
for(end=tp_out->buf+1; *end; ++end)
if(*end == '\n' || *end == '\7') break;
*end = 0;
tp_out->len = end - tp_out->buf;

/* An artificial newline, inserted by prepTTS to denote a sentence boundary,
 * won't have an offset.  In that case we need to grab the next one. */
i = tp_out->len;
while(!tp_out->offset[i]) ++i;
tp_out->offset[tp_out->len] = tp_out->offset[i];

readNextMark = acs_rb->cursor + tp_out->offset[tp_out->len];
//flip = 51 - flip;
acs_say_indexed(tp_out->buf+1, tp_out->offset+1, flip);
}

/* index mark handler, read next sentence if we finished the last one */
static void imark_h(int mark, int lastmark)
{
/* Not sure how we would get here if we weren't reading, but just in case */
if(!acs_rb) return;

if(mark == lastmark)
readNextPart();
}


/*********************************************************************
Dump the tty buffer into a file.
But first we need to convert it from unicode to utf8.
The conversion routine is part of the acsint bridge layer.
*********************************************************************/

static int dumpBuffer(void)
{
int fd, l, n;
char *utf8 =  acs_uni2utf8(acs_mb->start);
if(!utf8) return -1;
sprintf(shortPhrase, "/tmp/buf%d", acs_fgc);
fd = open(shortPhrase, O_WRONLY|O_CREAT|O_TRUNC, 0666);
if(fd < 0) {
free(utf8);
return -1;
}
l = strlen(utf8);
n = write(fd, utf8, l);
close(fd);
free(utf8);
return (n < l ? -1 : 0);
}



/*********************************************************************
Suspend or unsuspend the adapter.
This is invoked by a speech command,
or called when you switch consoles.
*********************************************************************/

static void suspend(void)
{
static const char suspendCommand[] = { CMD_SUSPEND, 0};
acs_suspendkeys(suspendCommand);
acs_rb = 0;
suspended = 1;
suspendClicks = soundsOn;
if(soundsOn) {
soundsOn = 0;
acs_sounds(0);
}
}

static void unsuspend(void)
{
acs_reset_configure();
etcjup(cfglist[acs_fgc]);
j_configure(jfile, 0);
if(suspendClicks) {
soundsOn = 1;
acs_sounds(1);
}
suspended = 0;
}


static void
chromscale(const char *scalefile)
{
FILE *f;
int f1, f2, step, duration;
f = fopen(scalefile, "r");
if(!f) return;
fscanf(f, "%d,%d,%d,%d",
&f1, &f2, &step, &duration);
fclose(f);
acs_scale(f1, f2, step, duration);
}


/*********************************************************************
Execute the speech command.
The argument is the command list, null-terminated.
*********************************************************************/

static const char *cmd_resume;
static void runSpeechCommand(int input, const char *cmdlist)
{
	const struct cmd *cmdp;
	char suptext[SUPPORTLEN]; /* supporting text */
static 	char lasttext[SUPPORTLEN]; /* supporting text */
	char support; /* supporting character */
	int i, n;
	int asword, asword2, quiet, rc, gsprop;
	unsigned int c;
	char cmd;
const char *t;
char *cut8;
char *yn;

modeValue = 1;
acs_log("runSpeech\n");
interrupt();
cmd_resume = 0;

if(screenMode & ctrack)
acs_mb->cursor = acs_mb->v_cursor;

acs_cursorset();

top:
	cmd = *cmdlist;
if(cmd) ++cmdlist;
// range check
if((unsigned char)cmd >= sizeof(speechcommands)/sizeof(struct cmd))
goto error_bell;
	cmdp = &speechcommands[cmd];
acs_log("cmd %s\n", cmdp->brief);
if(cmdp->cact) ctrack = 0;
asword = asword2 = 0;

if(suspended && cmd != CMD_SUSPEND)
return;

	/* some comands are meaningless when the buffer is empty
	 * or the cursor has rolled off the back. */
	if(cmdp->nonempty&2 && !acs_cursorvalid()) goto error_buzz;
if(cmdp->nonempty&1 && acs_mb->end == acs_mb->start) goto error_bound;

	support = 0;
	if(cmdp->nextchar == 1) {
		if(*cmdlist) support = *cmdlist++;
else{
acs_click();
if(!soundsOn) acs_say_string(o->modeword);
if(acs_get1char(&support)) goto error_bell;
}
	}

suptext[0] = 0;
if(cmdp->nextchar == 2) {
if(*cmdlist) {
for(i=0; *cmdlist && *cmdlist != '"'; ++i, ++cmdlist) {
if(i == sizeof(suptext)-1) goto error_bound;
suptext[i] = *cmdlist;
}
suptext[i] = 0;
if(*cmdlist) ++cmdlist;
} else {
if(soundsOn) acs_tone_onoff(0);
else acs_say_string(o->inputword);
if(acs_keystring(suptext, sizeof(suptext), ACS_KS_DEFAULT)) return;
}
}

quiet = ((!input)|*cmdlist);

	/* perform the requested action */
	switch(cmd) {
	case 0: /* command string is finished */
		acs_cursorsync();
if(!quiet) acs_click();
		return;

	case 1:
markleft = 0;
if(screenMode) goto error_bell;
acs_clearbuf();
if(!quiet) acs_tone_onoff(0);
acs_cursorset();
break;

	case 2: /* locate visual cursor */
		if(!screenMode) break;
acs_mb->cursor = acs_mb->v_cursor;
acs_cursorset();
ctrack = 1;
		break;

	case 3: acs_startbuf();
// if(!(soundsOn|quiet)) acs_say_string(o->topword);
break;

	case 4: acs_endbuf();
// if(!(soundsOn|quiet)) acs_say_string(o->bottomword);
break;

	case 5: acs_startline(); break;

	case 6: acs_endline(); break;

	case 7: acs_startword(); break;

	case 8: acs_endword(); break;

	case 9: acs_lspc(); break;

	case 10: acs_rspc(); break;

	case 11: if(!acs_back()) goto error_bound; break;

	case 12: if(!acs_forward()) goto error_bound; break;

	case 13: /* up a row */
		n = acs_startline();
		if(!acs_back()) goto error_bound;
		acs_startline();
		for(i=1; i<n; ++i) {
			if(acs_getc() == '\n') goto error_bell;
acs_forward();
		}
		break;

	case 14: /* down a row */
		n = acs_startline();
acs_endline();
		if(!acs_forward()) goto error_bound;
		for(i=1; i<n; ++i) {
			if(acs_getc() == '\n') goto error_bell;
			if(!acs_forward()) goto error_bound;
		}
		break;

/* read character, or cap character, or word for character */
	case 15: asword = 2; goto letter;
case 16: case 17: asword = 1; /* fall through */
letter:
acs_cursorsync();
		speakChar(c = acs_getc(), 1, soundsOn, asword);
if(!asword2) break;
if(c < 0x80) {
if(c < 'A' || c > 'Z' && c < 'a' || c > 'z') break;
asword2 = 0, asword = 2;
goto letter;
}
if(!acs_getpunc(c)) break;
asword2 = 0, asword = 2;
goto letter;

	case 18: /* read column number */
		acs_cursorsync();
		n = acs_startline();
		acs_cursorset();
		sprintf(shortPhrase, "%d", n);
		acs_say_string_uc(prepTTSmsg(shortPhrase));
		break;

	case 19: /* just read one word */
acs_cursorsync();
c = acs_getc();
if(c <= ' ') goto letter;
acs_startword();
acs_cursorsync();
gsprop = ACS_GS_STOPLINE | ACS_GS_REPEAT | ACS_GS_ONEWORD;
tp_in->buf[0] = 0;
tp_in->offset[0] = 0;
acs_rb = acs_mb;
acs_getsentence(tp_in->buf+1, WORDLEN, tp_in->offset+1, gsprop);
		tp_in->len = acs_unilen(tp_in->buf+1) + 1;
acs_rb->cursor += tp_in->offset[tp_in->len] - 1;
acs_cursorset();
tp_oneSymbol = 1;
char wordcase = acs_wordcase(tp_in->buf + 1);
char saveLiteral = tp_readLiteral;
tp_readLiteral = 1;
prepTTS();
tp_readLiteral = saveLiteral;
tp_oneSymbol = 0;
acs_rb = 0;
if(wordcase == 1) acs_say_string_n("cap ");
if(wordcase == 2) acs_say_string_n("allcap ");
if(wordcase == 3) acs_say_string_n("mixcap ");
		acs_say_string_uc(tp_out->buf+1);
break;

	case 20: /* start continuous reading */
if(!quiet) acs_click();
startread:
		/* We always start reading at the beginning of a word */
acs_startword();
		acs_cursorsync();
acs_rb = acs_mb;
/* start at the cursor, not at some leftover nextMark */
readNextMark = 0;
readNextPart();
if(!acs_rb) break;
if(!*cmdlist) break;
cmd_resume = cmdlist;
		return;

	case 21: acs_shutup(); break;

	case 22: acs_bypass(); break;

	/* clear, set, and toggle binary modes */
	case 23: binmode(0, support, quiet); goto top;
	case 24: binmode(1, support, quiet); goto top;
	case 25: binmode(2, support, quiet); goto top;

	case 26: asword = 1; /* search backwards */
	case 27: /* search forward */
		if(*suptext)
			strcpy(lasttext, suptext);
else strcpy(suptext, lasttext);
		if(!*suptext) goto error_bell;
		if(!acs_bufsearch(suptext, asword, oneLine)) goto error_bound;
if(*cmdlist) break; // more to do
		acs_cursorsync();
if(!quiet) acs_cr();
		if(!oneLine) {
			acs_say_string(o->okword);
acs_mb->cursor -= (strlen(suptext)-1);
			return;
		}
		/* start reading at the beginning of this line */
acs_startline();
		goto startread;

	case 28: /* volume */
		if(!isdigit(support)) goto error_bell;
rc = acs_setvolume(support-'0');
t = o->setvolword;
speechparam:
if(rc == -1) goto error_bound;
if(rc == -2) goto error_bell;
		if(quiet) break;
		acs_say_string(t);
		break;

	case 29: /* inc volume */
rc = acs_incvolume();
t = o->louderword;
goto speechparam;

	case 30: /* dec volume */
rc = acs_decvolume();
t = o->softerword;
goto speechparam;

	case 31: /* speed */
		if(!isdigit(support)) goto error_bell;
rc = acs_setspeed(support-'0');
t = o->setrateword;
goto speechparam;

	case 32: /* inc speed */
rc = acs_incspeed();
t = o->fasterword;
goto speechparam;

	case 33: /* dec speed */
rc = acs_decspeed();
t = o->slowerword;
goto speechparam;

	case 34: /* pitch */
		if(!isdigit(support)) goto error_bell;
rc = acs_setpitch(support-'0');
t = o->setpitchword;
goto speechparam;

	case 35: /* inc pitch */
rc = acs_incpitch();
t = o->higherword;
goto speechparam;

	case 36: /* dec pitch */
rc = acs_decpitch();
t = o->lowerword;
goto speechparam;

	case 37: /* set voice */
		if(support < '0' || support > '9') goto error_bell;
		rc = acs_setvoice(support-'0');
t = o->helloword;
goto speechparam;

	case 38: /* key binding */
if(acs_line_configure(suptext, cfg_syntax))
goto error_bell;
if(!quiet) acs_cr();
		return;

	case 39: /* last complete line */
acs_endbuf();
acs_cursorsync();
		if(screenMode) acs_back();
	while(1) {
		c = acs_getc();
		if(c == '\n') asword = 1;
		if(!acs_isspace(c) && asword) break;
		if(!acs_back()) goto error_bound;
	}
	break;

case 40: /* mark left */
if(!input) goto error_bell;
acs_cursorsync();
markleft = acs_mb->cursor;
if(!quiet) {
if(soundsOn) acs_tone_onoff(0);
else acs_say_string(o->markword);
}
break;

case 41: /* mark right */
if(!input) goto error_bell;
if(support < 'a' || support > 'z') goto error_bell;
if(!markleft) goto error_bell;
n = 0;
cutbuf[n++] = '@';
cutbuf[n++] = support;
cutbuf[n] = 0;
acs_line_configure(cutbuf, 0);
cutbuf[n++] = '<';
acs_cursorsync();
markright = acs_mb->cursor;
if(markright < markleft) goto error_bound;
++markright;
i = markright - markleft;
if(i + n >= sizeof(cutbuf)) goto error_bound;
c = *markright; // save this value
*markright = 0;
cut8 = acs_uni2utf8(markleft);
*markright = c; // put it back
if(!cut8) goto error_bell;
if(n + strlen(cut8) >= sizeof(cutbuf)) { free(cut8); goto error_bound; }
i = support - 'a';
if(cp_macro[i]) free(cp_macro[i]);
cp_macro[i] = cut8;
strcpy(cutbuf + n, cut8);
/* Stash it in the macro */
if(acs_line_configure(cutbuf, cfg_syntax) < 0) {
// this should never happen
free(cut8);
cp_macro[i] = 0;
goto error_bell;
}
markleft = 0;
if(!quiet) {
if(soundsOn) acs_tone_onoff(0);
else acs_say_string(o->cutword);
}
return;

#if 0
	case 42: /* set echo */
		if(support < '0' || support > '4') goto error_bell;
echoMode = support - '0';
		if(input && !*cmdlist) {
static const char * const echoWords[] = { "off", "letters", "words", "letters pause", "words pause"};
acs_say_string(echoWords[echoMode]);
}
		break;
#endif

case 43: /* set a marker in the tty buffer */
if(support < 'a' || support > 'z') goto error_bell;
acs_cursorsync();
if(!acs_mb->cursor) goto error_bell;
acs_mb->marks[support-'a'] = acs_mb->cursor;
if(!quiet) acs_tone_onoff(0);
break;

case 44: /* jump to a preset marker */
if(support < 'a' || support > 'z') goto error_bell;
if(!acs_mb->marks[support-'a']) goto error_bell;
acs_mb->cursor = acs_mb->marks[support-'a'];
acs_cursorset();
if(!quiet) acs_tone_onoff(0);
break;

case 45: /* reexec */
acs_buzz();
acs_sy_close();
acs_close();
// in case the ao system did not set O_CLOEXEC
if(aodev) {
ao_close(aodev);
ao_shutdown();
}
usleep(700000);
if(strchr(program_file, '/'))
	execv(program_file, argvector);
execvp(program_file, argvector);
// should never get here
puts("\7\7\7");
exit(1);

case 46: /* reload config file */
if(!*suptext) goto error_bell;
etcjup(suptext);
if(access(jfile, 4)) goto error_bell;
if(cfglist[acs_fgc]) free(cfglist[acs_fgc]);
cfglist[acs_fgc] = cloneString(suptext);
if(!quiet) {
acs_cr();
acs_say_string(o->reloadword);
}
acs_reset_configure();
j_configure(jfile, 1);
return;

case 47: /* dump tty buffer to a file */
if(dumpBuffer()) goto error_bell;
if(!quiet) {
acs_cr();
sprintf(shortPhrase, "%s %d",
o->bufword, acs_fgc);
		acs_say_string_uc(prepTTSmsg(shortPhrase));
}
return;

case 48: /* suspend */
if(suspended) {
static const short unsuspendNotes[] = {
500, 10, 530, 10, 560, 10, 0};
unsuspend();
acs_notes(unsuspendNotes);
} else {
static const short suspendNotes[] = {
560, 10, 530, 10, 500, 10, 0};
acs_notes(suspendNotes);
suspend();
}
suspendlist[acs_fgc] = suspended;
return;

case 49:
if(!*suptext) goto error_bell;
etcjup(suptext);
if(access(jfile, 4)) goto error_bell;
chromscale(jfile);
break;

case 50: // char plus asword
asword = asword2 = 1;
goto letter;

case 51:
// separate the yes no strings
yn = strchr(suptext, '|');
if(yn) *yn = 0;
if(modeValue) acs_say_string(suptext);
else if(yn) acs_say_string(yn + 1);
if(yn) *yn = '|';
break;

case 52: /* current word length */
		acs_cursorsync();
		acs_endword();
		n = acs_startline();
		acs_cursorset();
		n += acs_startword();
		n -= acs_startline();
		sprintf(shortPhrase, "%d", n);
		acs_cursorset();
		acs_say_string_uc(prepTTSmsg(shortPhrase));
break;

	default:
	error_bell:
if(soundsOn) 		acs_bell();
else acs_say_string(o->errorword);
		return;

	error_buzz:
		if(soundsOn) acs_buzz();
		else acs_say_string(o->outsideword);
		return;

	error_bound:
if(soundsOn) acs_highbeeps();
else acs_say_string(o->endword);
		return;
	} /* end switch on function */

modeValue = 1;
	goto top; /* next command */
}

/* keystroke handler */
static int last_key, last_ss;
static void key_h(int key, int ss, int leds)
{
last_key = key;
last_ss = ss;
}

/* Remember the last console, and speak a message if it changes */
static int last_fgc;
static void fgc_h(void)
{
if(!last_fgc) { /* first time */
last_fgc = acs_fgc;
return;
}

if(last_fgc == acs_fgc)
return; /* should never happen */

/* Modes that are per console */
if(screenMode != smlist[acs_fgc]) {
screenMode = smlist[acs_fgc];
acs_screenmode(screenMode);
ctrack = 1;
}

/* kill any pending keystroke command; we just switched consoles */
last_key = last_ss = 0;

/* stop reading, and speak the new console */
interrupt();
sprintf(shortPhrase, "%s %d", o->consword, acs_fgc);
		acs_say_string_uc(prepTTSmsg(shortPhrase));

// If firsst time to this console, assume the start config file.
if(!cfglist[acs_fgc])
cfglist[acs_fgc] = cloneString(start_config);

if(!suspended && suspendlist[acs_fgc]) {
suspend();
goto done;
}

if(suspended && !suspendlist[acs_fgc]) {
unsuspend();
goto done;
}

if(!stringEqual(cfglist[last_fgc], cfglist[acs_fgc])) {
acs_reset_configure();
etcjup(cfglist[acs_fgc]);
j_configure(jfile, 0);
}

done:
last_fgc = acs_fgc;
}

/* fifo input still works, even if suspended */
static void fifo_h(char *msg)
{
int rc;
/* stop reading, and speak the message */
interrupt();
// special execute now code
if(msg[0] == ':' && msg[1] == ':') {
rc = cfg_syntax(msg+2);
if(rc) acs_bell();
else runSpeechCommand(0, msg+2);
} else acs_say_string(msg);
}

static void more_h(int echo, unsigned int c)
{
if(suspended) return;

if (keyInterrupt && echo == 1) {
/* In this case we want to shutup, whether the unit
 * is in reading/indexed mode or not. */
acs_shutup();
acs_rb = 0;
goRead = 0;
}
if(echoMode && echo == 1 && c >= 0x20) {
interrupt();
speakChar(c, 1, soundsOn, 1);
}

goRead2 = (echo == 0);
if(acs_rb) return;
ctrack = 1;
if(!autoRead) return;
if(!echo) goRead = 1;
}

static void
openSound(void)
{
static const short startNotes[] = {
		476,5,
530,5,
596,5,
662,5,
762,5,
858,5,
942,5,
0,0};
acs_notes(startNotes);
}

static void
testTTS(void)
{
char line[400];
char *out;

/* This doesn't go through the normal acs_open() process */
acs_reset_configure();
/* key bindings don't matter here, but let's load our pronunciations */
j_configure(start_config, 0);

while(fgets(line, sizeof(line), stdin)) {
out = acs_uni2utf8(prepTTSmsg(line));
printf("%s", out);
free(out);
}

exit(0);
}

static void
selectLanguage(void)
{
    char buf[8];
    char *s = getenv("LANG");

acs_lang = ACS_LANG_EN; // default

    if(!s)
	return;
    if(!*s)
	return;

    strncpy(buf, s, 7);
    buf[7] = 0;
for(s=buf; *s; ++s) {
if(*s >= 'A' && *s <= 'Z')
*s |= 0x20;
}

    if(!strncmp(buf, "en", 2))
	return;			/* english is default */

    if(!strncmp(buf, "de", 2)) {
acs_lang = ACS_LANG_DE;
	return;
    }

    if(!strncmp(buf, "pt_br", 5)) {
acs_lang = ACS_LANG_PT_BR;
	return;
    }

    if(!strncmp(buf, "fr", 2)) {
acs_lang = ACS_LANG_FR;
	return;
    }

    if(!strncmp(buf, "sk", 2)) {
acs_lang = ACS_LANG_SK;
	return;
    }

    fprintf(stderr, "Sorry, language %s is not implemented\n", buf);
}

/* Supported synthesizers */
struct synth {
const char *name;
int style;
const char *initstring;
} synths[] = {
{"dbe", ACS_SY_STYLE_DOUBLE,
/*********************************************************************
I use to send 26g which enables bits 4 3 and 1.
4 allows for nonascii, so people can send Spanish characters and use the
Spanish dictionary. I'd like to keep this bit set.
Bit 1 is using ring indicator, but I don't watch for that.
I use to set it but don't use it, and if there's anything
wrong with that serial line, well then...
Bit 3 is early sync, sending me notification slightly before the word has fully spoken.
This seems important, but I don't notice any difference if I don't set it.
I have a doubletalk that loses its brains if this is set. One out of three.
So now I don't set that bit, so I can use all three talkers.
16g instead of 26g.
*********************************************************************/
"\1@ \0012b \00116g \0012o \00194i "},
{"dte", ACS_SY_STYLE_DECEXP},
{"dtp", ACS_SY_STYLE_DECPC},
{"bns", ACS_SY_STYLE_BNS},
{"ace", ACS_SY_STYLE_ACE},
{"esp", ACS_SY_STYLE_ESPEAKUP},
{0, 0}};

int main(int argc, char **argv)
{
int i, port, onusb = 0;
char serialdev[20];
char *cmd = NULL;
int lastrow, lastcol;

// remember the arg vector, before we start marching along.
argvector = argv;
++argv, --argc;
program_file = getenv("_");
if (!program_file)
	program_file = *argvector;

selectLanguage();
o = outwords + acs_lang;

if(setupTTS()) {
fprintf(stderr, "Could not malloc space for text preprocessing buffers.\n");
exit(1);
}

while(argc) {
if(argc && stringEqual(argv[0], "-d")) {
/* it should be safe to chdir, but not to close std{in,out,err}. Reload prints stuff there. */
daemon(1, 1);
/* make this the leader of its process group,
 * so the child process, a software synth, dies when it does. */
setsid();
++argv, --argc;
continue;
}

if(argc && argv[0][0] == '-' && argv[0][1] == 'a') {
char f = argv[0][2];
if(f >= '1' && f <= '9') {
aovolume = f - '0';
if(argv[0][3] == ',' && (f = argv[0][4]) >= '1' && f <= '9')
aospeed = f - '0';
}
ao_sample_format fmt;
fmt.bits = sizeof(short) * 8;
fmt.channels = 1;
fmt.rate = SAMPRATE;
fmt.byte_format = AO_FMT_NATIVE;
ao_initialize();
int driver_id = ao_default_driver_id();
if(driver_id >= 0)
aodev = ao_open_live(driver_id, &fmt, NULL);
if(!aodev) fprintf(stderr, "cannot open audio device for sounds\n");
++argv, --argc;
}

if(argc && stringEqual(argv[0], "-c")) {
++argv, --argc;
if(argc) {
start_config = argv[0];
++argv, --argc;
}
continue;
}

break;
}

if(argc && stringEqual(argv[0], "tts")) {
tp_readLiteral = 0;
testTTS();
return 0;
}

if(argc && stringEqual(argv[0], "ltts")) {
tp_readLiteral = 1;
testTTS();
return 0;
}

if(argc && stringEqual(argv[0], "tc")) {
j_configure(start_config, 0);
return 0;
}

if(argc != 2) usage();
for(i=0; synths[i].name; ++i)
if(stringEqual(synths[i].name, argv[0])) break;
if(!synths[i].name) usage();
acs_style = synths[i].style;
acs_style_defaults();
++argv, --argc;

if (*argv[0] == '|') {
cmd = argv[0]+1;
} else {
if(argv[0][0] == 'u')
port = atoi(argv[0]+1), onusb = 1;
else
port = atoi(argv[0]);
if(port < 0 || port > 7) usage();
sprintf(serialdev, "/dev/%s%d",
(onusb ? "ttyUSB" : "ttyS"), port);
}

/* Compare major minor numbers on acsdriver with what we see
 * in /sys.  If it's wrong, and we are root, fix it up.
 * This is linux only. */
acs_nodecheck(acsdriver);

if(acs_open(acsdriver) < 0) {
fprintf(stderr, o->openDriver, acsdriver, strerror(errno));
if(errno == EBUSY) {
fprintf(stderr, o->busyDriver);
exit(1);
}
if(errno == EACCES) {
fprintf(stderr, o->permDriver, acsdriver);
exit(1);
}
fprintf(stderr, o->makeDriver, acsdriver);
exit(1);
}

acs_key_h = key_h;
acs_fgc_h = fgc_h;
acs_more_h = more_h;
acs_fifo_h = fifo_h;
acs_imark_h = imark_h;

if (cmd && acs_pipe_system(cmd) == -1) {
fprintf(stderr, o->execSoft, cmd);
exit(1);
}

if(!cmd && acs_serial_open(serialdev, 9600)) {
fprintf(stderr, o->openSerial, serialdev);
exit(1);
}

openSound();

/* Initialize the synthesizer. */
if(synths[i].initstring)
acs_say_string(synths[i].initstring);

acs_startfifo("/etc/jupiter/fifo");

/* I have a low usage machine, so a small gap in output
 * usually means something new to read.  Set it at 0.4 seconds. */
acs_obreak(4);

/* This is the same as the default, but I set it here for clarity. */
acs_postprocess = ACS_PP_CTRL_H | ACS_PP_CRLF |
ACS_PP_CTRL_OTHER | ACS_PP_ESCB;

// First event sets the console, in case config file has execution commands.
acs_all_events();

/* this has to run after the device is open,
 * because it sends key capture commands to the acsint driver,
 * and after the first event sets up the console. */
cfglist[acs_fgc] = cloneString(start_config);
j_configure(start_config, 1);

// jupiter ready
acs_say_string(o->readyword);

if(screenMode & autoRead) {
acs_vc();
lastrow = acs_vc_row, lastcol = acs_vc_col;
acs_log("lc %d,%d\n", lastrow, lastcol);
}

/* This runs forever, you have to hit interrupt to kill it,
 * or kill it from another console. */

while(1) {
char newcmd[8];

acs_all_events();

key_command:
if(last_key) {
char *cmdlist; // the speech command
int mkcode = acs_build_mkcode(last_key, last_ss);
last_key = last_ss = 0;
cmdlist = acs_getspeechcommand(mkcode);
//There ought to be a speech command, else why were we called?
if(cmdlist) runSpeechCommand(1, cmdlist);
}

if(!acs_rb && cmd_resume) {
runSpeechCommand(1, cmd_resume);
}

if(goRead) {
unsigned int c;
goRead = 0;

refetch:
/* fetch the new stuff and start reading */
// Pause, to allow a block of characters to print.
usleep(100000);
acs_rb = acs_tb;
readNextMark = acs_rb->end;
acs_log("mark1 %d\n", readNextMark - acs_rb->start);
/* The refresh is really a call to events() in disguise.
 * So any of those handlers could be called.
 * Since acs_rb is set, more_h won't cause any trouble. */
acs_refresh();
/* did a keycommand sneak in? */
if(last_key) goto key_command;
/* did reading get killed for any other reason, e.g. console switch? */
if(!acs_rb) { acs_log("read off\n"); continue; }
if(!readNextMark) { acs_rb = 0; acs_log("mark off\n"); continue; }
acs_log("mark2 %d\n", readNextMark - acs_rb->start);

if(!*readNextMark) { acs_rb = 0; goto autoscreen; }

while(c = *readNextMark) {
if(c != ' ' && c != '\n' &&
c != '\r' && c != '\7')
break;
++readNextMark;
}
if(!c) goto refetch;

acs_log("mark3 %d %c\n", readNextMark - acs_rb->start, c);
// autoread turns off oneLine mode.
oneLine = 0;
if(screenMode) {
acs_vc();
lastrow = acs_vc_row, lastcol = acs_vc_col;
acs_log("lc %d,%d\n", lastrow, lastcol);
}
readNextPart();
continue;
}

autoscreen:
if(!screenMode) continue;
if(!autoRead) continue;

if(!goRead2 || acs_rb) {
// note the (possibly new) position of the cursor; that's it.
acs_vc();
lastrow = acs_vc_row, lastcol = acs_vc_col;
acs_log("lc %d,%d\n", lastrow, lastcol);
continue;
}

acs_screensnap();

// read new character if you arrowed left or right one character
acs_log("vc %d,%d\n", acs_vc_row, acs_vc_col);
if(acs_vc_row == lastrow && (acs_vc_col == lastcol+1 || acs_vc_col == lastcol-1)) {
acs_mb->cursor = acs_mb->v_cursor;
autoletter:
acs_log("autochar %c\n", acs_mb->cursor[0]);
		speakChar(acs_mb->cursor[0], 1, soundsOn, 1);
goto updatecursor;
}

// read new word if you arrowed left or right one word
if(acs_vc_row == lastrow && acs_vc_col != lastcol) {
acs_mb->cursor = acs_mb->v_cursor;
acs_log("autoword %c\n", acs_mb->cursor[0]);
newcmd[0] = cmdByName("word");
newcmd[1] = cmdByName("cursor");
newcmd[2] = 0;
runSpeechCommand(0, newcmd);
goto updatecursor;
}

// read new line if you arrowed up or down one line
if(acs_vc_row == lastrow+1 || acs_vc_row == lastrow-1) {
acs_mb->cursor = acs_mb->v_cursor;
acs_log("autoline %c\n", acs_mb->cursor[0]);
newcmd[0] = cmdByName("sline");
newcmd[1] = cmdByName("stmode");
newcmd[2] = '1';
newcmd[3] = cmdByName("read");
newcmd[4] = cmdByName("cursor");
newcmd[5] = 0;
runSpeechCommand(0, newcmd);
// fall through
}

updatecursor:
lastrow = acs_vc_row, lastcol = acs_vc_col;
}

acs_close();
}

