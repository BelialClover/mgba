/* Copyright (c) 2013-2014 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "LogView.h"

#include "LogController.h"
#include "LoadSaveState.h"
#include "Window.h"

#include <QTextBlock>

#include "platform/sdl/sdl-audio.h"
#include <thread>
#include <sqlite3.h>

using namespace QGBA;

LogView::LogView(LogController* log, Window* window, QWidget* parent)
	: QWidget(parent)
{
	m_ui.setupUi(this);
	connect(m_ui.levelDebug, &QAbstractButton::toggled, [this](bool set) {
		setLevel(mLOG_DEBUG, set);
	});
	connect(m_ui.levelStub, &QAbstractButton::toggled, [this](bool set) {
		setLevel(mLOG_STUB, set);
	});
	connect(m_ui.levelInfo, &QAbstractButton::toggled, [this](bool set) {
		setLevel(mLOG_INFO, set);
	});
	connect(m_ui.levelWarn, &QAbstractButton::toggled, [this](bool set) {
		setLevel(mLOG_WARN, set);
	});
	connect(m_ui.levelError, &QAbstractButton::toggled, [this](bool set) {
		setLevel(mLOG_ERROR, set);
	});
	connect(m_ui.levelFatal, &QAbstractButton::toggled, [this](bool set) {
		setLevel(mLOG_FATAL, set);
	});
	connect(m_ui.levelGameError, &QAbstractButton::toggled, [this](bool set) {
		setLevel(mLOG_GAME_ERROR, set);
	});
	connect(m_ui.clear, &QAbstractButton::clicked, this, &LogView::clear);
	connect(m_ui.advanced, &QAbstractButton::clicked, this, [window]() {
		window->openSettingsWindow(SettingsView::Page::LOGGING);
	});
	connect(m_ui.maxLines, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
	        this, &LogView::setMaxLines);
	m_ui.maxLines->setValue(DEFAULT_LINE_LIMIT);

	connect(log, &LogController::logPosted, this, &LogView::postLog);
	connect(log, static_cast<void (LogController::*)(int)>(&LogController::levelsSet), this, &LogView::setLevels);
	connect(log, static_cast<void (LogController::*)(int)>(&LogController::levelsEnabled), [this](int level) {
		bool s = blockSignals(true);
		setLevel(level, true);
		blockSignals(s);
	});
	connect(log, static_cast<void (LogController::*)(int)>(&LogController::levelsDisabled), [this](int level) {
		bool s = blockSignals(true);
		setLevel(level, false);
		blockSignals(s);
	});
	connect(this, &LogView::levelsEnabled, log, static_cast<void (LogController::*)(int)>(&LogController::enableLevels));
	connect(this, &LogView::levelsDisabled, log, static_cast<void (LogController::*)(int)>(&LogController::disableLevels));
}


//Stuff
const char* pokemonNames[] = {
    "",           // 0 is not a valid National Dex number
    "bulbasaur", "ivysaur", "venusaur", "charmander", "charmeleon", "charizard",
    "squirtle", "wartortle", "blastoise", "caterpie", "metapod", "butterfree",
    "weedle", "kakuna", "beedrill", "pidgey", "pidgeotto", "pidgeot",
    "rattata", "raticate", "spearow", "fearow", "ekans", "arbok",
    "pikachu", "raichu", "sandshrew", "sandslash", "nidoran♀", "nidorina", "nidoqueen",
    "nidoran♂", "nidorino", "nidoking", "clefairy", "clefable", "vulpix",
    "ninetales", "jigglypuff", "wigglytuff", "zubat", "golbat", "oddish",
    "gloom", "vileplume", "paras", "parasect", "venonat", "venomoth",
    "diglett", "dugtrio", "meowth", "persian", "psyduck", "golduck",
    "mankey", "primeape", "growlithe", "arcanine", "poliwag", "poliwhirl",
    "poliwrath", "abra", "kadabra", "alakazam", "machop", "machoke",
    "machamp", "bellsprout", "weepinbell", "victreebel", "tentacool", "tentacruel",
    "geodude", "graveler", "golem", "ponyta", "rapidash", "slowpoke",
    "slowbro", "magnemite", "magneton", "farfetch'd", "doduo", "dodrio",
    "seel", "dewgong", "grimer", "muk", "shellder", "cloyster",
    "gastly", "haunter", "gengar", "onix", "drowzee", "hypno",
    "krabby", "kingler", "voltorb", "electrode", "exeggcute", "exeggutor",
    "cubone", "marowak", "hitmonlee", "hitmonchan", "lickitung", "koffing",
    "weezing", "rhyhorn", "rhydon", "chansey", "tangela", "kangaskhan",
    "horsea", "seadra", "goldeen", "seaking", "staryu", "starmie",
    "mr. mime", "scyther", "jynx", "electabuzz", "magmar", "pinsir",
    "tauros", "magikarp", "gyarados", "lapras", "ditto", "eevee",
    "vaporeon", "jolteon", "flareon", "porygon", "omanyte", "omastar",
    "kabuto", "kabutops", "aerodactyl", "snorlax", "articuno", "zapdos",
    "moltres", "dratini", "dragonair", "dragonite", "mewtwo", "mew",
    "chikorita", "bayleef", "meganium", "cyndaquil", "quilava", "typhlosion",
    "totodile", "croconaw", "feraligatr", "sentret", "furret", "hoothoot",
    "noctowl", "ledyba", "ledian", "spinarak", "ariados", "crobat",
    "chinchou", "lanturn", "pichu", "cleffa", "igglybuff", "togepi",
    "togetic", "natu", "xatu", "mareep", "flaaffy", "ampharos",
    "bellossom", "marill", "azumarill", "sudowoodo", "politoed", "hoppip",
    "skiploom", "jumpluff", "aipom", "sunkern", "sunflora", "yanma",
    "wooper", "quagsire", "espeon", "umbreon", "murkrow", "slowking",
    "misdreavus", "unown", "wobbuffet", "girafarig", "pineco", "forretress",
    "dunsparce", "gligar", "steelix", "snubbull", "granbull", "qwilfish",
    "scizor", "shuckle", "heracross", "sneasel", "teddiursa", "ursaring",
    "slugma", "magcargo", "swinub", "piloswine", "corsola", "remoraid",
    "octillery", "delibird", "mantine", "skarmory", "houndour", "houndoom",
    "kingdra", "phanpy", "donphan", "porygon2", "stantler", "smeargle",
    "tyrogue", "hitmontop", "smoochum", "elekid", "magby", "miltank",
    "blissey", "raikou", "entei", "suicune", "larvitar", "pupitar",
    "tyranitar", "lugia", "ho-oh", "celebi", "treecko", "grovyle",
    "sceptile", "torchic", "combusken", "blaziken", "mudkip", "marshtomp",
    "swampert", "poochyena", "mightyena", "zigzagoon", "linoone", "wurmple",
    "silcoon", "beautifly", "cascoon", "dustox", "lotad", "lombre",
    "ludicolo", "seedot", "nuzleaf", "shiftry", "taillow", "swellow",
    "wingull", "pelipper", "ralts", "kirlia", "gardevoir", "surskit",
    "masquerain", "shroomish", "breloom", "slakoth", "vigoroth", "slaking",
    "nincada", "ninjask", "shedinja", "whismur", "loudred", "exploud",
    "makuhita", "hariyama", "azurill", "nosepass", "skitty", "delcatty",
    "sableye", "mawile", "aron", "lairon", "aggron", "meditite",
    "medicham", "electrike", "manectric", "plusle", "minun", "volbeat",
    "illumise", "roselia", "gulpin", "swalot", "carvanha", "sharpedo",
    "wailmer", "wailord", "numel", "camerupt", "torkoal", "spoink",
    "grumpig", "spinda", "trapinch", "vibrava", "flygon", "cacnea",
    "cacturne", "swablu", "altaria", "zangoose", "seviper", "lunatone",
    "solrock", "barboach", "whiscash", "corphish", "crawdaunt", "baltoy",
    "claydol", "lileep", "cradily", "anorith", "armaldo", "feebas",
    "milotic", "castform", "kecleon", "shuppet", "banette", "duskull",
    "dusclops", "tropius", "chimecho", "absol", "wynaut", "snorunt",
    "glalie", "spheal", "sealeo", "walrein", "clamperl", "huntail",
    "gorebyss", "relicanth", "luvdisc", "bagon", "shelgon", "salamence",
    "beldum", "metang", "metagross", "regirock", "regice", "registeel",
    "latias", "latios", "kyogre", "groudon", "rayquaza", "jirachi",
    "deoxys", "turtwig", "grotle", "torterra", "chimchar", "monferno",
    "infernape", "piplup", "prinplup", "empoleon", "starly", "staravia",
    "staraptor", "bidoof", "bibarel", "kricketot", "kricketune", "shinx",
    "luxio", "luxray", "budew", "roserade", "cranidos", "rampardos",
    "shieldon", "bastiodon", "burmy", "wormadam", "mothim", "combee",
    "vespiquen", "pachirisu", "buizel", "floatzel", "cherubi", "cherrim",
    "shellos", "gastrodon", "ambipom", "drifloon", "drifblim", "buneary",
    "lopunny", "mismagius", "honchkrow", "glameow", "purugly", "chingling",
    "stunky", "skuntank", "bronzor", "bronzong", "bonsly", "mime jr.",
    "happiny", "chatot", "spiritomb", "gible", "gabite", "garchomp",
    "munchlax", "riolu", "lucario", "hippopotas", "hippowdon", "skorupi",
    "drapion", "croagunk", "toxicroak", "carnivine", "finneon", "lumineon",
    "mantyke", "snover", "abomasnow", "weavile", "magnezone", "lickilicky",
    "rhyperior", "tangrowth", "electivire", "magmortar", "togekiss", "yanmega",
    "leafeon", "glaceon", "gliscor", "mamoswine", "porygon-z", "gallade",
    "probopass", "dusknoir", "froslass", "rotom", "uxie", "mesprit",
	"azelf", "dialga", "palkia", "heatran", "regigigas", "giratina",
    "cresselia", "phione", "manaphy", "darkrai", "shaymin", "arceus",
    "victini", "snivy", "servine", "serperior", "tepig", "pignite",
    "emboar", "oshawott", "dewott", "samurott", "patrat", "watchog",
    "lillipup", "herdier", "stoutland", "purrloin", "liepard", "pansage",
    "simisage", "pansear", "simisear", "panpour", "simipour", "munna",
    "musharna", "pidove", "tranquill", "unfezant", "blitzle", "zebstrika",
    "roggenrola", "boldore", "gigalith", "woobat", "swoobat", "drilbur",
    "excadrill", "audino", "timburr", "gurdurr", "conkeldurr", "tympole",
    "palpitoad", "seismitoad", "throh", "sawk", "sewaddle", "swadloon",
    "leavanny", "venipede", "whirlipede", "scolipede", "cottonee", "whimsicott",
    "petilil", "lilligant", "basculin", "sandile", "krokorok", "krookodile",
    "darumaka", "darmanitan", "maractus", "dwebble", "crustle", "scraggy",
    "scrafty", "sigilyph", "yamask", "cofagrigus", "tirtouga", "carracosta",
    "archen", "archeops", "trubbish", "garbodor", "zorua", "zoroark",
    "minccino", "cinccino", "gothita", "gothorita", "gothitelle", "solosis",
    "duosion", "reuniclus", "ducklett", "swanna", "vanillite", "vanillish",
    "vanilluxe", "deerling", "sawsbuck", "emolga", "karrablast", "escavalier",
    "foongus", "amoonguss", "frillish", "jellicent", "alomomola", "joltik",
    "galvantula", "ferroseed", "ferrothorn", "klink", "klang", "klinklang",
    "tynamo", "eelektrik", "eelektross", "elgyem", "beheeyem", "litwick",
    "lampent", "chandelure", "axew", "fraxure", "haxorus", "cubchoo",
    "beartic", "cryogonal", "shelmet", "accelgor", "stunfisk", "mienfoo",
    "mienshao", "druddigon", "golett", "golurk", "pawniard", "bisharp",
    "bouffalant", "rufflet", "braviary", "vullaby", "mandibuzz", "heatmor",
    "durant", "deino", "zweilous", "hydreigon", "larvesta", "volcarona",
    "cobalion", "terrakion", "virizion", "tornadus", "thundurus", "reshiram",
    "zekrom", "landorus", "kyurem", "keldeo", "meloetta", "genesect",
    "chespin", "quilladin", "chesnaught", "fennekin", "braixen", "delphox",
    "froakie", "frogadier", "greninja", "bunnelby", "diggersby", "fletchling",
    "fletchinder", "talonflame", "scatterbug", "spewpa", "vivillon", "litleo",
    "pyroar", "flabébé", "floette", "florges", "skiddo", "gogoat",
    "pancham", "pangoro", "furfrou", "espurr", "meowstic", "honedge",
    "doublade", "aegislash", "spritzee", "aromatisse", "swirlix", "slurpuff",
    "inkay", "malamar", "binacle", "barbaracle", "skrelp", "dragalge",
    "clauncher", "clawitzer", "helioptile", "heliolisk", "tyrunt", "tyrantrum",
    "amaura", "aurorus", "sylveon", "hawlucha", "dedenne", "carbink",
    "goomy", "sliggoo", "goodra", "klefki", "phantump", "trevenant",
    "pumpkaboo", "gourgeist", "bergmite", "avalugg", "noibat", "noivern",
    "xerneas", "yveltal", "zygarde", "diancie", "hoopa", "volcanion",
    "rowlet", "dartrix", "decidueye", "litten", "torracat", "incineroar",
    "popplio", "brionne", "primarina", "pikipek", "trumbeak", "toucannon",
    "yungoos", "gumshoos", "grubbin", "charjabug", "vikavolt", "crabrawler",
    "crabominable", "oricorio", "cutiefly", "ribombee", "rockruff", "lycanroc",
    "wishiwashi", "mareanie", "toxapex", "mudbray", "mudsdale", "dewpider",
    "araquanid", "fomantis", "lurantis", "morelull", "shiinotic", "salandit",
    "salazzle", "stufful", "bewear", "bounsweet", "steenee", "tsareena",
    "comfey", "oranguru", "passimian", "wimpod", "golisopod", "sandygast",
    "palossand", "pyukumuku", "type: null", "silvally", "minior", "komala",
    "turtonator", "togedemaru", "mimikyu", "bruxish", "drampa", "dhelmise",
    "jangmo-o", "hakamo-o", "kommo-o", "tapu koko", "tapu lele", "tapu bulu",
    "tapu fini", "cosmog", "cosmoem", "solgaleo", "lunala", "nihilego",
    "buzzwole", "pheromosa", "xurkitree", "celesteela", "kartana", "guzzlord",
    "necrozma", "magearna", "marshadow", "poipole", "naganadel", "stakataka",
    "blacephalon", "zeraora", "meltan", "melmetal", "grookey", "thwackey",
    "rillaboom", "scorbunny", "raboot", "cinderace", "sobble", "drizzile",
    "inteleon", "skwovet", "greedent", "rookidee", "corvisquire", "corviknight",
    "blipbug", "dottler", "orbeetle", "nickit", "thievul", "gossifleur",
    "eldegoss", "wooloo", "dubwool", "chewtle", "drednaw", "yamper",
    "boltund", "rolycoly", "carkol", "coalossal", "applin", "flapple",
    "appletun", "silicobra", "sandaconda", "cramorant", "arrokuda", "barraskewda",
    "toxel", "toxtricity", "sizzlipede", "centiskorch", "clobbopus", "grapploct",
    "sinistea", "polteageist", "hatenna", "hattrem", "hatterene", "impidimp",
    "morgrem", "grimmsnarl", "obstagoon", "perrserker", "cursola", "sirfetch’d",
    "mr. rime", "runerigus", "milcery", "alcremie", "falinks", "pincurchin",
    "snom", "frosmoth", "stonjourner", "eiscue", "indeedee", "morpeko",
    "cufant", "copperajah", "dracozolt", "arctozolt", "dracovish", "arctovish",
    "duraludon", "dreepy", "drakloak", "dragapult", "zacian", "zamazenta",
    "eternatus", "kubfu", "urshifu", "zarude", "regieleki", "regidrago",
    "glastrier", "spectrier", "calyrex"
};

void LogView::playCry(const QString& number){
    bool ok;
    int speciesnum = number.toInt(&ok);

    if (ok) {
        // Construct the filename using QString and QDir
        QString filename = QDir::toNativeSeparators("sounds/cries/normal/" + QString::fromUtf8(pokemonNames[speciesnum]) + ".wav");
        // Convert the QString to a const char* if needed
        char const* filenameChar     = filename.toUtf8().constData();
        std::thread audioThread(mSDLPlayAudio, filenameChar);
		audioThread.detach(); // Detach the thread

		/* //Database Stuff
		QString pokemon_name = QDir::toNativeSeparators(QString::fromUtf8(pokemonNames[speciesnum]));
        char const* pokemon_nameChar = pokemon_name.toUtf8().constData();
		LogView::insertString(pokemon_nameChar);
		*/
    }
}

void LogView::playAnimeCry(const QString& number){
    bool ok;
    int speciesnum = number.toInt(&ok);

    if (ok) {
		if(speciesnum < 650){
			// Construct the filename using QString and QDir
			QString filename = QDir::toNativeSeparators("sounds/cries/anime/" + number + ".wav");
			// Convert the QString to a const char* if needed
			char const* filenameChar = filename.toUtf8().constData();
			std::thread audioThread(mSDLPlayAudio, filenameChar);
			audioThread.detach(); // Detach the thread
		}
		else{
			// Construct the filename using QString and QDir
			QString filename = QDir::toNativeSeparators("sounds/cries/normal/" + QString::fromUtf8(pokemonNames[speciesnum]) + ".wav");
			// Convert the QString to a const char* if needed
			char const* filenameChar = filename.toUtf8().constData();
			std::thread audioThread(mSDLPlayAudio, filenameChar);
			audioThread.detach(); // Detach the thread
		}
    }
}

#define EWRAM_START 0x02000000
#define COMPRESSED_POKEMON_BYTE_SIZE (36 / 4)
int LogView::insertCompressedPokemon(char const* species, int level, int shiny, char const* nickname, int value1, int value2, int value3, int value4, int value5, int value6, int value7, int value8, int value9){
	int i;
	uint32_t password[COMPRESSED_POKEMON_BYTE_SIZE];
    sqlite3 *db;
    int rc = sqlite3_open("database/test.db", &db);

    /*for(i = 0; i < COMPRESSED_POKEMON_BYTE_SIZE; i++){
		password[i] = core->busRead32(core, EWRAM_START + 0x3fde0 + (i * 0x4));//Sends the Pokémon data into an array
    }*/

	const char* sql = "INSERT INTO CompressedPokemon (Species, Level, Shiny, Nickname, Value1, Value2, Value3, Value4, Value5, Value6, Value7, Value8, Value9) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
    sqlite3_stmt* stmt;

	if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) {
        // Handle error
        return 1;
    }

	// Bind parameters
    sqlite3_bind_text(stmt, 1,  species, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt,  2,  level);
    sqlite3_bind_int(stmt,  3,  shiny);
    sqlite3_bind_text(stmt, 4,  nickname, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt,  5,  value1);
    sqlite3_bind_int(stmt,  6,  value2);
    sqlite3_bind_int(stmt,  7,  value3);
    sqlite3_bind_int(stmt,  8,  value4);
    sqlite3_bind_int(stmt,  9,  value5);
    sqlite3_bind_int(stmt,  10, value6);
    sqlite3_bind_int(stmt,  11, value7);
    sqlite3_bind_int(stmt,  12, value8);
    sqlite3_bind_int(stmt,  13, value9);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        // Handle error
        return 1;
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return 0;
}

int LogView::insertString(char const* filename){
    sqlite3 *db;
    int rc = sqlite3_open("database/test.db", &db);
	//core->busWrite8(core, address, value)

	const char *sql = "INSERT INTO test_table (Pokemon) VALUES (?)";
    sqlite3_stmt *stmt;

	if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) {
        // Handle error
        return 1;
    }

    sqlite3_bind_text(stmt, 1, filename, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        // Handle error
        return 1;
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return 0;
}

void LogView::postLog(int level, int category, const QString& log) {
	QString line = QString("[%1] %2:\t%3").arg(LogController::toString(level)).arg(mLogCategoryName(category)).arg(log);

	//Check if any special command is sent
	if(level == mLOG_WARN){
		//Play Cry
		QRegularExpression re1("^PlayCry:(?<crynum>\\d{1,3})$");
		QRegularExpressionMatch match1 = re1.match(log);
		if (match1.hasMatch()) {
			//Plays the Cry
			const QString& temp = match1.captured("crynum");
			LogView::playCry(temp);
			return;
		}

		//Play Anime Cry
		QRegularExpression re2("^PlayAnimeCry:(?<crynum>\\d{1,3})$");
		QRegularExpressionMatch match2 = re2.match(log);
		if (match2.hasMatch()) {
			//Plays the Cry
			const QString& temp = match2.captured("crynum");
			LogView::playAnimeCry(temp);
			return;
		}

		//Insert Pokemon
		QRegularExpression re3("^InsertMon:(?<species>[^:]+):(?<level>\\d+):(?<shiny>\\d+):(?<nickname>[^:]+):(?<value1>\\d+):(?<value2>\\d+):(?<value3>\\d+):(?<value4>\\d+):(?<value5>\\d+):(?<value6>\\d+):(?<value7>\\d+):(?<value8>\\d+):(?<value9>\\d+)$");
		QRegularExpressionMatch match3 = re3.match(log);

		if (match3.hasMatch()) {
			QString species  = match3.captured("species");
			int level        = match3.captured("level").toInt();
			int shiny        = match3.captured("shiny").toInt();
			QString nickname = match3.captured("nickname");
			int value1       = match3.captured("value1").toInt();
			int value2       = match3.captured("value2").toInt();
			int value3       = match3.captured("value3").toInt();
			int value4       = match3.captured("value4").toInt();
			int value5       = match3.captured("value5").toInt();
			int value6       = match3.captured("value6").toInt();
			int value7       = match3.captured("value7").toInt();
			int value8       = match3.captured("value8").toInt();
			int value9       = match3.captured("value9").toInt();

			bool ok;
    		int speciesnum = species.toInt(&ok);
			if (ok) {
				char const* SpeciesName = pokemonNames[speciesnum];
				char const* Nickname    = pokemonNames[speciesnum];
				insertCompressedPokemon(SpeciesName, level, shiny,
									Nickname, value1, value2, value3,
									value4, value5, value6, value7, value8, value9);
			}
			return;

			/*insertCompressedPokemon(species.toUtf8().constData(), level, shiny,
									nickname.toUtf8().constData(), value1, value2, value3,
									value4, value5, value6, value7, value8, value9);*/
		
		}
	}

	// TODO: Log to file
	m_pendingLines.enqueue(line);
	++m_lines;
	if (m_lines > m_lineLimit) {
		clearLine();
	}
	update();
}

void LogView::clear() {
	m_ui.view->clear();
	m_lines = 0;
}

void LogView::setLevels(int levels) {
	m_ui.levelDebug->setCheckState(levels & mLOG_DEBUG ? Qt::Checked : Qt::Unchecked);
	m_ui.levelStub->setCheckState(levels & mLOG_STUB ? Qt::Checked : Qt::Unchecked);
	m_ui.levelInfo->setCheckState(levels & mLOG_INFO ? Qt::Checked : Qt::Unchecked);
	m_ui.levelWarn->setCheckState(levels & mLOG_WARN ? Qt::Checked : Qt::Unchecked);
	m_ui.levelError->setCheckState(levels & mLOG_ERROR ? Qt::Checked : Qt::Unchecked);
	m_ui.levelFatal->setCheckState(levels & mLOG_FATAL ? Qt::Checked : Qt::Unchecked);
	m_ui.levelGameError->setCheckState(levels & mLOG_GAME_ERROR ? Qt::Checked : Qt::Unchecked);
}

void LogView::setLevel(int level, bool set) {
	if (level & mLOG_DEBUG) {
		m_ui.levelDebug->setCheckState(set ? Qt::Checked : Qt::Unchecked);
	}
	if (level & mLOG_STUB) {
		m_ui.levelStub->setCheckState(set ? Qt::Checked : Qt::Unchecked);
	}
	if (level & mLOG_INFO) {
		m_ui.levelInfo->setCheckState(set ? Qt::Checked : Qt::Unchecked);
	}
	if (level & mLOG_WARN) {
		m_ui.levelWarn->setCheckState(set ? Qt::Checked : Qt::Unchecked);
	}
	if (level & mLOG_ERROR) {
		m_ui.levelError->setCheckState(set ? Qt::Checked : Qt::Unchecked);
	}
	if (level & mLOG_FATAL) {
		m_ui.levelFatal->setCheckState(set ? Qt::Checked : Qt::Unchecked);
	}
	if (level & mLOG_GAME_ERROR) {
		m_ui.levelGameError->setCheckState(set ? Qt::Checked : Qt::Unchecked);
	}

	if (set) {
		emit levelsEnabled(level);
	} else {
		emit levelsDisabled(level);
	}
}

void LogView::setMaxLines(int limit) {
	m_lineLimit = limit;
	while (m_lines > m_lineLimit) {
		clearLine();
	}
}

void LogView::paintEvent(QPaintEvent* event) {
	while (!m_pendingLines.isEmpty()) {
		m_ui.view->appendPlainText(m_pendingLines.dequeue());
	}
	QWidget::paintEvent(event);
}

void LogView::clearLine() {
	if (m_ui.view->document()->isEmpty()) {
		m_pendingLines.dequeue();
	} else {
		QTextCursor cursor(m_ui.view->document());
		cursor.setPosition(0);
		cursor.select(QTextCursor::BlockUnderCursor);
		cursor.removeSelectedText();
		cursor.deleteChar();
	}
	--m_lines;
}