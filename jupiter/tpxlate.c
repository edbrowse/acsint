/*********************************************************************

tpxlate.c: perform final translations to prepare for tts.
This expands the constructs that were encoded in tpencode.c.

Copyright (C) Karl Dahlke, 2014.
This software may be freely distributed under the GPL, general public license,
as articulated by the Free Software Foundation.

The point of these routines is to add text, or codes, to an output string,
as a result of processing an input string.
When we reach the end of the output string, we either return an error code
all the way back to the calling routine, or we realloc the string,
depending on the application.
In practice the output buffer is large enough to handle any
reasonably sized sentence.
*********************************************************************/

#include <time.h>
#include <malloc.h>

#include "tp.h"

#define appendBackup() (--tp_out->len)
#define case_different(x, y) (acs_isupper(x) ^ acs_isupper(y))


/*********************************************************************
A few global variables.
These can be adjusted based on your synthesizer.
*********************************************************************/

char tp_acronUpper = 1; /* acronym letters in upper case? */
char tp_acronDelim = ' ';
char tp_oneSymbol; /* read one symbol - not a sentence */
char tp_readLiteral = 1; // read each punctuation mark
/* a convenient place to put little phrases to speak */
char shortPhrase[NEWWORDLEN];


/*********************************************************************
Strings that are folded into the output text.
There is one structure of strings for each language.
Of course other aspects of translation must also change.
Spanish might say "the fifth of may", rather than May fifth,
but we hope most of the changes are captured in these strings.
*********************************************************************/

struct OUTWORDS {
const char *natoWords[26];
const char *real2;
const char *real3[26];
const char *contractions[60];
const char *decades[8];
const char *hundredWord;
const char *thousandWord;
const char *millionWord;
const char *billionWord;
const char *trillionWord;
const char *bigNumbers[5];
const char *hundWords[10];
const char *hundredthWord;
/*Sometimes it's more portable to use words than numbers. */
/*Especially if you're adding 's on the end, such as 5 Boing 707's. */
const char *idigits[10];
const char *ohWord; /*o instead of 0 */
const char *teens[10];
const char *ordinals[20];
const char *orddecades[8];
const char *todayWord;
const char *yesterdayWord;
const char *tomorrowWord;
const char *weekdays[7];
const char *noonWord;
const char *midnightWord;
const char *oclockWord;
const char *andWord;
const char *orWord;
const char *capWord;
const char *unicodeWord;
const char *halfWord;
const char *toTheWord;
const char *squareWord;
const char *cubeWord;
const char *dashWord;
const char *colonWord;
const char *bangWord;
const char *minusWord;
const char *dollarWord;
const char *dollarsWord;
const char *centWord;
const char *centsWord;
const char *ieWord;
const char *egWord;
const char *theWord;
const char *lengthWord;
const char *byWord;
const char *pointWord;
const char *dotWord;
const char *atWord;
const char *toWord;
const char *throughWord;
const char *slashWord;
const char *numberWord;
const char *numbersWord;
const char *lessWord;
const char *greaterWord;
const char *equalsWord;
const char *oreqWord;
const char *months[12];
const char *nohundred[30];
const char *slashOrPhrases[8];
};

static const struct OUTWORDS outwords[6] = {

{ /* no output words for the zero language */
0

},{ /* English */

// nato standard alphabet
{"alpha", "bravo", "charlie", "delta", "echo",
"foxtrot", "golf", "hotel", "india", "juliett",
"killo", "liema", "mike", "november", "oscar",
"papa", "quebec", "romeo", "sierra", "tango",
"uniform", "victor", "whiskey", "x ray", "yankee", "zulu"},
// 2 letter words
"adahalamanasatauawaxhalamapaedehelemenbedehemereweyeidifinisithipiofohonorowoxozcodogohojolonosotouhumunupusbymy",
// 3 letter words
{
"abe abs>ace ack^act ada>add ado^ads^adz^"
"aft age ago aha>aid ail>aim air ala^ale>"
"alf>all alm>alo>alp alt amo^amp amy ana>"
"and ani>ann ant any ape app^apt^arc are "
"arg>ark arm art ase>ash ask ass ast>ate "
"aud>aux ave>awe^awk awl^axe aye aze>azy>",
"bac>bad baf>bag bah>bak bal>bam ban bar "
"bas bat bay bec>bed bee beg bek>bel>ben "
"ber^bes^bet bib bic bid bif^big bik>bil>"
"bin bio bis>bit bla>bly boa bob boc^boe>"
"bog^bok>bom>bon^boo bop>bor^bow box boy "
"bra^bry>btw^bub>bud bug bum bun bur bus "
"but buy bye ",
"cab cad caf cal cam can cap car cas cat "
"caw>cel cen cob cod cog com con coo cop "
"cor cot cow coy cry cub cud^cum>cup cus "
"cut cuz^",
"dab dad dam dan dat>dax^day deb dec ded>"
"dee>def>del den deo>det dev dew dib did "
"die dig dim din dip dis>dob>doc>doe>dog "
"dom>don dos dot dry dub>dud^due dug duh>"
"dum>dun duo^dye ",
"eak>eal>ear eat ebb ebs>eck>ect>eek^eel^"
"eep>eke^egg ego elf>elk>elm elp>els>elt>ema>"
"emy end eon^era^erg>esh>esk>est>etc eth>"
"eve evi>ewe ext>eye ",
"fab fad fag^fan far fat fax fay feb fed "
"fee fer few fez fib fie>fig fin fit fix "
"fiz flo flu fly foe fog foo for fox fro "
"fry fud^fug>fun fur fus fyi ",
"gab gad gaf>gag gak>gal gam>gan>gap gar "
"gas gat>gay gee>gem gen geo get gig^gin "
"gip^git>gnu gob god gon>goo gor got gov "
"guf>gum gun gus gut guy gym gyp>",
"hab hac had haf>hag hak hal ham han>hap "
"har>has hat haw>hay hed hee>hem hen her "
"hew>hex hey hic hid him hin>hip his hit "
"hob hoc hoe hog hon hoo hop hor^hot how "
"hub hud hue^hug hum hun hut hyp>",
"ian>ice ich>ick>icy ids>ied>ier>iff>ike "
"ilk>ill ime>imp inc^ine>ink inn ins>int "
"ion ips>ire>irk>ise>ish>isk>iso ist>ite>"
"ith>its ity>ive>ivy ize>",
"jab jac jag>jam jan jap jar jaw jay jed "
"jem jen jet jew jib jif jig jim jin jip "
"job joc joe jog jon jot joy jud jug jut ",
"kam kan kap kar kat kay ked^keg ken key "
"kid kim kin kip kit kob kod kon kop kub "
"kut ",
"lab lac>lad lag^lam lan>lap law lax lay "
"lea led^lee leg lei>lem len>les>let lew^"
"lex lib lid lie lim lin lip lis>lit liv>"
"liz lob^loc log lop^los^lot lou^lox low "
"lug lux>lyn ",
"mac mad mag mak mam^man map mar^mas>mat "
"max maw may med meg mel men mes>met mew>"
"mex mid mig min mip mir^mis^mit mix mob "
"mod mom mop mos>mow mox>muc>mud muf>mug "
"mum mus>mut mux ",
"nab nad>nag nak>nan>nap nas>nay ned nel "
"neo>net new nip nit nod non nop nor nos>"
"not nov>now nox nub nuc>nuk>num nun nut ",
"oaf>oak oam>oar oat obe>obs>ock>odd ode^"
"ods>off oft oge>ohm oil ola>old ole>oli^"
"ome>one ong>onk>ons>oon>oop>ops opt ora "
"orb ore org ork>orn>orr>ors>ort>ose>osh>"
"ost>ote>oth>oul>our out ova owe owl own "
"ows>",
"pac pad pak pal pam pan par pas>pat paw "
"pay pea peb ped>peg pek pel pen pep per "
"pet pew phi pic pie pig pik pil pin pip>"
"pit ply pod poe poo pop pot pow>pox poy "
"pre pro pry pub pug pun pup pur put ",
"que quo quy ",
"rac rad rag rak ram ran rap rat raw ray "
"raz red rem reo rep rex rev^rew rib rid "
"rif>rig ril>rim rin>rip ris>rit>rob roc>"
"rod rok>rom ron rop rot row roy roz rub "
"rue rug rum run rut rye ",
"sac>sad sag sal sam san>sap sar>sas>sat "
"saw sax say sea sco sed see sel>sep>set "
"sew sex sha she shy sic>sid>sil sim sin "
"sip sir sis>sit six ski sky sly sob soc "
"sod sog son sop^sot>sow sox soy spa spy "
"sty sub sue sum sun sup^syn ",
"tab tac tad^tag tak tal>tam tan tap tar "
"tat>tax taz>tea ted tee tel ten tes>tex "
"the thy tib>tic tie tif tik>til tim tin "
"tip tis^tiz>toc tod>toe tom ton too top "
"tor>tos>tot tow tox>toy try tub tug tum "
"tun tut tux two ",
"ugh^uma>una uni uno ups>urn use ush>",
"vac vad val>van var>vat vax vet vex via^"
"vic vie vik vim vix voc vol von vow vox ",
"wac wad wag wam wap war was wax wav>way "
"web wed wee^wer>wet who^why wic wig win "
"wip wix woe^wok^won woo wop>wow wry ",
"",
"yac yak yam yan>yap yaw yee yen yes yet "
"yon yot you yuk yum yup ",
"zac zag zam zak zan zap zar zed zen zig "
"zip zit>zoo "
},
/* contractions, we don't need 's possessive or 'll future tense. */
{	"isn't", "wasn't", "hasn't", "didn't",
	"hadn't", "i've", "you've", "we've", "they've",
	"who've", "who'd", "who're",
	"how've", "how'd", "how're",
	"doesn't", "haven't", "weren't", "mustn't",
	"shouldn't", "couldn't", "wouldn't", "don't",
	"aren't", "ain't", "oughtn't", "mightn't",
	"shan't", "i'm", "can't", "won't",
	"i'd", "you'd", "he'd", "she'd",
	"they'd", "we'd", "it'd",
	"would've", "should've", "could've", "must've",
	"you're", "we're", "they're",
0},
{"twenty", "thirty", "forty", "fifty", "sixty", "seventy", "eighty", "ninety"},
"hundred", "thousand", "million", "billion", "trillion",
{"thousand", "million", "billion", "trillion", 0},
{0},
"hundredth",
{"0", /* I haven't found a good portable word for zero */
"one", "two", "three", "four", "five", "six", "seven", "eight", "nine"},
"o",
{"ten", "eleven", "twelve", "thirteen", "fourteen",
"fifteen", "sixteen", "seventeen", "eighteen", "nineteen"},
{"zeroath", "first", "second", "third", "forth",
"fifth", "sixth", "seventh", "eighth", "ninth",
"tenth", "eleventh", "twelvth", "thirteenth", "fourteenth",
"fifteenth", "sixteenth", "seventeenth", "eighteenth", "nineteenth"},
{"twentieth", "thirtieth", "fortieth", "fiftieth", "sixtieth", "seventieth", "eightieth", "ninetieth"},
"today", "yesterday", "tomorrow",
{"sunday", "munday", "tuesday", "wednesday",
"thursday", "friday", "saturday"},
"noon", "midnight", "uhclock",
"and", "or",
"cap ",
"code",
"half",
"to the", "squared", "cubed",
"dash", "colen", "bang", "minus", "doller", "dollers", "cent", "cents",
"that is,", "for example,",
"the", "length", "by",
"point", "dot", "at", "to", "through",
"slash", "number", "numbers",
"less than", "greater than", "eequals", "or eequal to",
{"january", "february", "march",
"aiprle", "may", "june",
"july", "august", "september",
"october", "noavember", "december"},
{"room", "suite", "apartment", "apt",
"dept", "department", "box", "pobox",
"part", "piece", "site", "box",
"car", "flight", "number",
0},
{"he/she", "she/he", "him/her", "her/him", "his/her", "her/his", 0},

},{ /* German */

{"Anton", "Berta", "Cäsar", "Dora", "Emil", "Friedrich", "Gustav", "Heinrich",
"Ida", "Julius", "Kaufmann", "Ludwig", "Martha", "Nordpol", "Otto", "Paula",
"Quelle", "Richard", "Samuel", "Theodor", "Ulrich", "Viktor", "Wilhelm",
"Xanthippe", "Ypsilon", "Zacharias",},
/* umlauts
"Ärger" "Ökonom", "Übermut",
*/
// 2 letter words
"andedueresinjamaumzu",
// 3 letter words
{0},
// contractions, none in this language
{0},
{"zwansig", "dreizig", "vierzig", "fünfzig", "sechzig", "sebzig", "ochtzig", "neunzig"},
"hundert", "tausand", "million", "billion", "trillion",
{"tausand", "million", "billion", "trillion", 0},
{0},
"hundert",
{"0", /* I haven't found a good portable word for zero */
"einz", "zwei", "drei", "vier", "fünf", "sechs", "seben", "ocht", "neun"},
"o",
{"zehn", "elf", "zwolf", "dreizehn", "vierzehn",
"fünfzehn", "sechzehn", "sebenzehn", "ochtzehn", "neunzehn"},
{"nullte", "erste", "zweite", "drite", "vierte",
"fünfte", "sechte", "sebente", "octhe", "neunte",
"zehnte", "elfte", "zwolfte", "dreizehnte", "vierzehnte",
"fünfzehnte", "sechzehnte", "sebenzehnte", "ochtzehnte", "neunzehnte"},
{"zwansigte", "dreizigte", "vierzigte", "fünfzigte", "sechzigte", "sebzigte", "ochtzigte", "neunzigte"},
"heute", "gestern", "morgan",
{"sontag", "muntag", "deenstag", "mitwok",
"donerstag", "freitag", "samstag"},
"mittag", "mitternacht", "uhr",
"und", "odor",
"auf ",
"num",
"halp",
"zu", "squared", "cubed",
"dash", "colen", "bang", "minus", "toller", "toller", "cent", "cents",
"das ist,", "zum beispiel,",
"die", "langes", "bei",
"punck", "punck", "an", "zu", "durch",
"slash", "numer", "numern",
"less than", "greater than", "eequals", "or eequal to",
{"januar", "februar", "march",
"aiprle", "mei", "june",
"juligh", "august", "september",
"october", "noavember", "december"},
{"zimmer", "suite", "apartment", "apt",
"dept", "department", "box", "pobox",
"part", "piece", "site", "box",
"car", "flug", "numer",
0},
{"er/sie", "sie/er", "ihm/ihr", "ihr/ihm", "sein/ihr", "ihr/sein", 0},

},{ /* Portuguese */

{"alfa", "bravo", "carlos", "delta", "eco",
"fox", "golfe", "hotel", "índia", "julieta",
"kilo", "lima", "maique", "novembro", "oscar",
"papa", "quebeque", "romeu", "serra", "tango",
"uniforme", "vítor", "wisque", "x raio", "yanque", "zulu"},
// 2 letter words
0,
// 3 letter words
{0},
// contractions, none in this language
{0},
{"vinte", "trinta", "quarenta", "cincoenta", "sessenta", "setenta",
"oitenta", "noventa"},
"cem", "mil", "milhão", "bilhão", "trilhão",
{"mil", "milhão", "bilhão", "trilhão", 0},
{0, "cento", "duzentos", "trezentos", "quatrocentos", "quinhentos", "seiscentos", "setecentos", "oitocentos", "novecentos"},
"centésimo",
{"0", /* I haven't found a good portable word for zero */
"um", "dois", "três", "quatro", "cinco", "seis", "sete", "oito", "nove"},
"zero",
{"dez", "onze", "doze", "treze", "catorze",
"quinze", "dezesseis", "dezessete", "dezoito", "dezenove"},
{"zero-ésimo", "primeiro", "segundo", "terceiro", "quarto",
"quinto", "sexto", "sétimo", "oitavo", "nono",
"décimo", "undécimo", "duodécimo", "décimo-terceiro", "décimo-quarto",
"décimo-quinto", "décimo-sexto", "décimo-sétimo", "décimo-oitavo",
"décimo-nono"},
{"vigésimo", "trigésimo", "quadragésimo", "quinquagésimo",
"sexagésimo", "septuagésimo", "octogésimo", "nonuagésimo"},
"hoje", "ontem", "amanhã",
{"Domingo", "Segunda-Feira", "Terça-Feira", "Quarta-Feira",
"Quinta-Feita", "Sexta-Feira", "Sábado"},
"meio-dia", "meia-noite", "horas",
"e", "ou",
"cap ",
"código",
"meio",
"à", "quadrado", "cubo",
"hífen", "dois pontos", "exclamação", "menos", "real", "reais",
"centavo", "centavos",
"isto é,", "por exemplo,",
"o", "comprimento", "por",
"ponto", "ponto", "arroba", "para", "por",
"barra", "número", "números",
"menor", "maior", "igual", "ou igual a",
{"janeiro", "fevereiro", "março",
"abril", "maio", "junho",
"julho", "agosto", "setembro",
"outubro", "novembro", "dezembro"},
{"sala", "suite", "apartamento", "apê",
"depto", "departamento", "caixa", "caixa postal",
"parte", "bloco", "local", "caixa",
"carro", "vôo", "número",
0},
{0},

},{ /* French */

// nato standard alphabet
{"alpha", "bravo", "charli", "delta", "echo",
"foxtrote", "golf", "hôtel", "inndia", "djouliette",
"kilô", "lima", "maïk", "novembe", "oscar",
"popa", "kèbèk", "rômïô", "sïèra", "tanngo",
"youniform", "victor", "wiski", "èks rè", "yannki", "zoulou"},
// 2 letter words
0,
// 3 letter words
{0},
// contractions, none in this language
{0},

{"vingt", "trente", "quarante", "cinquante", "soixante", "soixante-dix", "quatre-vingt", "quatre-vingt-dix"},
"cent", "mille", "million", "milliard", "billion",
{"mille", "million", "milliard", "billion", 0},
{0},
"centième",
{"0", /* I haven't found a good portable word for zero */
"un", "deux", "trois", "quatre", "cinq", "six", "sept", "huit", "neuf"},
"o",
{"dix", "onze", "douze", "treize", "quatorze",
"quinze", "seize", "dix-sept", "dix-huit", "dix-neuf"},
{"zeroième", "premier", "deuxième", "troisième", "quatrième",
"cinquième", "sixième", "septième", "huitième", "neuvième",
"dixième", "onzième", "douzième", "treizième", "quatorième",
"quinzième", "seizième", "dix-septième", "dix-huitième", "dix-neuvième"},
{"vingtième", "trentième", "quarantième", "cinquantième", "soixantième", "soixante-dixième", "quatre-vingtième", "quatre-vingt-dixième"},
"aujourd'hui", "hier", "demain",
{"dimanche", "lundi", "mardi", "mercredi",
"jeudi", "vendredi", "samedi"},
"midi", "minuit", "heure",
"et", "ou",
"cap ",
"code",
"demi",
"à", "carré", "cube",
"tiret", "deux points", "point d'exclamation", "moins", "dollar", "dollars", "centime", "centimes",
"c'est à dire,", "par exemple,",
"la", "longueur", "par",
"point", "point", "à", "à", "via",
"slash", "nombre", "nombres",
"inférieur à", "supérieur à", "égal", "ou égal à",
{"janvier", "février", "mars",
"avril", "mai", "juin",
"juillet", "août", "septembre",
"octobre", "novembre", "décembre"},
{"chambre", "suite", "appartement", "apt",
"dept", "département", "boîte", "boîte postale",
"partie", "pièce", "site", "boîte",
"voiture", "vol", "numéro",
0},
{"il/elle", "elle/il", "lui/elle", "elle/lui", "son/sa", "sa/son", 0},

},{ /* Slovak */

// nato standard alphabet
{"adam", "božena", "cyril", "dávid", "emil",
"františek", "gustáv", "helena", "ivan", "jozef",
"karol", "ladislav", "mária", "norbert", "oto",
"peter", "quido", "rudolf", "svätopluk", "tomáš",
"urban", "václav", "dvojité vé", "xaver", "ypsilon", "zuzana"},
// 2 letter words
"ahakaubobubybzdoduhahehihohujajejukulamamimumynanenopiposasisostuttitotutyvovyzazo",
// 3 letter words
{0},
// contractions, none in this language
{0},
{"dvadsať", "tridsať", "štyridsať", "päťdesiat", "šesťdesiat", "sedemdesiat", "osemdesiat", "deväťdesiat"},
"sto", "tisíc", "milión", "bilión", "trilión",
{"tisíc", "milión", "bilión", "trilión", 0},
{0},
"sto",
{"0", /* I haven't found a good portable word for zero */
"jeden", "dva", "tri", "štyri", "päť", "šesť", "sedem", "osem", "deväť"},
"nula",
{"desať", "jedenásť", "dvanásť", "trinásť", "štrnásť",
"pätnásť", "šestnásť", "sedemnásť", "osemnásť", "devätnásť"},
{"nultý", "prvý", "druhý", "tretí", "štvrty",
"piaty", "šiesty", "siedmy", "ôsmy", "deviaty",
"desiaty", "jedenásty", "dvanásty", "trinásty", "štrnásty",
"pätnásty", "šestnásty", "sedemnásty", "osemnásty", "devätnásty"},
{"dvadsiaty", "tridsiaty", "štyridsiaty", "päťdesiaty", "šesťdesiaty", "sedemdesiaty", "osemdesiaty", "deväťdesiaty"},
"dnes", "včera", "zajtra",
{"nedeľa", "pondelok", "utorok", "streda",
"štvrtok", "piatok", "sobota"},
"poludnie", "polnoc", "hodín",
"a", "alebo",
"veľké ",
"kód",
"polovica",
"do", "druhá odmocnina", "tretia odmocnina",
"spojovník", "dvojbodka", "výkričník", "mínus", "dolár", "dolárov", "cent", "centov",
"to jest,", "napríklad,",
"", "dĺžka", "by",
"bod", "bodka", "zavináč", "do", "cez",
"lomka", "číslo", "čísla",
"menší", "väčší", "rovná sa", "alebo sa rovná",
{"január", "február", "marec",
"apríl", "máj", "jún",
"júl", "august", "september",
"október", "november", "december"},
{"izba", "suite", "apartmán", "apt",
"dept", "department", "box", "pobox",
"part", "piece", "site", "box",
"car", "let", "číslo",
0},
{"on/ona", "ona/on", "jemu/jej", "jej/jemu", "jeho/jej", "jej/jeho", 0},

}};

static const struct OUTWORDS *ow;
static const char *andWord;

/* Set things up for tts preprocessing */
int
setupTTS(void)
{
const int room = 400;
tp_in->buf = malloc(room * sizeof(unsigned int));
tp_in->offset = malloc(room * sizeof(acs_ofs_type));
tp_out->buf = malloc(room * sizeof(unsigned int));
tp_out->offset = malloc(room * sizeof(acs_ofs_type));
if(!tp_in->buf || !tp_in->offset || !tp_out->buf || !tp_out->offset)
return -1;
tp_in->room = room;
tp_out->room = room;

	ow = outwords + acs_lang;
	andWord = ow->andWord;

//  sortReservedWords();

return 0;
}


/*********************************************************************
Turn null into '\n'.
Turn nonspace control character or delete into space.
The name of this function is vestigial; now acting on unicodes,
so we wouldn't want to ascify the buffer.
*********************************************************************/

static void ascify(void)
{
	unsigned int *s, *end_s;
	unsigned int  c;

	s =tp_in->buf + 1;
	end_s = tp_in->buf + tp_in->len;

	for(; s < end_s; ++s) {
		c = *s;
		if(c == 0) c = '\n'; //should never happen
		if(c == '\t') goto add_c;
		if(c == '\n') goto add_c;
		if(c == '\7')goto add_c;
		if(!tp_readLiteral) {
			/* Treat delete or control character as space */
			if(c == 0x7f) c = ' ';
			if(c < ' ') c = ' ';
		}

add_c:
		*s = c;
	} /* end loop over characters in the input message. */
}


/* speak a single character.
 * Or write, into shortPhrase, how it should be pronounced.
* sayit means speak the character now.
 * bellsound means newline and bell make sounds, rather than speaking words.
 * asword = 1, say the word cap before capital letter.
 * asword = 2, a letter is spoken using the nato phonetic alphabet,
 * thus making it clear whether it is m or n. */
void speakChar(unsigned int c, int sayit, int bellsound, int asword)
{
	short i, l;
	const char *t;
	static char ctrlstr[] = "controal x";
char hexbuf[16];

	if(c == '\7') {
		if(bellsound) { acs_bell(); return; }
		t = acs_getpunc(c);
		goto copy_t;
	}

	if(c == '\r') {
		t = acs_getpunc(c);
		goto copy_t;
	}

	if(c == '\n') {
		if(bellsound) { acs_cr(); return; }
		t = acs_getpunc(c);
		goto copy_t;
	}

	if(c == '\t') {
		t = acs_getpunc(c);
		goto copy_t;
	}

	if(c < 27) {
		ctrlstr[9] = c|0x40;
		t = ctrlstr;

copy_t:
		strcpy((char*)shortPhrase, t);
		if(sayit) acs_say_string(shortPhrase);
		return;
}

if(asword != 2 || c < 0x80) {
t = (char*)acs_getpunc(c);
if(t) goto copy_t;
}

if( c < 0x80 && acs_isalpha(c) && asword == 2) {
t = ow->natoWords[acs_tolower(c)-'a'];
goto copy_t;
}

if(c < 0x80 && acs_isalnum(c)) {
	if(sayit) {
if(acs_isupper(c) && asword == 1)
acs_say_string_n(ow->capWord);
c = acs_tolower(c);
acs_say_char(c);
}
// alphanum without sayit, you shouldn't be here.
return;
}

// We are past getpunc(), guess we don't know how to say this unicode.
// Just say it in hex.
sprintf(hexbuf, "%x", c);
strcpy(shortPhrase, ow->unicodeWord);
l = strlen(shortPhrase);
for(i=0; hexbuf[i]; ++i) {
shortPhrase[l++] = ' ';
shortPhrase[l++] = hexbuf[i];
}
shortPhrase[l] = 0;
			t =  shortPhrase;
		goto copy_t;
}


/*********************************************************************
Text buffer structures.
Holds input text and output text, as transformed by each pass.
*********************************************************************/

static struct textbuf tb1, tb2;

struct textbuf *tp_in = &tb1, *tp_out = &tb2;

void textBufSwitch(void)
{
	struct textbuf *save;
	save = tp_in;
	tp_in = tp_out;
	tp_out = save;
	memset(tp_out->offset, 0, tp_out->room*sizeof(acs_ofs_type));
	tp_out->buf[0] = 0;
	tp_out->len = 1;
}

static void carryOffsetForward(const unsigned int *s)
{
	acs_ofs_type offset = tp_in->offset[s - tp_in->buf];
	tp_out->offset[tp_out->len] = offset;
}

/* There's always room for the last zero */
void textbufClose(const unsigned int *s, int overflow)
{
	if(overflow) {
		/* Back up to the start of this token. */
		while(!tp_out->offset[tp_out->len]) {
			appendBackup();
			if(tp_out->len == 1) break;
		}
	} else carryOffsetForward(s);
	tp_out->buf[tp_out->len] = 0;
}


/*********************************************************************
Keep track of the current date.
This is done to simplify the reading of dates.
If we're talking about yesterday, we can just say "yesterday",
rather than month day year.
*********************************************************************/


/*********************************************************************
Check to see whether a word is contained in a list of words.
This is a case insensitive search.
If s_len> is 0, we ask whether the candidate word
has any of the strings as a left prefix.
Returns the index of the matching string,
or -1 if there is no match.
*********************************************************************/

static int wordInList(const char * const *list, const unsigned int *s, int s_len)
{
	const char *x;
	int i, len;

	for(i=0; (x = *list); ++list, ++i) {
		if((len = acs_substring_mix(x, s)) < 0) continue;
		if(!s_len || s_len == len) return i;
	} /* loop over words  in list */

	return -1;
}


/*********************************************************************
A dozen append() routines add strings, dates,
times, moneys, phone numbers, etc to the growing buffer.
They all return 1 if we run out of buffer.
*********************************************************************/

static int roomCheck(int n)
{
	unsigned int *buf;
	acs_ofs_type *ofs;
	int room;
	if(tp_out->len + n < tp_out->room) return 0;
	room = tp_out->room/3*4;
	buf = realloc(tp_out->buf, room * sizeof(unsigned int));
	if(!buf) return 1;
	ofs = realloc(tp_out->offset, room*sizeof(acs_ofs_type));
	if(!ofs) return 1;
	tp_out->buf = buf;
	tp_out->offset = ofs;
tp_out->room = room;
	return 0;
}

static int appendChar(unsigned int c)
{
	if(roomCheck(1)) return 1;
	tp_out->buf[tp_out->len++] = c;
	return 0;
}

/* append an isolated char or digit */
static int appendIchar(unsigned int c)
{
	if(roomCheck(2)) return 1;
	tp_out->buf[tp_out->len++] = c;
	tp_out->buf[tp_out->len++] = ' ';
	return 0;
}

/* Input is lower case utf8, output is the unicode buffer. */
static int appendString(const char *s)
{
	int n = strlen(s);
	if(roomCheck(n+1)) return 1;
	n = acs_utf82uni(s, tp_out->buf + tp_out->len);
	tp_out->len += n;
	tp_out->buf[tp_out->len++] = ' ';
	return 0;
}

static int appendIdigit(int n)
{
	return (ow->idigits[n] ?
appendString(ow->idigits[n]) :
appendIchar('0'+n));
}

/* Speak a string of digits.
In espeakup the string reads faster and smoother if it is in words,
so that's what we do. */
static int appendDigitString(const unsigned int *s, int n)
{
	unsigned int c;
	while(n--) {
		c = *s++;
		if(appendIdigit(c-'0')) return 1;
	}
	return 0;
}

static void lastUncomma(void)
{
	int len = tp_out->len;
	acs_ofs_type offset = tp_out->offset[len];
	unsigned int *s = tp_out->buf + len - 1;
	unsigned int c = *s;
	while(c == ' ') --len, c = *--s;
	if(c != ',') return;
	--len;
	tp_out->offset[len] = offset;
	tp_out->len = len;
}

static int appendAcronString(const char *s)
{
	char c;
	int n = strlen(s);
	if(roomCheck(2*n)) return 1;
	while(n--) {
		c = *s++;
		/* we assume c is an ascii letter */
		if(tp_acronUpper) c &= 0xdf;
		else c |= 0x20;
		tp_out->buf[tp_out->len++] = c;
		c = (n ? tp_acronDelim : ' ');
		tp_out->buf[tp_out->len++] = c;
	}
	return 0;
}

static int appendAcronCodes(const unsigned int *s, int n)
{
	unsigned int c;
	if(roomCheck(2*n)) return 1;
	while(n--) {
		c = *s++;
		/* we assume c is alpha */
		if(tp_acronUpper) c = acs_toupper(c);
		else c = acs_tolower(c);
		tp_out->buf[tp_out->len++] = c;
		c = (n ? tp_acronDelim : ' ');
		tp_out->buf[tp_out->len++] = c;
	}
	return 0;
}

/* Read a natural number, up to 3 digits. */
/* The dohundred parameter indicates 2 hundred 3 or 2 oh 3. */
/* The zero parameter indicates whether 0 will be read. */
static int append3num(int n, int dohundred, int zero)
{
	const char *q;
	int rc = 0;

	if(!n) {
		if(zero) return appendIdigit(0);
		return 0;
	}

// PT complicated; just feed to the synth.
if(acs_lang == ACS_LANG_PT_BR || acs_lang == ACS_LANG_SK) {
char buf[4];
sprintf(buf, "%d", n);
return appendString(buf);
}

	if(n >= 100) {
		rc |= appendIdigit(n/100);
		n %= 100;
		if(!n) dohundred = 1;
// Don't think there is anything like 2 oh 3 in languages other than english.
		if(acs_lang != ACS_LANG_EN) dohundred = 1;
		if(dohundred) rc |= appendString(ow->hundredWord); 
		else if(n < 10) rc |= appendString(ow->ohWord);
		if(!n) return rc;
	} /* hundreds */

	if(n < 10) return appendString(ow->idigits[n]);
	if(n < 20) return appendString(ow->teens[n-10]);

		q = ow->decades[n/10 - 2];
		n %= 10;
		if(!n) return appendString(q);

if(acs_lang == ACS_LANG_DE) {
rc |= appendString(ow->idigits[n]);
		rc |= appendString(ow->andWord);
		rc |= appendString(q);
} else {
		rc |= appendString(q);
rc |= appendString(ow->idigits[n]);
	}

	return rc;
}

/* read 09 as O 9, and 00 as o o */
static int appendOX(int n)
{
	int rc = 0;
	if(n < 10) rc |= appendString(ow->ohWord);
	if(n) rc |= append3num(n, 0, 0);
	else rc |= appendString(ow->ohWord);
	return rc;
}

/* Read a 4 digit number as a year. */
/* This is optimal for other 4-digit numbers, such as house numbers etc. */
static int appendYear(int y)
{
	int rc = 0;

if(acs_lang == ACS_LANG_PT_BR || acs_lang == ACS_LANG_SK) {
char buf[6];
sprintf(buf, "%d", y);
return appendString(buf);
}

	if(!((y%1000) / 100)) {
		if(y >= 1000) {
			rc |= appendIdigit(y/1000);
			rc |= appendString(ow->thousandWord);
		}
		y %= 100;
		rc |= append3num(y, 0, 0);
		return rc;
	} /* in the year 2007 */

// I don't think 2525 reads right in German 5 and 20 5 and 20
if(y >= 2000 && acs_lang == ACS_LANG_DE) {
unsigned int yd[4];
yd[0] = y / 1000 + '0';
y%= 1000;
yd[1] = y / 100 + '0';
y%= 100;
yd[2] = y / 10 + '0';
y%= 10;
yd[3] = y + '0';
return appendDigitString(yd, 4);
}

	rc |= append3num(y/100, 0, 0); /* century */
	y %= 100;
	if(!y) rc |= appendString(ow->hundredWord);
	else rc |= appendOX(y);
	return rc;
}

/* Read a natural number, up to 6 digits. */
static int append6num(int n)
{
	int rc = 0;
	int top = n/1000;
	int bottom = n%1000;
	if(top < 10) return appendYear(n);
	if(top) {
		rc |= append3num(top, 1, 0);
		rc |= appendString(ow->thousandWord);
	}
	rc |= append3num(bottom, 1, (int)!top);
	return rc;
}

/* append 3-digit ordinal, such as first, or seventeenth */
static int appendOrdinal(int n)
{
	int rc = 0;

	if(n >= 100) {
		rc |= appendIdigit(n/100);
		n %= 100;
		if(!n) { rc |= appendString(ow->hundredthWord); return rc; }
		rc |= appendString(ow->hundredWord);
	}

	if(n < 20) {
		rc |= appendString(ow->ordinals[n]);
		return rc;
	}

	if(n%10 == 0) {
		rc |= appendString(ow->orddecades[n/10-2]);
		return rc;
	}

	/* write the decade first */
	rc |= appendString(ow->decades[n/10 - 2]);
	rc |= appendString(ow->ordinals[n%10]);
	return rc;
}

/* read the nxx and xxxx of a phone number */
static int appendNxx(int n)
{
	int rc = 0;
	int h = n/100; /* h is nonzero */
	n %= 100;
	rc |= appendIdigit(h);
	if(!n) rc |= appendString(ow->hundredWord);
	else {
		rc |= appendIdigit(n/10);
		rc |= appendIdigit(n%10);
	}
	return 0;
}

static int appendXxxx(int n)
{
	int rc = 0;
	int t, h; /* thousands, hundreds */

	t = n / 1000;
	n %= 1000;
	h = n / 100;
	n %= 100;

	if(h) {
		if(t && (n/10 || !n)) {
			rc |= append3num(t*10 + h, 0, 0);
		} else {
			rc |= appendIdigit(t);
			rc |= appendIdigit(h);
		}

		if(!n) {
			rc |= appendString(ow->hundredWord);
			return rc;
		}

		if(t && n/10) {
			rc |= append3num(n, 0, 0);
		} else {
			rc |= appendIdigit(n/10);
			rc |= appendIdigit(n%10);
		}

		return rc;
	} /* second digit is nonzero */

	rc |= appendIdigit(t);

	if(!n) {
		if(t) rc |= appendString(ow->thousandWord);
		else rc |= appendString("0 0 0");
		return rc;
	} /* ends in 000 */

	rc |= appendIdigit(0);
	rc |= appendIdigit(n/10);
	rc |= appendIdigit(n%10);
	return rc;
}

static int appendFraction(int num, int den, int preand)
{
	int rc = 0;
	if(preand) rc |= appendString(ow->andWord);
	rc |= append3num(num, 1, 1);
	if(den > 1) {
		if(den == 2) rc |= appendString(ow->halfWord);
		else rc |= appendOrdinal(den);
	}
	if(num > 1) { appendBackup(); rc |= appendIchar('s'); }
	return rc;
}

static int alphaLength(const unsigned int *s)
{
	int len = 0;
	while(acs_isalpha(*s)) ++s, ++len;
	return len;
}

static const unsigned int *atoi_s;
static int atoiLength(const unsigned int *s, int len)
{
	int n = 0;
	while(len--) {
		if(!acs_isdigit(*s)) break;
		n = 10*n + *s - '0';
		++s;
	}
	atoi_s = s;
	return n;
}

/* Append "dollars and xx cents" to a money expression.
 * The number of dollars has already been spoken, as a natural number,
 * unless that number is zero.
 * The hard part of this routine is the plurality dollars.
 * You owe me $5.
 * Send me a $5 rebate.
 * The $5 will surely come in handy.
 * The $5 check is in the mail.
 * The subcontractor was given $8.5 million.
 * The subcontractor was awarded an $8.5 million contract.
 * Zeroflag indicates 0 dollars.
 * Oneflag indicates 1 dollar.
 * If the number of cents is negative, cents were not specified. */
static int appendMoney(int zeroflag, int oneflag, int cents, const unsigned int *q)
{
	int rc = 0;
	int pluralflag = 1;
	int j, len;
	unsigned int c;
	const unsigned int *s = q;

	if(zeroflag && cents <= 0)
		rc |= appendIdigit(0);

	/* print dollar or dollars */
	if(!zeroflag || (zeroflag && cents <= 0)) {
		rc |= appendString(~oneflag & pluralflag ? ow->dollarsWord : ow->dollarWord);
		if(cents > 0) rc |= appendString(ow->andWord);
	} /* print dollars */

	if(cents > 0) {
		oneflag = (cents == 1);
		rc |= append3num(cents, 0, 0);
		rc |= appendString(~oneflag & pluralflag ? ow->centsWord : ow->centWord);
	} /* print cents */

	return rc;
}


/*********************************************************************
We need to decide whether a word is a native word
or an acronym, which should be read letter by letter.
Begin by storing the 2 and 3 letter words in tables.
These are the words that are "pronunceable", in the native language.
These are in the outward table real2 and real3.
I downshift the letters to english, because the accents don't change
the plausibility of that sequence of letters being pronuncible.
Thus the real words in the table are ascii,
even if they aren't in the language itself.
Thus für German would be in the table as fur.
The 3 letter words have codes on when they might be acronyms.
> appended to a word means the word is spelled only when it is in upper case.
If an ^ is appended, we also require a context
of lower case words.
For example, "The ERA almost passed in the Reagan era."
Note, some of these aren't words at all,
but they should, nonetheless, be pronounced, rather than spelled.
Ron and Tim are perfectly good names.
ROM and SIM are well-known computer acronyms.
TAM is an acronym that I never heard of, but if it comes up
in your line of work, I'm betting you will pronounce it,
rather than saying t.a.m.
*********************************************************************/

/* is a 2-letter word native? */
static int isWord2(const unsigned int *s)
{
	char c1 = acs_unaccent(s[0]);
	char c2 = acs_unaccent(s[1]);
const char *w = ow->real2;

if(!w) return 1; // nothing to check against

	for(; *w; w+=2)
		if(w[0] == c1 && w[1] == c2) return 1;

	return 0;
}


static int isWord3(const unsigned int *s)
{
unsigned int c0;
	char c1 = acs_unaccent(s[0]);
	char c2 = acs_unaccent(s[1]);
	char c3 = acs_unaccent(s[2]);
	int lowbit = acs_islower(s[0]) | acs_islower(s[1]) | acs_islower(s[2]);
	short i;
const char *w;

if(c1 == ' ') return 0; // should not happen

w = ow->real3[(c1|0x20)-'a'];
if(!w) return -1; // nothing to check against

	for(; *w; w+=4) {
		if(w[1] != c2) continue;
		if(w[2] != c3) continue;

		/* we've got the right 3 letters */
		c1 = s[3];
		if(c1 == ' ') return 1;
		if(lowbit) return 1;
		if(c1 == '>') return 0;
		/* check for lower case context */
		for(i=0; i<10; ++i) {
			c0 = *--s;
			if(!c0) break;
if(acs_isalpha(c0) && acs_islower(c0))
return 0;
		}

		return 1;
	} /* loop looking for this 3-letter word */

	return 0;
}

/*********************************************************************
This 32x32 matrix records the reasonable letter pairs in the native language.
Bit 4 means the pair can start a word.
Bit 2 means the pair can appear in the middle of a word.
Bit 1 means the pair can end a word.
The transitionValue() function normalizes
characters by removing umlauts and accent marks.
In other words, a often appears between m and d iff
a umlaut often appears between m and d, as in madchen.
The umlauted letter is treated just like the letter.
This is only an approximation, but I think it's good enough.
This should be carefully reviewed for each language.
For now I'm assuming one transition fits all,
at least for western languages.
*********************************************************************/

static const unsigned char letterPairs[26][32] = {
	{0,7,7,7,4,7,7,0,6,0,3,7,7,7,0,7,0,7,7,7,6,6,7,7,3,6},
	{7,2,0,0,7,0,0,0,7,0,0,6,0,0,7,0,0,6,3,0,6,0,0,0,7,0},
	{7,0,2,0,7,0,0,7,6,0,3,6,0,0,7,0,2,6,3,3,6,0,0,0,7,0},
	{7,0,0,3,7,0,2,0,6,0,0,0,0,0,7,0,0,6,3,0,6,0,0,0,7,0},
	{7,7,7,7,7,7,7,0,6,0,7,7,7,7,2,7,6,7,7,7,4,6,3,7,3,0},
	{7,0,0,0,7,3,0,0,6,0,0,6,0,0,7,0,0,6,0,3,6,0,0,0,1,0},
	{7,0,0,0,7,0,2,3,7,0,0,6,0,0,7,0,0,6,3,0,6,0,0,0,7,0},
	{7,0,0,0,7,0,0,0,6,0,0,0,0,0,7,0,0,2,0,3,6,0,0,0,7,0},
	{3,3,7,7,3,7,7,0,0,0,7,7,7,7,7,7,2,7,7,7,0,6,0,3,0,3},
	{6,0,0,0,6,0,0,0,6,0,0,0,0,0,6,0,0,0,0,0,6,0,0,0,0,0},
	{7,0,0,0,7,0,0,0,7,0,0,0,0,6,7,0,0,0,7,0,6,0,0,0,1,0},
	{7,0,0,3,7,3,0,0,6,0,1,3,3,0,7,3,0,0,3,3,6,2,0,0,7,0},
	{7,3,0,0,7,0,0,0,7,0,0,0,2,0,7,3,0,0,3,0,6,0,0,0,7,0},
	{7,0,2,3,7,2,3,0,7,0,3,0,0,2,7,0,0,0,3,3,6,0,0,0,3,0},
	{6,7,7,7,3,7,7,7,7,0,7,7,7,7,6,7,0,7,7,7,6,6,7,7,3,0},
	{7,0,0,0,7,0,0,7,7,0,0,6,0,0,7,2,0,6,7,3,6,0,0,0,7,0},
	{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,6,0,0,0,0,0},
	{7,3,3,3,7,3,3,6,7,0,3,3,3,3,7,3,0,2,3,3,6,2,0,0,7,0},
	{7,0,7,0,7,0,0,7,7,0,7,6,6,6,7,7,6,0,3,7,6,0,4,0,7,0},
	{7,0,0,0,7,0,0,7,7,0,0,2,0,0,7,0,0,6,3,2,6,0,4,0,7,0},
	{2,3,3,3,3,3,3,0,2,0,0,7,7,7,0,7,0,7,7,7,0,0,0,3,0,3},
	{7,0,0,0,7,0,0,0,6,0,0,0,0,0,7,0,0,0,0,0,0,0,0,0,1,0},
	{7,0,0,3,7,0,0,6,6,0,0,3,0,3,6,0,0,4,3,2,0,0,0,0,0,0},
	{3,0,2,0,3,0,0,0,3,0,0,0,0,0,2,2,0,0,0,3,0,0,0,0,1,0},
	{7,0,0,0,7,0,2,0,2,0,0,0,3,3,7,0,0,0,3,2,0,0,0,0,0,0},
	{7,0,0,0,7,0,0,0,6,0,0,0,0,0,7,0,0,0,0,0,0,0,0,0,7,3},
};

static int transitionValue(char x, char y)
{
	/* paranoia */
	if(x == ' ' || y == ' ') return 0;
	return letterPairs[x-'a'][y-'a'];
}


/*********************************************************************
Do the first 2 or 3 letters obey the above rules?
This is used to analyze login@domain.  For instance,
pronounce mjordan@basketbal.com as m jordan, since mj doesn't start a word.
*********************************************************************/

static int leadSequence(const unsigned int *s, int n)
{
	char c1 = acs_unaccent(s[0]);
	char c2 = acs_unaccent(s[1]);
	char c3 = acs_unaccent(s[2]);

	if(!(transitionValue(c1, c2)&4)) return 0;
	if(n == 3 && !(transitionValue(c2, c3)&2)) return 0;
	return 1;
}


/*********************************************************************
Analyze a string of letters and apostrophes.
Is it a native word?
The string may have other letters following it.
This happens when we analyze runTogetherWords.
We specify the length, rather than assuming it is null terminated.
We try to analyze the word no matter its length --
but the calling function will not acronize any word longer than 6 letters.
Nobody wants to hear that many letters in a row.
Garbage is garbage -- you'll get through it faster by pronouncing it.
Therefore, this routine only HAS to work for words up to 6 letters.
The 2 and 3 letter cases are table driven,
and there's some heuristics for the 4 letter words.
Longer words use the transition matrix above.
This has lots of English assumptions in it.
*********************************************************************/

static const char *icc3[] = {
"chl","chr","phl","phr",
"sch","scl","scr","shr","shw","sph","spl","spr","str",
"thr","thw",0};

static const char *icc4[] = {
"schl","schr","schw",0};

static int isPronounceable(const unsigned int *s, int len)
{
	int i, cnt;
	char c1, c2, c3;

	/* check for words with apostrophes in them */
	c1 = c2 = 0;
	for(i=1; i<len; ++i)
		if(acs_unaccent(s[i]) == '\'')
++c1, c2 = i;
	if(!c1) goto no_apos;
	if(c1 > 1) return 0;
	i = c2;
	if(i == len-2 &&
	acs_tolower(s[len-1]) == 's') {
		/* analyze the word without its trailing 's */
		len -= 2;
		goto no_apos;
	}
	if(i == len-3 &&
	acs_tolower(s[len-1]) == 'l' &&
	acs_tolower(s[len-2]) == 'l') {
		/* analyze the word without its trailing 'll */
		len -= 3;
		goto no_apos;
	}
	if(i == 1 && acs_tolower(s[0]) == 'o') {
		/* analyze the word without its leading o' */
		len -= 2;
		s += 2;
		goto no_apos;
	}

	/* word must be in the list to be pronunceable */
return wordInList(ow->contractions, s, len) >= 0;

	no_apos:
	if(len == 1) return 1;
	if(len == 2) return isWord2(s);

	/* Simple vowel check */
	c1 = 0;
	for(i=cnt=0; i<len; ++i) {
		if(!acs_isvowel(s[i])) continue;
		if(!cnt) c1 = i;
		++cnt;
	}
	if(!cnt) return 0; /* no vowels */

	/* If it's a name, try to say it anyways */
	if(acs_isupper(s[0]) && acs_islower(s[1])) {
		if((len == 3 && acs_islower(s[2])) ||
		(len > 3 && acs_islower(s[3]))) return 1; /* name */
	} /* upper lower */

	if(len == 3) {
i = isWord3(s);
if(i >= 0) return i;
}

	/* are there too many leading consonents? */
  return 1;
	/* We'll cut you some slack on McGruff. */
	if(acs_tolower(*s) == 'm' && acs_tolower(s[1]) == 'c')
		c1 -= 2, s += 2;
// can't start with 5 consonents.
	if(c1 > 4) return 0;
	if(c1 > 2) {
		if(wordInList(c1 == 3 ? icc3 : icc4, s, 0) < 0) return 0;

		/* special case, lots of consonents and only one vowel. */
		if(acs_substring_mix((char*)"strength", s) > 0) return 1;
	}

	if(cnt*3 + 5 < len) return 0; /* not enough vowels */
//  	if(cnt == len) return 0; /* all vowels */

	/* count the invalid transitions */
	cnt = 0;
	c1 = acs_unaccent(*s);
	c2 = acs_unaccent(*++s);
	i = 3;
	/* Allow Mc at the start of a longer word. */
	if(c1 == 'm' && c2 == 'c' && len >= 5) {
	c1 = acs_unaccent(*++s);
	c2 = acs_unaccent(*++s);
		i += 2;
	}
	if(!(transitionValue(c1, c2)&4)) ++cnt;
	for(++s; i<len; ++i, ++s) {
		c1 = c2;
		c2 = acs_unaccent(*s);
		if(!(transitionValue(c1, c2)&2)) ++cnt;
	}
	c1 = c2;
	c2 = acs_unaccent(*s);
	if(!(transitionValue(c1, c2)&1)) {
		++cnt;
		if(len == 4) return 0;
	}

	if ((len+2)/3 <= cnt) return 0;
	return 1;
}


/*********************************************************************
Expand a word.  More accurately, a token that starts with a letter.
If the lead character is a digit, expand the number.
Return 1 on overflow.
Pass back the updated pointer, just after the input token.
*********************************************************************/

static int expandAlphaNumeric(unsigned int **sp)
{
	unsigned int c, d, e, f;
	unsigned int *start = *sp, *end; /* bracket the token */
	unsigned int *q;
	const unsigned int *ur; /* user replacement */
	unsigned int *casecut = 0, *comma = 0, *apos = 0;
	int i, j, value;
	int zeroflag, oneflag, hundredflag;
	int rc;
char nbc = " ,,.,"[acs_lang]; // number block character
char ndc = " ..,."[acs_lang]; // number decimal character

	e = 0; /* quiet gcc */

	c = *start;
	if(acs_isalpha(c)) goto alphaToken;

	/* Check for 1st 2nd etc. */
	d = start[-1];
	e = start[1];
	end = start + 1;
	i = c - '0';
	if(acs_isdigit(e)) {
		i = 10*i + e-'0';
		e = *++end;
	}
	if(acs_isdigit(e)) {
		i = 10*i + e-'0';
		e = *++end;
	}
	if(acs_isalpha(e) && acs_isalpha(end[1]) &&
	!acs_isalnum(end[2])) {
		e = acs_tolower(e);
		f = acs_tolower(end[1]);
		if((e == 's' && f == 't') ||
		(e == 'n' && f == 'd') ||
		(e == 'r' && f == 'd') ||
		(e == 't' && f == 'h')) {
char g = end[-1];
if(			(g == '0' && e == 't') ||
			(g == '1' && (e == 's' || (end > start+1 && e == 't'))) ||
			(g == '2' && (e == 'n' || (end > start+1 && e == 't'))) ||
			(g == '3' && (e == 'r' || (end > start+1 && e == 't'))) ||
			(g > '3' && e == 't')) {
				end += 2;
				if(appendOrdinal(i)) goto overflow;
				goto success;
			} /* correct ending for a one-digit number */
		} /* ends in st or nd or rd or th */
	} /* 1 or 2 or 3 digits followed by 2 letters */

	/* find the start and end of this number */
	if(d == ',' || d == '.' || acs_isalpha(d) ||
	tp_oneSymbol) comma = start;
	if(d == '-' && acs_isalnum(start[-2])) comma = start;
	if(c == '0') comma = start;
	for(end=start+1; (e = *end); ++end) {
		if(acs_isdigit(e)) continue;
		if(e != nbc) break;
		if(!acs_isdigit(end[1])) break;
		if(comma == start) continue;
		if(!comma) { /* first comma */
			comma = end;
			if(comma - start > 3) comma = start;
			continue;
		}
		/* subsequent comma */
		if(end - comma != 4) comma = start;
		else comma = end;
	} /* loop gathering digits and commas */
	if(comma && end - comma != 4) comma = start;

	if(comma && comma > start) {
		if(acs_isalpha(e)) comma = start;
		if(e == '-' && acs_isalnum(end[1])) comma = start;
		if(end - start > 19) comma = start; /* I don't do trillions */
		/* int foo[] = {237,485,193,221}; */
		if(tp_readLiteral && end - start > 7) comma = start;
	}

	/* Bad comma arrangement?  Read each component. */
	if(comma == start) {
		for(q=start; q<end; ++q)
			if(*q == nbc) { end = q; break; }
		comma = 0;
	}

	if(end - start > WORDLEN) end = start + WORDLEN;
	e = *end;

	if(comma) {
		zeroflag = oneflag = 1;
		while(start < end) { /* procede by groups of 3 */
			i = 0;
			do { /* gather the next group of 3 */
				i = 10*i + *start - '0';
				++start;
			} while ((end-start) % 4);
			if(i) zeroflag = 0;
			if(start == end && i != 1) oneflag = 0;
			if(start < end && i) oneflag = 0;
			if(append3num(i, 1, 0)) goto overflow;
			if(i && start < end) {
				i = (end-start) / 4 - 1;
				if(appendString(ow->bigNumbers[i])) goto overflow;
			}
			++start;
		} /* loop over groups of 3 */
		if(d == '$') goto money;
		if(zeroflag && appendIdigit(0)) goto overflow;
		goto success;
	} /* comma formatted number */

#if 0
	/* read 19980502, when part of a filename. */
	if(end-start == 8 && start[6] <= '3' && start[4] <= '1' &&
	((start[0] == '1' && start[1] == '9') || (start[0] == '2' && start[1] == '0'))) {
		if(acs_isalpha(d) || acs_isalpha(e) ||
		(d == '.' && acs_isalnum(start[-2])) ||
		(e == '.' && acs_isalnum(end[1]))) {
			i = atoiLength(start, 4);
			if(appendYear(i)) goto overflow;
			i = atoiLength(start+4, 2);
			if(appendOX(i)) goto overflow;
			i = atoiLength(start+6, 2);
			if(appendOX(i)) goto overflow;
			goto success;
		} /* number part of a larger token */
	} /* 19yymmdd */
#endif

	/* If you wanted me to read thousands and millions,
	 * you should have used commas. */
	if(end - start > 4) goto copydigits;

	/* starts with 0, read all the digits */
	if(c == '0' && (end - start == 4 || d != '$')) goto copydigits;

	/* read digits after the decimal point */
	if(d == ndc) {
		/* Unless we are in the mids of 192.168.9.3 */
		if(e == '.' && acs_isdigit(end[1])) goto copynumber;
		q = start-2;
		if(!acs_isdigit(*q)) goto copydigits;
		do { --q; } while(acs_isdigit(*q));
		if(*q != '.') goto copydigits;
copynumber:
		value = atoiLength(start, end-start);
		if(appendYear(value)) goto overflow;
		if(!value && appendIdigit(0)) goto overflow;
		goto success;
	} /* leading decimal point */

	/* read digits before the decimal point */
	if(e == '.' && d != '$') {
		if(acs_isdigit(end[1])) goto copynumber;
	}

	/* speak digits in coded numbers, such as social security 374-81-6339.
	 * We assume English text, rather than a mathematical formula
	 * such as 374-28-2929 = -2583. */
	if(d == '-' && e == '-') goto copydigits;
	if(d == '-' && acs_isdigit(start[-2])) {
		q = start-3;
		while(acs_isalnum(*q)) {
			if(acs_isalpha(*q)) goto copydigits;
			--q;
		}
		if(*q == '-') goto copydigits;
	}
	if(e == '-' && acs_isdigit(end[1])) {
		q = end+2;
		while(acs_isalnum(*q)) {
			if(acs_isalpha(*q)) goto copydigits;
			++q;
		}
		if(*q == '-') goto copydigits;
	}

	/* speak the number naturally */
	i = end - start;
	value = atoiLength(start, i);
	zeroflag = !value;
	oneflag = (value == 1);
	if(i == 4) {
		if(appendYear(value)) goto overflow;
		if(d == '$' && i < 4) goto money;
		appendBackup();
		goto possessive; /* the 1970's */
	} /* 4 digits */

	/* Do we read 302 as 3 o 2 or 3 hundred 2? */
	/* Apply the latter if we have a word either side: I ate 302 cookies */
	hundredflag = 0;
	if(i == 3 && c != '0') {
		if(d == '$') { hundredflag = 1; goto past3; }
		if(d == '-' || d == '#') goto past3;
		if(e == '-' || e == '\'') goto past3;
		if(acs_isalpha(d) || acs_isalpha(e)) goto past3;
		if(acs_isspace(d) && acs_isalpha(start[-2])) {
			hundredflag = 1;
			/* Unless we find a keyword like room 302 */
			q = start-2;
			i = 0;
			do --q, ++i; while(acs_isalpha(*q));
			if(wordInList(ow->nohundred, ++q, i) >= 0)
				hundredflag = 0;
			goto past3;
		}
		if(acs_isspace(e) && acs_isalpha(end[1])) {
			hundredflag = 1;
		}
	} /* three digits */
past3:

	if(append3num(value, hundredflag, 0)) goto overflow;
	if(d == '$' && !tp_oneSymbol) {
		if(!tp_readLiteral) goto money;
		if(end-start == 2 || end-start == 3) goto money;
		if(e == '.' && acs_isdigit(end[1])) goto money;
		/* read $3 as dollar three, a positional parameter */
	}
	if(zeroflag && appendIdigit(0)) goto overflow;

	appendBackup();
	goto possessive;

copydigits:
	if(d == '$' && !tp_readLiteral &&
	appendString(ow->dollarWord)) goto overflow;
	if(appendDigitString(start, end-start)) goto overflow;
	appendBackup();
	goto possessive;

money:
	/* Note that 0, as in $0, has not yet been spoken. */
	/* This allows us to read $0.39 as 39 cents. */
	i = -1;
	/* Check for $4K or $8.5M. */
	if(!comma) {
		int moneySuffix = -1;
		q = end;
		if(*q == '.') {
			++q;
			if(acs_isdigit(*q)) i = *q++ - '0';
			if(acs_isdigit(*q)) i = 10*i + *q++ - '0';
		} /* .xx after the number */
		e = *q;
		if(e == '\n' || e == ' ' || e == '-')
			e = *++q;
		if(acs_isalpha(e)) {
			/* look for the word million */
			j = alphaLength(q);
			/* This is rather unusual; we use bigNumber[]
			 * for both input and output. */
			if(j > 1) {
				moneySuffix = wordInList( ow->bigNumbers, q, j);
			} else if(!acs_isalnum(q[1])) {
				e = acs_toupper(e);
				if(e == 'K') moneySuffix = 0;
				if(e == 'M') moneySuffix = 1;
				if(e == 'B') moneySuffix = 2;
			}
			if(moneySuffix >= 0) {
				q += j;
				if(i > 0) {
					if(appendString(ow->pointWord)) goto overflow;
					if(i >= 10 && appendIdigit(i/10)) goto overflow;
					if(i < 10 && end[1] == '0'&&
					appendString(ow->ohWord)) goto overflow;
					i %= 10;
					if(appendIdigit(i)) goto overflow;
				} /* point xx */
				if(appendString(ow->bigNumbers[moneySuffix])) goto overflow;
				if(appendMoney(0, 0, -1, q)) goto overflow;
				end = q;
				goto success;
			} /* valid money suffix */
		} /* letter follows money */
	} /* no commas in the money number */

	/* determine how many cents are present */
	i = -1;
	if(*end == '.' &&
	acs_isdigit(end[1]) && acs_isdigit(end[2]) &&
	!acs_isdigit(end[3])) {
		i = atoiLength(end+1, 2);
	end += 3;
	} /* .xx follows */
	if(appendMoney(zeroflag, oneflag, i, end)) goto overflow;
	goto success;

alphaToken:
	c = acs_tolower(c);
	/* Special case, PH.D.
	 * In this and the next section, we assume a prior phase
	 * has stripped off the last period, unless this word really
	 * marks the end of the sentence.
	 * Thus we compare with "Ph.d", without the last period. */
	if(c == 'p' &&
	acs_substring_mix((char*)"ph.d", start) > 0 &&
	!acs_isalnum(start[4])) {
		if(appendAcronString((char*)"phd")) goto overflow;
		end = start + 4;
		goto success;
	}

	/* check for U.S.A etc */
	end = start;
	while(end[1] == '.' && acs_isalpha(end[2]))
		end += 2;
	if(end - start >= 2 && !acs_isalnum(end[1])) {
		if(!tp_readLiteral || end - start > 2 || acs_isupper(*start)) {
			++end;
			/* check for e.g. and i.e. */
			d = acs_tolower(start[2]);
			if(c == 'e' && d == 'g') {
				if(appendString(ow->egWord)) goto overflow;
				goto success;
			}
			if(c == 'i' && d == 'e') {
				if(appendString(ow->ieWord)) goto overflow;
				goto success;
			}
			/* speak each letter */
			for(; start < end; start+=2) {
				c = *start;
				if(tp_acronUpper) c = acs_toupper(c);
				else c = acs_tolower(c);
				if(appendIchar(c)) goto overflow;
				if(start < end-1) tp_out->buf[tp_out->len-1] = tp_acronDelim;
			}
			appendBackup();
			goto possessive;
		} /* reading the word without the dots */
	} /* processing an A.B.C style word */

	/* find the end of the word */
	f = c = *start;
	d = start[-1];
	for(end=start+1; (e = *end); ++end, f=e) {
		if(acs_isalpha(e)) {
			if(case_different(e, f) && !casecut && acs_isalpha(f)) {
				if(acs_isupper(e)) {
					casecut = end;
					/* Don't use casecut for McDonalds. */
					if(end == start+2 &&
					acs_tolower(start[0]) == 'm' && start[1] == 'c')
						casecut = 0;
				} else if(end-start >= 2) {
					casecut = end-1;
					/* Again, some McDonalds code */
					if(end-start == 3 &&
					acs_tolower(start[0]) == 'm' &&
					start[1] == 'c')
						casecut = 0;
				}
			} /* letters have different case */
			continue;
		} /* another letter */
		if(acs_unaccent(e) == '\'') {
			if(!apos) apos = end;
			continue;
		}
		break;
	} /* loop finding the end of the word */

	/* strip out trailing apostrophes */
	while(acs_unaccent(end[-1]) == '\'') {
		if(end-start >= 5 && !tp_readLiteral &&
		acs_tolower(end[-2]) == 'n' &&
		acs_tolower(end[-3]) == 'i') {
			f = 'G';
			if(acs_islower(end[-2])) f = 'g';
			end[-1] = f;
			break;
		} else {
			--end;
			if(end == apos) apos = 0;
		}
	}

	/* Strip off 's */
	/* The possessive code will put it back on after translation. */
	if(end-start > 2 && acs_tolower(end[-1]) == 's' && acs_unaccent(end[-2]) == '\'') {
		end -= 2, e = '\'';
		if(apos == end) apos = 0;
	}
	if(end-start > 3 && acs_unaccent(end[-3]) == '\'' &&
	acs_tolower(end[-1]) == 'l' && end[-1] == end[-2]) {
		end -= 3, e = '\'';
		if(apos == end) apos = 0;
	}

	/* check for plural acronym, as in PCs, and back over the final s */
	if(casecut == end-2 && end[-1] == 's' && acs_isupper(end[-3]))
	--end, casecut = 0;

	/* special code for 401Ks */
	if(acs_isdigit(d) && end-start == 2 &&
	acs_isupper(c) && start[1] == 's')
		--end;

	/* Check the entire word, and then the first piece of the
	 * runTow->ogetherWord.  Thus readingRainbow will be
	 * transmuted into reeding rainbow, via read -> reed. */
	while(1) {
		ur = acs_replace(start, end-start);
		if(ur) {
			j = acs_unilen(ur);
			if(roomCheck(j)) goto overflow;
			memcpy(tp_out->buf + tp_out->len, ur, j*sizeof(unsigned int));
			tp_out->len += j;
			goto possessive;
		} /* user replaced the entire word */
		if(!casecut) break;
		end = casecut;
		casecut = 0;
		if(apos && apos >= end) apos = 0;
	}

	e = *end;
	if(apos) { /* interior apostrophe */
		/* If it's not a native word, or it has yet more apostrophes
		 * around it, read each component. */
		if(acs_unaccent(d) == '\'' || acs_unaccent(d) == '\'' ||
		!isPronounceable(start, end-start))
			end = apos, apos = 0, e = '\'';
	} /* interior apostrophe */

	/* str76at  s t r 7 6 a t */
	if(acs_isdigit(d) && end-start < 3) goto acronym;

	/* A word cannot be too long */
	if(end - start > WORDLEN)
		end = start + WORDLEN, e = *end;

	/* Check for mjordan@domain.com. */
	if(e == '@' && end - start >= 4 && acs_isalpha(end[1])) {
		char leadLetters[3];
		leadLetters[0] = 0;
		if(!leadSequence(start, end-start)) {
			leadLetters[0] = c;
			leadLetters[1] = 0;
			if(!leadSequence(start+1, end-start-1)) {
				leadLetters[1] = start[1];
				leadLetters[2] = 0;
			}
		}
		i = strlen(leadLetters);
		if(i) {
			if(appendAcronString(leadLetters)) goto overflow;
			start += i;
		}
	} /* followed by @ */

	if(isPronounceable(start, end-start)) goto copyword;

	/* Don't bother spelling out long words;
	 * the user hasn't got the time or the patience. */
	i = end - start;
	if(i > 6) goto copyword;

	/* in a hyphenated word such as dis-obedient, the dis doesn't
	 * look like a valid English word, so it gets acronized.
	 * Check for this here, and jump to copyword. */
	if(acs_unaccent(e) == '-' && i <= 3 && i > 1 && acs_isalpha(end[1])) {
		if(leadSequence(start, i))
			goto copyword;
	}

acronym:
	rc = appendAcronCodes(start, end-start);
	if(rc) goto overflow;
	appendBackup();
	goto possessive;

copyword:
	/* copy a word into the buffer, shiftint to lower case */
	for(; start<end; ++start) {
		c = *start;
		if(acs_isalpha(c)) c = acs_tolower(c);
		if(appendChar(c)) goto overflow;
	}

possessive:
	/* check for lower s after an acronym,
	 * as in plural PCs. */
	if(e == 's' && !acs_isalnum(end[1]) &&
	acs_isalpha(end[-1])) {
		++end;
		if(appendString("'s")) goto overflow;
		goto success;
	}

	/* check for 's or 'll */
	if(acs_unaccent(e) == '\'' && acs_tolower(end[1]) == 's' && !acs_isalnum(end[2])) {
		end += 2;
		if(appendString("'s")) goto overflow;
		goto success;
	}
	if(acs_unaccent(e) == '\'' && acs_tolower(end[1]) == 'l' && end[1] == end[2] && !acs_isalnum(end[3])) {
		end += 3;
		if(appendString("'ll")) goto overflow;
		goto success;
	}

	appendChar(' ');

success:
	*sp = end;
	return 0;

overflow:
	return 1;
}


/*********************************************************************
Expand a punctuation mark.
If in literal mode, we simply read it.
Otherwise we may look around for context.
For instance, hyphen may be read as nothing, pause, minus, or dash.
This routine assumes several phases have run before.
Sequences of a repeating punctuation mark should be reduced to one,
or at most two.
The "range" phase should have replaced some of the hyphens with range codes,
as in Nov 21-25.
Return 1 on overflow.
Pass back the updated pointer, just after the punctuation mark,
or passed any additional characters that are swallowed.
*********************************************************************/

static int expandPunct(const unsigned int **sp)
{
	unsigned int c, d, e;
	const unsigned int *s = *sp;
	const unsigned int *end = s+1;
	const unsigned int *t;
	char spaceAround;
	int len, leftnum, rightnum, leftlen, rightlen;

	c = s[0];
	d = s[-1];
	e = s[1];

	if(tp_readLiteral) {
		/* Here are the exceptions */
		if(tp_oneSymbol |c >= 0x80 || !strchr(".^$", (char)c)) {
do_punct:
			d = acs_unaccent(c);
			if(!tp_oneSymbol &&
			!acs_getpunc(c) &&
			c >= 0x100 &&
			!isalnum(d))
				c = d;
			speakChar(c, 0, 0, 0);
				if(appendString(shortPhrase)) goto overflow;
			goto success;
		}
	}

	switch(c) {
	case '(':
		if(tolower(e) == 's' && s[2] == ')' && acs_isalpha(d)) {
			/* fill out the form(s) */
			appendBackup();
			if(appendIchar('s')) goto overflow;
			++end;
			break;
		}

	case '[': case '{':
		if(acs_isspace(d)) {
			do_comma: if(appendIchar(',')) goto overflow;
		}
		break;

	case ')': case ']': case '}':
		if(acs_isalnum(d) &&
		s[-2] && s[-2] < 0x80 && strchr("([{", s[-2]))
			break; /* the other side of (s) */
		if(d != ' ') goto do_comma;
		break;

	case '"':
		if(acs_isspace(d) && acs_isalnum(e)) {
			/* Which do we hit first, space or quote? */
			for(t=end; (c = *t); ++t) {
				if(c == '"') break;
				if(acs_isspace(c)) break;
			}
		if(acs_isspace(c)) goto do_comma;
		}
		if(acs_isalnum(d) && acs_isspace(e)) {
			for(t=s-1; (c = *t); --t) {
				if(c == '"') break;
				if(acs_isspace(c)) break;
			}
			if(acs_isspace(c)) goto do_comma;
		}
		break;

	case '-':
		/* Turn re-position into re position.
		 * Strange as it may seem, some people use a single -
		 * for a compressed period in a run-on sentence.
		 * Turn word-I into word.I */
		if(acs_isalpha(d) && acs_isalnum(e)) {
			if(e == 'I' && acs_isalpha(s[-2]) && acs_isspace(s[2])) {
				if(appendIchar('.')) goto overflow;
			}
			break;
		}

		/* Turn 10-year-old into 10 year old. */
		if(acs_isdigit(d) && acs_isalpha(e)) break;

		/* -37 becomes minus 37 */
		/* Same for -$37 and -x, but not -word. */
		if(acs_isspace(d) &&
		(acs_isalnum(e) || e == '$')) {
			if(acs_isalpha(e) && acs_isalpha(end[1])) break;
			if(appendString(ow->minusWord)) goto overflow;
			break;
		}

		/* Check for space on either side */
		t = s-1;
		++s;
		spaceAround = 0;
		if(d == ' ') { spaceAround = 2; d = *--t; }
		if(e == ' ') { spaceAround |= 1; e = *++s; }
		leftnum = rightnum = -1;
		leftlen = rightlen = 0; /* quiet gcc warning */
		if(acs_isdigit(e) && e != '0') {
			for(len=1; len<6; ++len)
				if(!acs_isdigit(s[len])) break;
			rightlen = len;
			if(len < 6) rightnum = atoiLength(s, len);
			c = s[len];
			if(acs_isalpha(c) || c == '-') rightnum = -1;
		}
		if(acs_isdigit(d)) {
			for(len=1; len<6; ++len)
				if(!acs_isdigit(t[-len])) break;
			leftlen = len;
			if(len < 6) leftnum = atoiLength(t-len+1, len);
			c = t[-len];
			if(acs_isalpha(c) || c == '-') rightnum = -1;
		}
		if(spaceAround == 2) break;
		if(spaceAround == 1) goto do_comma;
		if(leftnum > 0 && rightnum > leftnum &&
		(rightlen == leftlen ||
		(spaceAround && rightlen == leftlen+1))) {
				lastUncomma();
				if(appendString(ow->toWord)) goto overflow;
				break;
			}
		if(spaceAround) goto do_comma;
		if(!acs_isdigit(e)) goto do_comma;
		if(appendString(ow->dashWord)) goto overflow;
		break;

	case '$':
		/* This logic decides whether to read the word dollar.
		 * It is applicable only when tp_readLiteral is 1.
		 * Supress the word "dollar" if we're starting a money amount.
		 * Unfortunately this logic mirrors the logic in
		 * expandAlphaNumeric, which also decides whether a number
		 * is money or not.
		 * This duplicate logic is a bad design,
		 * but right now I can't think of a better one.
		 * First check for $.39 = 39 cents */
		if(e == '.' && acs_isdigit(end[1]) && acs_isdigit(end[2]) &&
		!acs_isalnum(end[3])) {
			if(appendMoney(1, 0,
			atoiLength(end+1, 2), end+3)) goto overflow;
			end += 3;
			break;
		}
		if(!acs_isdigit(e)) goto nomoney;
// now $3, but could be a positional parameter
		for(t=end+1; acs_isdigit(*t); ++t)  ;
		len = t - end;
		if(len > 3) goto nomoney; // $3456
		if(len > 1) break; // $34 or $345
// now looks like $3 and something, could be a parameter
		if(!tp_readLiteral) break;
		if(*t == '.' && acs_isdigit(t[1])) break; // $3.5
		/* Check for comma formatting. */
		if(*t != ',') goto nomoney;
		for((s = ++t); acs_isdigit(*t); ++t)  ;
		if(t-s != 3) goto nomoney;
		if(*t != ',') break;
		if(!acs_isdigit(t[1])) break;
// $345,678,digits
nomoney:
		if(tp_readLiteral) goto do_punct;
		break;

	case '.':
		/* turn . into dot or point */
		if(acs_isdigit(d) && acs_isdigit(e)) {
			/* Usually said as point. */
			/* But not in web addresses like 192.168.10.3 */
			for(t=end+1; acs_isdigit(*t); ++t) ;
			if(*t == '.' && acs_isdigit(t[1])) goto do_dot;
			for(t=s-2; acs_isdigit(*t); --t) ;
			if(*t == '.' && acs_isdigit(t[-1])) goto do_dot;
do_point:
			if(appendString(ow->pointWord)) goto overflow;
			break;
		}
		if(acs_isalnum(d) && acs_isalnum(e)) {
do_dot:
			if(appendString(ow->dotWord)) goto overflow;
			break;
		}
		if(acs_isdigit(e)) goto do_point;
		if(tp_readLiteral) goto do_punct;
		if(!e || acs_isspace(e)) goto copychar;
		break;

	case ';':
		if(acs_isspace(e)) goto do_comma;
		break;

	case '<': case '>': 	case '=':
		t = s-1;
		if(*t == ' ') --t;
		++s;
		if(*s == '=') ++s;
		if(*s == ' ') ++s;
		if(acs_isdigit(*t) || acs_isdigit(*s) ||
		(t[1] == ' ' && s[-1] == ' ')) {
			const char *w = ow->equalsWord;
			if(c == '<') w = ow->lessWord;
			if(c == '>') w = ow->greaterWord;
			if(appendString(w)) goto overflow;
			if(e == '=' && c != '=') {
				if(appendString(ow->oreqWord)) goto overflow;
			}
			end = s;
		}
		break;

	case '%':
		if(acs_isdigit(d) && !acs_isalnum(e)) {
			appendBackup();
			goto copychar;
		}
		break;

		case '@':
		if(acs_isalnum(d) && acs_isalnum(e))
			if(appendString(ow->atWord)) goto overflow;
		break;

		case '#':
		if(acs_isalnum(d)) break;
		if(acs_isdigit(e)) {
			if(appendString(ow->numberWord)) goto overflow;
			break;
		}
		if((e == 's' && !acs_isalnum(s[2])) ||
		(acs_unaccent(e) == '\'' && s[2] == 's' && !acs_isalnum(s[3]))) {
			if(appendString(ow->numbersWord)) goto overflow;
			++end;
			if(acs_unaccent(e) == '\'') ++end;
		}
		break;

	case '/':
		/* this/that becomes this or that */
		if(!acs_isalpha(d)) goto do_slash;
		if(!acs_isalpha(e)) goto do_slash;
		t = s-2;
		s += 2;
		leftnum = rightnum = 1;
		while(acs_isalpha(*t)) --t, ++leftnum;
		while(acs_isalpha(*s)) ++s, ++rightnum;
		if(leftnum == 1 && rightnum == 1) break; /* an A/B switch */
		if(leftnum == 1 || rightnum == 1) goto do_slash;
		d = *t, e = *s;
		if(d && d < 0x80 && !strchr("\"( \t\n", d)) goto do_slash;
		if(e && e < 0x80 && !strchr("\") \t\n.?!,;:", e)) goto do_slash;
		if(e == '.' && s[1] && !acs_isspace(s[1])) goto do_slash;
		/* move pointers to the start of the two words */
		s -= rightnum;
		++t;
		/* and/or is already set */
		if(rightnum == 2 && acs_substring_mix("or", s) == 2) break;
		if(wordInList(ow->slashOrPhrases, t, leftnum+rightnum+1) >= 0) {
			if(appendString(ow->orWord)) goto overflow;
			break;
		} /* predefined or phrase */
		if(case_different(*s, *t) ||
		case_different(s[rightnum-1], t[leftnum-1]))
			goto do_slash;
		if(leftnum < 4 || rightnum < 4) goto do_slash;
		if(!isPronounceable(t, leftnum)) goto do_slash;
		if(!isPronounceable(s, rightnum)) goto do_slash;
		if(appendString(ow->andWord)) goto overflow;
		break;
do_slash:
		if(appendString(ow->slashWord)) goto overflow;
		break;

	case '^': /* squared cubed etc */
		if(!acs_isalnum(d)) goto noexp;
		if(!acs_isdigit(e)) goto noexp;
		if(e == '0') goto noexp;
		rightnum = e - '0';
		t = end+1;
		e = *t;
		if(acs_isdigit(e)) rightnum = 10*rightnum + e - '0', e = *++t;
		if(acs_isdigit(e)) goto noexp;
		end = t;
		if(rightnum > 3 || rightnum < 2) {
			if(appendString(ow->toTheWord)) goto overflow;
			if(appendOrdinal(rightnum)) goto overflow;
		}
		if(rightnum == 2) {
			if(appendString(ow->squareWord)) goto overflow;
		}
		if(rightnum == 3) {
			if(appendString(ow->cubeWord)) goto overflow;
		}
			break;
noexp:
		if(tp_readLiteral) goto do_punct;
		break;

case ':':
		if(e == '/' || (acs_isalpha(d) && !acs_isalnum(s[-2]))) {
			if(appendString(ow->colonWord)) goto overflow;
			break;
		}
		if(acs_isspace(e)) goto do_comma;
		break;

	case '&':
		/* I hope TTS knows what to do with AT&T etc */
		if(!acs_isalnum(d)) break;
		if(!acs_isalnum(e)) break;
		appendBackup();
		goto copychar;

	case '!':
		if(d == ' ') break;
		if(acs_isalnum(d) && acs_isalnum(e)) {
			if(appendString(ow->bangWord)) goto overflow;
		}
		/* fall through */

	case ',': case '?':
	case '+':
copychar:
		if(appendIchar(c)) goto overflow;
		if(c == '&' && acs_isalnum(e)) appendBackup();
	} /* switch */

success:
	*sp = end;
	return 0;

overflow:
	return 1;
}


/*********************************************************************
Expand encoded constructs and render words and numbers.
This is generally the last phase of translation.
We assume there are no control characters,
except those that denote flavors of white space,
as described in another module.
And there should be no binary data.
We assume the calling routine has placed or moved the input text to tp_in,
whence we can write to tp_out.
See tc_textBufSwitch() near the top of this file.
*********************************************************************/

static void expandSentence(void)
{
	unsigned int *s;
	unsigned int c;
	int overflowValue = 1;

	s = tp_in->buf + 1;

	while((c = *s)) {
		if(c == '\t' && !tp_oneSymbol) c = ' ';
		if(c == ' ') goto nextchar;
		carryOffsetForward(s);
		if(c == '\n' || c == '\7') {
passThrough:
			if(appendChar(c)) goto overflow;
			goto nextchar;
		} /* physical newline or bell */

		if(c == '\f' && !tp_oneSymbol) {
			goto passThrough;
		} /* formfeed */

		if(c == '\n' && !tp_oneSymbol)goto passThrough;

		if(acs_isalnum(c)) {
			if(expandAlphaNumeric(&s)) goto overflow;
			continue;
		} /* word or number */

		if(expandPunct((const unsigned int **)&s)) goto overflow;
		continue;

nextchar:
		++s;
	} /* loop scanning input characters */

	overflowValue = 0;

overflow:
	textbufClose(s, overflowValue);
}


/*********************************************************************
Postprocess the text, to get rid of extra blanks
and multiple consecutive commas.
*********************************************************************/

static void postCleanup(void)
{
	unsigned int *s, *t;
	acs_ofs_type *u, *v;
	unsigned int c, d, e;
	char *w;
	static const char squishable[] = ",;:.?!";
	char presquish = 0, postsquish, insquish;

	s = t = tp_out->buf + 1;
	u = v = tp_out->offset + 1;
	d = '\f';

	for(; (c = *s); ++s, ++u) {
		insquish = postsquish = 0;

		if(ispunct(c)) {
			if(d == ' ') d = *--t, --v;
			if(acs_isspace(d)) continue;
			w = strchr(squishable, (char)c);
			if(w) insquish = w - squishable + 1;
		} /* punctuation */

		e = s[1];
		if(e) {
			w = strchr(squishable, (char)e);
			if(w) postsquish = w - squishable + 1;
		}

		if(c == ' ') {
			if(acs_isspace(d)) continue;
			if(acs_isspace(e)) continue;
			if(postsquish) continue;
			goto add_c;
		} /* space */

		if(acs_isspace(c)) presquish = 0;

		if(presquish && insquish) {
			if(insquish > presquish) {
				t[-1] = d = c;
				v[-1] = *u;
				presquish = insquish;
			}
			continue;
		}

		/* copy the character and the offset */
add_c:
		*t++ = d = c;
		*v++ = *u;
		presquish = insquish;
	} /* loop over chars in tp_out */

	if(d == ' ') --t, --v;
	*t = 0;
	*v = *u;
	tp_out->len = t - tp_out->buf;
}


/*********************************************************************
Prepare text for tts.
Run all the phases of translation.
*********************************************************************/

char debugPoint;
static acs_ofs_type end_ofs;
#define debugCheck(c, which) \
if(debugPoint == c) { \
int kk; \
printf("%s", which->buf+1); \
for(kk=1; kk<=which->len; ++kk) if(which->offset[kk]) printf("%d=%d\n", kk, which->offset[kk]); } \
if(end_ofs != which->offset[which->len]) { \
fprintf(stderr, "end offset inconsistency %c.%d.%d length %d\n", \
c, end_ofs, which->offset[which->len], which->len); \
exit(1); \
} \
if(debugPoint == c) exit(0)

void prepTTS(void)
{
	end_ofs = tp_in->offset[tp_in->len];

	debugCheck('a', tp_in);

	/* get ready for the first in->out transformation */
	memset(tp_out->offset, 0, tp_out->room*sizeof(acs_ofs_type));
	tp_out->buf[0] = 0;
	tp_out->len = 1;

	if(!tp_oneSymbol) {
		ascify();
		debugCheck('b', tp_in);
	} /* tp_oneSymbol */

	/* translate everything to alphanum text */
	expandSentence();
	debugCheck('y', tp_out);

	/* compress whitespace and sequences of commas and periods */
	postCleanup();
	debugCheck('z', tp_out);
}


unsigned int *prepTTSmsg(const char *msg)
{
int i, len;

/* I assume there is room for the message */
tp_in->buf[0] = 0;
len = acs_utf82uni(msg, tp_in->buf+1);
++len;
tp_in->len = len;
tp_in->buf[len] = 0;

	for(i=1; i<=len; ++i)
		tp_in->offset[i] = i;

	prepTTS();

	return tp_out->buf + 1;
}

