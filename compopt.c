// Copyright (C) 2010-2016 Joel Rosdahl
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 3 of the License, or (at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
// more details.
//
// You should have received a copy of the GNU General Public License along with
// this program; if not, write to the Free Software Foundation, Inc., 51
// Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

#include "ccache.h"
#include "compopt.h"

#define TOO_HARD         (1 << 0)
#define TOO_HARD_DIRECT  (1 << 1)
#define TAKES_ARG        (1 << 2)
#define TAKES_CONCAT_ARG (1 << 3)
#define TAKES_PATH       (1 << 4)
#define AFFECTS_CPP      (1 << 5)

struct compopt {
	const char *name;
	int type;
};

static const struct compopt compopts[] = {
	{"--param",         TAKES_ARG},
	{"--save-temps",    TOO_HARD},
	{"--serialize-diagnostics", TAKES_ARG | TAKES_PATH},
	{"-A",              TAKES_ARG},
	{"-B",              TAKES_ARG | TAKES_CONCAT_ARG | TAKES_PATH},
	{"-D",              AFFECTS_CPP | TAKES_ARG | TAKES_CONCAT_ARG},
	{"-E",              TOO_HARD},
	{"-F",              AFFECTS_CPP | TAKES_ARG | TAKES_CONCAT_ARG | TAKES_PATH},
	{"-G",              TAKES_ARG},
	{"-I",              AFFECTS_CPP | TAKES_ARG | TAKES_CONCAT_ARG | TAKES_PATH},
	{"-L",              TAKES_ARG},
	{"-M",              TOO_HARD},
	{"-MF",             TAKES_ARG},
	{"-MM",             TOO_HARD},
	{"-MQ",             TAKES_ARG},
	{"-MT",             TAKES_ARG},
	{"-P",              TOO_HARD},
	{"-U",              AFFECTS_CPP | TAKES_ARG | TAKES_CONCAT_ARG},
	{"-V",              TAKES_ARG},
	{"-Xassembler",     TAKES_ARG},
	{"-Xclang",         TAKES_ARG},
	{"-Xlinker",        TAKES_ARG},
	{"-Xpreprocessor",  AFFECTS_CPP | TOO_HARD_DIRECT | TAKES_ARG},
	{"-arch",           TAKES_ARG},
	{"-aux-info",       TAKES_ARG},
	{"-b",              TAKES_ARG},
	{"-fmodules",       TOO_HARD},
	{"-fno-working-directory", AFFECTS_CPP},
	{"-fplugin=libcc1plugin", TOO_HARD}, // interaction with GDB
	{"-frepo",          TOO_HARD},
	{"-fworking-directory", AFFECTS_CPP},
	{"-idirafter",      AFFECTS_CPP | TAKES_ARG | TAKES_CONCAT_ARG | TAKES_PATH},
	{"-iframework",     AFFECTS_CPP | TAKES_ARG | TAKES_CONCAT_ARG | TAKES_PATH},
	{"-imacros",        AFFECTS_CPP | TAKES_ARG | TAKES_CONCAT_ARG | TAKES_PATH},
	{"-imultilib",      AFFECTS_CPP | TAKES_ARG | TAKES_CONCAT_ARG | TAKES_PATH},
	{"-include",        AFFECTS_CPP | TAKES_ARG | TAKES_CONCAT_ARG | TAKES_PATH},
	{"-include-pch",    AFFECTS_CPP | TAKES_ARG | TAKES_CONCAT_ARG | TAKES_PATH},
	{"-install_name",   TAKES_ARG}, // Darwin linker option
	{"-iprefix",        AFFECTS_CPP | TAKES_ARG | TAKES_CONCAT_ARG | TAKES_PATH},
	{"-iquote",         AFFECTS_CPP | TAKES_ARG | TAKES_CONCAT_ARG | TAKES_PATH},
	{"-isysroot",       AFFECTS_CPP | TAKES_ARG | TAKES_CONCAT_ARG | TAKES_PATH},
	{"-isystem",        AFFECTS_CPP | TAKES_ARG | TAKES_CONCAT_ARG | TAKES_PATH},
	{"-iwithprefix",    AFFECTS_CPP | TAKES_ARG | TAKES_CONCAT_ARG | TAKES_PATH},
	{"-iwithprefixbefore",
	 AFFECTS_CPP | TAKES_ARG | TAKES_CONCAT_ARG | TAKES_PATH},
	{"-nostdinc",       AFFECTS_CPP},
	{"-nostdinc++",     AFFECTS_CPP},
	{"-remap",          AFFECTS_CPP},
	{"-save-temps",     TOO_HARD},
	{"-stdlib=",        AFFECTS_CPP | TAKES_CONCAT_ARG},
	{"-trigraphs",      AFFECTS_CPP},
	{"-u",              TAKES_ARG | TAKES_CONCAT_ARG},
};

// MSVC Specific options
static const struct compopt compopts_msvc[] = {
	{"/AI",   AFFECTS_CPP | TAKES_ARG| TAKES_CONCAT_ARG | TAKES_PATH},
	{"/C",    AFFECTS_CPP},
	{"/D",    AFFECTS_CPP | TAKES_ARG | TAKES_CONCAT_ARG},
	{"/E",    TOO_HARD},
	{"/EH",   TAKES_CONCAT_ARG},
	{"/EP",   TOO_HARD},
	{"/FA",   TAKES_CONCAT_ARG},
	{"/FC",   0},
	{"/FI",   AFFECTS_CPP | TAKES_ARG | TAKES_CONCAT_ARG | TAKES_PATH},
	{"/FR",   TAKES_ARG | TAKES_CONCAT_ARG | TAKES_PATH}, // extended.sbr
	{"/FR:",  TAKES_ARG | TAKES_PATH},                    // extended.sbr
	{"/FS",   0},
	{"/FU",   AFFECTS_CPP | TAKES_ARG | TAKES_CONCAT_ARG | TAKES_PATH}, // force assembly
	{"/Fa",   TAKES_ARG | TAKES_CONCAT_ARG | TAKES_PATH}, // assembly_listing.txt
	{"/Fa:",  TAKES_ARG | TAKES_PATH},                    // assembly_listing.txt
	{"/Fd",   TAKES_ARG | TAKES_CONCAT_ARG | TAKES_PATH}, // debug.pdb
	{"/Fd:",  TAKES_ARG | TAKES_PATH},                    // debug.pdb
	{"/Fe",   TAKES_ARG | TAKES_CONCAT_ARG | TAKES_PATH}, // foo.exe
	{"/Fe:",  TAKES_ARG | TAKES_PATH},                    // foo.exe
	{"/Fi",   TAKES_ARG | TAKES_CONCAT_ARG | TAKES_PATH}, // foo.i
	{"/Fi:",  TAKES_ARG | TAKES_PATH},                    // foo.i
	{"/Fm",   TAKES_ARG | TAKES_CONCAT_ARG | TAKES_PATH}, // map.txt
	{"/Fm:",  TAKES_ARG | TAKES_PATH},                    // map.txt
	{"/Fo",   TAKES_ARG | TAKES_CONCAT_ARG | TAKES_PATH}, // foo.obj
	{"/Fo:",  TAKES_ARG | TAKES_PATH},                    // foo.obj
	{"/Fp",   TAKES_ARG | TAKES_CONCAT_ARG | TAKES_PATH}, // headers.pch
	{"/Fp:",  TAKES_ARG | TAKES_PATH},                    // headers.pch
	{"/Fr",   TAKES_ARG | TAKES_CONCAT_ARG | TAKES_PATH}, // source_browser.sbr
	{"/Fr:",  TAKES_ARG | TAKES_PATH},                    // source_browser.sbr
	{"/Fx",   AFFECTS_CPP},
	{"/GA",   0},
	{"/GF",   0},
	{"/GH",   0},
	{"/GL",   0},
	{"/GL-",  0},
	{"/GR",   0},
	{"/GR-",  0},
	{"/GS",   0},
	{"/GS-",  0},
	{"/GT",   0},
	{"/GX",   0},
	{"/GX-",  0},
	{"/GZ",   0},
	{"/Ge",   0},
	{"/Gh",   0},
	{"/Gm",   0},
	{"/Gm-",  0},
	{"/Gs",   TAKES_CONCAT_ARG},
	{"/Gv",   0},
	{"/Gw",   0},
	{"/Gw-",  0},
	{"/Gy",   0},
	{"/Gy-",  0},
	{"/H",    TAKES_CONCAT_ARG},
	{"/I",    AFFECTS_CPP | TAKES_ARG | TAKES_CONCAT_ARG | TAKES_PATH},
	{"/J",    0},
	{"/L",    TAKES_ARG},
	{"/MD",   0},
	{"/MDd",  0},
	{"/MP",   TAKES_CONCAT_ARG},
	{"/MT",   0},
	{"/MTd",  0},
	{"/O",    TAKES_CONCAT_ARG},
	{"/P",    TOO_HARD},
	{"/Qfast_transcendentals", 0},
	{"/Qpar", 0},
	{"/Qpar-", TAKES_CONCAT_ARG},
	{"/RTC",  TAKES_CONCAT_ARG},
	{"/TC",   0},
	{"/TP",   0},
	{"/Tc",   TAKES_CONCAT_ARG},
	{"/Tp",   TAKES_CONCAT_ARG},
	{"/U",    AFFECTS_CPP | TAKES_ARG | TAKES_CONCAT_ARG},
	{"/V",    TAKES_CONCAT_ARG},
	{"/W",    TAKES_CONCAT_ARG},
	{"/WL",   0},
	{"/WX",   0},
	{"/Wall", 0},
	{"/Wv:",  TAKES_CONCAT_ARG},
	{"/X",    AFFECTS_CPP},
	{"/Y",   0},
	{"/Yc",   TAKES_CONCAT_ARG | TAKES_PATH},
	{"/Yd",   0},
	{"/Yl",   TAKES_CONCAT_ARG},
	{"/Yu",   TAKES_CONCAT_ARG | TAKES_PATH},
	{"/Z7",   0},
	{"/ZH:",  TAKES_CONCAT_ARG},
	{"/ZI",   0},
	{"/ZW",   0},
	{"/Za",   0},
	{"/Zc:",  TAKES_CONCAT_ARG},
	{"/Zi",   0},
	{"/Zl",   0},
	{"/Zm",   TAKES_CONCAT_ARG},
	{"/Zo",   0},
	{"/Zo-",  0},
	{"/Zp",   TAKES_CONCAT_ARG},
	{"/Zs",   0},
	{"/arch:", TAKES_CONCAT_ARG},
	{"/await", 0},
	{"/bigobj", 0},
	{"/clr",  0},
	{"/clr:", TAKES_CONCAT_ARG},
	{"/constexpr:", TAKES_CONCAT_ARG},
	{"/doc",  TAKES_ARG | TAKES_CONCAT_ARG | TAKES_PATH}, // .xdc
	{"/errorReport:", TAKES_CONCAT_ARG},
	{"/execution-charset:", TAKES_CONCAT_ARG},
	{"/favor:", TAKES_CONCAT_ARG},
	{"/fp:",  TAKES_CONCAT_ARG},
	{"/guard:cf", 0},
	{"/guard:cf-", 0},
	{"/homeparams", 0},
	{"/nologo", 0},
	{"/openmp", 0},
	{"/sdl",  0},
	{"/showIncludes", TOO_HARD},
	{"/source-charset:", TAKES_CONCAT_ARG},
	{"/u",    AFFECTS_CPP},
	{"/utf-8", 0},
	{"/validate-charset", 0},
	{"/validate-charset-", 0},
	{"/vd",   TAKES_CONCAT_ARG},
	{"/vm",   TAKES_CONCAT_ARG},
	{"/volatile:", TAKES_CONCAT_ARG},
	{"/w",    TAKES_CONCAT_ARG},
	{"/wd",   TAKES_CONCAT_ARG},
	{"/we",   TAKES_CONCAT_ARG},
	{"/wo",   TAKES_CONCAT_ARG},
};

static int
compare_compopts(const void *key1, const void *key2)
{
	const struct compopt *opt1 = (const struct compopt *)key1;
	const struct compopt *opt2 = (const struct compopt *)key2;
	return strcmp(opt1->name, opt2->name);
}

static int
compare_prefix_compopts(const void *key1, const void *key2)
{
	const struct compopt *opt1 = (const struct compopt *)key1;
	const struct compopt *opt2 = (const struct compopt *)key2;
	return strncmp(opt1->name, opt2->name, strlen(opt2->name));
}

static const struct compopt *
find(const char *option)
{
	struct compopt key;

	if (compiler == COMPILER_MSVC) {
		char* s = x_strdup(option);
		if (s[0] == '-') {
			s[0] = '/';
		}
		key.name = s;
		const struct compopt* co = bsearch(
			&key, compopts_msvc, sizeof(compopts_msvc) /
			sizeof(compopts_msvc[0]),
			sizeof(compopts[0]),
			compare_compopts);
		free(s);
		return co;
	} else {
		key.name = option;
		return bsearch(
			&key, compopts, sizeof(compopts) / sizeof(compopts[0]),
			sizeof(compopts[0]),
			compare_compopts);
	}
}

static const struct compopt *
find_prefix(const char *option)
{
	struct compopt key;

	if (compiler == COMPILER_MSVC) {
		char* s = x_strdup(option);
		if (s[0] == '-') {
			s[0] = '/';
		}
		key.name = s;
		const struct compopt* co = bsearch(
			&key, compopts_msvc, sizeof(compopts_msvc) /
			sizeof(compopts_msvc[0]),
			sizeof(compopts[0]), compare_prefix_compopts);
		free(s);
		return co;
	} else {
		key.name = option;
		return bsearch(
			&key, compopts, sizeof(compopts) / sizeof(compopts[0]),
			sizeof(compopts[0]), compare_prefix_compopts);
	}
}

// Runs fn on the first two characters of option.
bool
compopt_short(bool (*fn)(const char *), const char *option)
{
	char *short_opt = x_strndup(option, 2);
	bool retval = fn(short_opt);
	free(short_opt);
	return retval;
}

// For test purposes.
bool
compopt_verify_sortedness(void)
{
	for (size_t i = 1; i < sizeof(compopts)/sizeof(compopts[0]); i++) {
		if (strcmp(compopts[i-1].name, compopts[i].name) >= 0) {
			fprintf(stderr,
							"compopt_verify_sortedness: %s >= %s\n",
							compopts[i-1].name,
							compopts[i].name);
			return false;
		}
	}
	for (size_t i = 1; i < sizeof(compopts_msvc)/sizeof(compopts_msvc[0]); i++) {
		if (strcmp(compopts_msvc[i-1].name, compopts_msvc[i].name) >= 0) {
			fprintf(stderr,
							"compopt_verify_sortedness: %s >= %s\n",
							compopts_msvc[i-1].name,
							compopts_msvc[i].name);
			return false;
		}
	}
	return true;
}

bool
compopt_known(const char *option)
{
	const struct compopt *co = find(option);
	return co && !(co->type & TOO_HARD);
}

bool
compopt_affects_cpp(const char *option)
{
	const struct compopt *co = find(option);
	return co && (co->type & AFFECTS_CPP);
}

bool
compopt_too_hard(const char *option)
{
	const struct compopt *co = find(option);
	return co && (co->type & TOO_HARD);
}

bool
compopt_too_hard_for_direct_mode(const char *option)
{
	const struct compopt *co = find(option);
	return co && (co->type & TOO_HARD_DIRECT);
}

bool
compopt_takes_path(const char *option)
{
	const struct compopt *co = find(option);
	return co && (co->type & TAKES_PATH);
}

bool
compopt_takes_arg(const char *option)
{
	const struct compopt *co = find(option);
	return co && (co->type & TAKES_ARG);
}

int
compopt_takes_concat_arg(const char *option)
{
	const struct compopt *co = find_prefix(option);
	if (!co || !(co->type & TAKES_CONCAT_ARG)) {
		return 0;
	}
	int len = (int)strlen(co->name);
	return (co->type & TAKES_PATH) && is_full_path(option+len)
				 ? len : 0;
}

// Determines if the prefix of the option matches any option and affects the
// preprocessor.
bool
compopt_prefix_affects_cpp(const char *option)
{
	// Prefix options have to take concatentated args.
	const struct compopt *co = find_prefix(option);
	if (!co || !(co->type & TAKES_CONCAT_ARG) || !(co->type & AFFECTS_CPP)) {
		return false;
	}
	int len = (int)strlen(co->name);
	if (co->type & TAKES_PATH) {
		return strchr(option+len, '.') || is_full_path(option+len);
	} else {
		return !strchr(option+len, '.') && !is_full_path(option+len);
	}
}
