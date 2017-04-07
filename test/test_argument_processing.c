// Copyright (C) 2010-2017 Joel Rosdahl
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

// This file contains tests for the processing of compiler arguments.

#include "../ccache.h"
#include "../conf.h"
#include "framework.h"
#include "util.h"

extern struct conf *conf;

static char *
get_root(void)
{
#ifndef _WIN32
	return x_strdup("/");
#else
	char volume[4];   // "C:\"
	GetVolumePathName(get_cwd(), volume, sizeof(volume));
	return x_strdup(volume);
#endif
}

static char *
get_posix_path(char *path)
{
#ifndef _WIN32
	return x_strdup(path);
#else
	char *posix;
	char *p;

	// /-escape volume.
	if (isalpha(path[0]) && path[1] == ':') {
		posix = format("/%c%s", path[0], &path[2]);
	} else {
		posix = x_strdup(path);
	}
	// Convert slashes.
	for (p = posix; *p; p++) {
		if (*p == '\\') {
			*p = '/';
		}
	}
	return posix;
#endif
}

TEST_SUITE(argument_processing)

TEST(dash_E_should_result_in_called_for_preprocessing)
{
	struct args *orig = args_init_from_string("cc -c foo.c -E");
	struct args *preprocessed, *compiler;

	create_file("foo.c", "");
	CHECK(!cc_process_args(orig, &preprocessed, &compiler));
	CHECK_INT_EQ(1, stats_get_pending(STATS_PREPROCESSING));

	args_free(orig);
}

TEST(dash_M_should_be_unsupported)
{
	struct args *orig = args_init_from_string("cc -c foo.c -M");
	struct args *preprocessed, *compiler;

	create_file("foo.c", "");
	CHECK(!cc_process_args(orig, &preprocessed, &compiler));
	CHECK_INT_EQ(1, stats_get_pending(STATS_UNSUPPORTED_OPTION));

	args_free(orig);
}

TEST(dependency_flags_should_only_be_sent_to_the_preprocessor)
{
#define CMD \
	"cc -MD -MMD -MP -MF foo.d -MT mt1 -MT mt2 -MQ mq1 -MQ mq2" \
	" -Wp,-MD,wpmd -Wp,-MMD,wpmmd -Wp,-MP -Wp,-MT,wpmt -Wp,-MQ,wpmq -Wp,-MF,wpf"
	struct args *orig = args_init_from_string(CMD " -c foo.c -o foo.o");
	struct args *exp_cpp = args_init_from_string(CMD);
#undef CMD
	struct args *exp_cc = args_init_from_string("cc -c");
	struct args *act_cpp = NULL, *act_cc = NULL;
	create_file("foo.c", "");

	CHECK(cc_process_args(orig, &act_cpp, &act_cc));
	CHECK_ARGS_EQ_FREE12(exp_cpp, act_cpp);
	CHECK_ARGS_EQ_FREE12(exp_cc, act_cc);

	args_free(orig);
}

TEST(cpp_only_flags_to_preprocessor_if_run_second_cpp_is_false)
{
#define CMD \
	"cc -I. -idirafter . -iframework. -imacros . -imultilib ." \
	" -include test.h -include-pch test.pch -iprefix . -iquote ." \
	" -isysroot . -isystem . -iwithprefix . -iwithprefixbefore ." \
	" -DTEST_MACRO -DTEST_MACRO2=1 -F. -trigraphs -fworking-directory" \
	" -fno-working-directory -MD -MMD -MP -MF foo.d -MT mt1 -MT mt2" \
	" -MQ mq1 -MQ mq2 -Wp,-MD,wpmd -Wp,-MMD,wpmmd -Wp,-MP -Wp,-MT,wpmt" \
	" -Wp,-MQ,wpmq -Wp,-MF,wpf"
	struct args *orig = args_init_from_string(CMD " -c foo.c -o foo.o");
	struct args *exp_cpp = args_init_from_string(CMD);
#undef CMD
	struct args *exp_cc = args_init_from_string("cc -c");
	struct args *act_cpp = NULL, *act_cc = NULL;
	create_file("foo.c", "");

	conf->run_second_cpp = false;
	CHECK(cc_process_args(orig, &act_cpp, &act_cc));
	CHECK_ARGS_EQ_FREE12(exp_cpp, act_cpp);
	CHECK_ARGS_EQ_FREE12(exp_cc, act_cc);

	args_free(orig);
}

TEST(cpp_only_flags_to_preprocessor_and_compiler_if_run_second_cpp_is_true)
{
#define CMD \
	"cc -I. -idirafter . -iframework. -imacros . -imultilib ." \
	" -include test.h -include-pch test.pch -iprefix . -iquote ." \
	" -isysroot . -isystem . -iwithprefix . -iwithprefixbefore ." \
	" -DTEST_MACRO -DTEST_MACRO2=1 -F. -trigraphs -fworking-directory" \
	" -fno-working-directory"
#define DEP_OPTS \
	" -MD -MMD -MP -MF foo.d -MT mt1 -MT mt2 " \
	" -MQ mq1 -MQ mq2 -Wp,-MD,wpmd -Wp,-MMD,wpmmd"
	struct args *orig = args_init_from_string(CMD DEP_OPTS " -c foo.c -o foo.o");
	struct args *exp_cpp = args_init_from_string(CMD DEP_OPTS);
	struct args *exp_cc = args_init_from_string(CMD " -c");
#undef CMD
	struct args *act_cpp = NULL, *act_cc = NULL;
	create_file("foo.c", "");

	conf->run_second_cpp = true;
	CHECK(cc_process_args(orig, &act_cpp, &act_cc));
	CHECK_ARGS_EQ_FREE12(exp_cpp, act_cpp);
	CHECK_ARGS_EQ_FREE12(exp_cc, act_cc);

	args_free(orig);
}

TEST(dependency_flags_that_take_an_argument_should_not_require_space_delimiter)
{
	struct args *orig = args_init_from_string(
		"cc -c -MMD -MFfoo.d -MT mt -MTmt -MQmq foo.c -o foo.o");
	struct args *exp_cpp = args_init_from_string(
		"cc -MMD -MFfoo.d -MT mt -MTmt -MQmq");
	struct args *exp_cc = args_init_from_string("cc -c");
	struct args *act_cpp = NULL, *act_cc = NULL;
	create_file("foo.c", "");

	CHECK(cc_process_args(orig, &act_cpp, &act_cc));
	CHECK_ARGS_EQ_FREE12(exp_cpp, act_cpp);
	CHECK_ARGS_EQ_FREE12(exp_cc, act_cc);

	args_free(orig);
}

TEST(make_relative_path)
{
	extern char *current_working_dir;

	free(conf->base_dir);
	conf->base_dir = get_root();
	current_working_dir = get_cwd();

	CHECK_STR_EQ_FREE2("./foo.c",
										 make_relative_path(format("%s/%s", current_working_dir,
																							 "foo.c")));

	char *dir = format("%s/a/b/c/foo.c", current_working_dir);
	create_parent_dirs(dir);
	CHECK_STR_EQ_FREE2("a/foo.c",
										 make_relative_path(format("%s/a/foo.c",
																							 current_working_dir)));
	CHECK_STR_EQ_FREE2("a/b/foo.c",
										 make_relative_path(format("%s/a/b/foo.c",
																							 current_working_dir)));
	CHECK_STR_EQ_FREE2("a/b/c/foo.c",
										 make_relative_path(format("%s/a/b/c/foo.c",
																							 current_working_dir)));
	CHECK_STR_EQ_FREE12(format("%s/a/b/c/d/foo.c", current_working_dir),
											make_relative_path(format("%s/a/b/c/d/foo.c",
																								current_working_dir)));

#ifdef _WIN32
	// Mixed / and \\ char
	CHECK_STR_EQ_FREE2("a/foo.c",
										 make_relative_path(format("%s\\a\\foo.c",
																							 current_working_dir)));
	CHECK_STR_EQ_FREE2("a/b/foo.c",
										 make_relative_path(format("%s\\a/b\\foo.c",
																							 current_working_dir)));
	CHECK_STR_EQ_FREE2("a/b/c/foo.c",
										 make_relative_path(format("%s/a\\b\\c/foo.c",
																							 current_working_dir)));

	// All \\ case
	for (char *p = current_working_dir; *p; ++p) {
		if (*p == '/') {
			*p = '\\';
		}
	}
	CHECK_STR_EQ_FREE2("a/foo.c",
										 make_relative_path(format("%s\\a\\foo.c",
																							 current_working_dir)));
	// All / case
	for (char *p = current_working_dir; *p; ++p) {
		if (*p == '\\') {
			*p = '/';
		}
	}
	CHECK_STR_EQ_FREE2("a/b/foo.c",
										 make_relative_path(format("%s/a/b/foo.c",
																							 current_working_dir)));
	// Mixed / and \\ case
	int count = 0;
	for (char *p = current_working_dir; *p; ++p) {
		if (*p == '\\' || *p == '/') {
			*p = "/\\"[count ^= 1];
		}
	}
	CHECK_STR_EQ_FREE2("a/b/c/foo.c",
										 make_relative_path(format("%s/a\\b/c\\foo.c",
																							 current_working_dir)));
#endif
	free(dir);
}

TEST(input_relative_path)
{
	extern char *current_working_dir;
	char *arg_string;
	struct args *orig;
	struct args *act_cpp = NULL, *act_cc = NULL;

	create_file("foo.c", "//Cannot be empty");
	current_working_dir = get_cwd();
	free(conf->base_dir);
	conf->base_dir = x_strdup(current_working_dir);
	arg_string = format("gcc -c %s/foo.c", current_working_dir);
	orig = args_init_from_string(arg_string);
	free(arg_string);

	CHECK(cc_process_args(orig, &act_cpp, &act_cc));
	CHECK_STR_EQ("foo.c", input_file);
	CHECK_STR_EQ("foo.o", output_obj);

	args_free(orig);
	args_free(act_cpp);
	args_free(act_cc);
}

TEST(sysroot_should_be_rewritten_if_basedir_is_used)
{
	extern char *current_working_dir;
	char *arg_string;
	struct args *orig;
	struct args *act_cpp = NULL, *act_cc = NULL;

	create_file("foo.c", "");
	free(conf->base_dir);
	conf->base_dir = get_root();
	current_working_dir = get_cwd();
	arg_string = format("cc --sysroot=%s/foo -c foo.c", current_working_dir);
	orig = args_init_from_string(arg_string);
	free(arg_string);

	CHECK(cc_process_args(orig, &act_cpp, &act_cc));
	CHECK_STR_EQ("--sysroot=./foo", act_cpp->argv[1]);

	args_free(orig);
	args_free(act_cpp);
	args_free(act_cc);
}

TEST(sysroot_with_separate_argument_should_be_rewritten_if_basedir_is_used)
{
	extern char *current_working_dir;
	char *arg_string;
	struct args *orig;
	struct args *act_cpp = NULL, *act_cc = NULL;

	create_file("foo.c", "");
	free(conf->base_dir);
	conf->base_dir = get_root();
	current_working_dir = get_cwd();
	arg_string = format("cc --sysroot %s/foo -c foo.c", current_working_dir);
	orig = args_init_from_string(arg_string);
	free(arg_string);

	CHECK(cc_process_args(orig, &act_cpp, &act_cc));

	CHECK_STR_EQ("--sysroot", act_cpp->argv[1]);
	CHECK_STR_EQ("./foo", act_cpp->argv[2]);

	args_free(orig);
	args_free(act_cpp);
	args_free(act_cc);
}

TEST(MF_flag_with_immediate_argument_should_work_as_last_argument)
{
	struct args *orig = args_init_from_string(
		"cc -c foo.c -o foo.o -MMD -MT bar -MFfoo.d");
	struct args *exp_cpp = args_init_from_string(
		"cc -MMD -MT bar -MFfoo.d");
	struct args *exp_cc = args_init_from_string("cc -c");
	struct args *act_cpp = NULL, *act_cc = NULL;
	create_file("foo.c", "");

	CHECK(cc_process_args(orig, &act_cpp, &act_cc));
	CHECK_ARGS_EQ_FREE12(exp_cpp, act_cpp);
	CHECK_ARGS_EQ_FREE12(exp_cc, act_cc);

	args_free(orig);
}

TEST(MT_flag_with_immediate_argument_should_work_as_last_argument)
{
	struct args *orig = args_init_from_string(
		"cc -c foo.c -o foo.o -MMD -MFfoo.d -MT foo -MTbar");
	struct args *exp_cpp = args_init_from_string(
		"cc -MMD -MFfoo.d -MT foo -MTbar");
	struct args *exp_cc = args_init_from_string("cc -c");
	struct args *act_cpp = NULL, *act_cc = NULL;
	create_file("foo.c", "");

	CHECK(cc_process_args(orig, &act_cpp, &act_cc));
	CHECK_ARGS_EQ_FREE12(exp_cpp, act_cpp);
	CHECK_ARGS_EQ_FREE12(exp_cc, act_cc);

	args_free(orig);
}

TEST(MQ_flag_with_immediate_argument_should_work_as_last_argument)
{
	struct args *orig = args_init_from_string(
		"cc -c foo.c -o foo.o -MMD -MFfoo.d -MQ foo -MQbar");
	struct args *exp_cpp = args_init_from_string(
		"cc -MMD -MFfoo.d -MQ foo -MQbar");
	struct args *exp_cc = args_init_from_string("cc -c");
	struct args *act_cpp = NULL, *act_cc = NULL;
	create_file("foo.c", "");

	CHECK(cc_process_args(orig, &act_cpp, &act_cc));
	CHECK_ARGS_EQ_FREE12(exp_cpp, act_cpp);
	CHECK_ARGS_EQ_FREE12(exp_cc, act_cc);

	args_free(orig);
}

TEST(MQ_flag_without_immediate_argument_should_not_add_MQobj)
{
	struct args *orig = args_init_from_string(
		"gcc -c -MD -MP -MFfoo.d -MQ foo.d foo.c");
	struct args *exp_cpp = args_init_from_string(
		"gcc -MD -MP -MFfoo.d -MQ foo.d");
	struct args *exp_cc = args_init_from_string("gcc -c");
	struct args *act_cpp = NULL, *act_cc = NULL;
	create_file("foo.c", "");

	CHECK(cc_process_args(orig, &act_cpp, &act_cc));
	CHECK_ARGS_EQ_FREE12(exp_cpp, act_cpp);
	CHECK_ARGS_EQ_FREE12(exp_cc, act_cc);

	args_free(orig);
}

TEST(MT_flag_without_immediate_argument_should_not_add_MTobj)
{
	struct args *orig = args_init_from_string(
		"gcc -c -MD -MP -MFfoo.d -MT foo.d foo.c");
	struct args *exp_cpp = args_init_from_string(
		"gcc -MD -MP -MFfoo.d -MT foo.d");
	struct args *exp_cc = args_init_from_string("gcc -c");
	struct args *act_cpp = NULL, *act_cc = NULL;
	create_file("foo.c", "");

	CHECK(cc_process_args(orig, &act_cpp, &act_cc));
	CHECK_ARGS_EQ_FREE12(exp_cpp, act_cpp);
	CHECK_ARGS_EQ_FREE12(exp_cc, act_cc);

	args_free(orig);
}

TEST(MQ_flag_with_immediate_argument_should_not_add_MQobj)
{
	struct args *orig = args_init_from_string(
		"gcc -c -MD -MP -MFfoo.d -MQfoo.d foo.c");
	struct args *exp_cpp = args_init_from_string(
		"gcc -MD -MP -MFfoo.d -MQfoo.d");
	struct args *exp_cc = args_init_from_string("gcc -c");
	struct args *act_cpp = NULL, *act_cc = NULL;
	create_file("foo.c", "");

	CHECK(cc_process_args(orig, &act_cpp, &act_cc));
	CHECK_ARGS_EQ_FREE12(exp_cpp, act_cpp);
	CHECK_ARGS_EQ_FREE12(exp_cc, act_cc);

	args_free(orig);
}

TEST(MT_flag_with_immediate_argument_should_not_add_MQobj)
{
	struct args *orig = args_init_from_string(
		"gcc -c -MD -MP -MFfoo.d -MTfoo.d foo.c");
	struct args *exp_cpp = args_init_from_string(
		"gcc -MD -MP -MFfoo.d -MTfoo.d");
	struct args *exp_cc = args_init_from_string("gcc -c");
	struct args *act_cpp = NULL, *act_cc = NULL;
	create_file("foo.c", "");

	CHECK(cc_process_args(orig, &act_cpp, &act_cc));
	CHECK_ARGS_EQ_FREE12(exp_cpp, act_cpp);
	CHECK_ARGS_EQ_FREE12(exp_cc, act_cc);

	args_free(orig);
}

TEST(fprofile_flag_with_existing_dir_should_be_rewritten_to_real_path)
{
	struct args *orig = args_init_from_string(
		"gcc -c -fprofile-generate=some/dir foo.c");
	struct args *exp_cpp = args_init_from_string("gcc");
	struct args *exp_cc = args_init_from_string("gcc");
	struct args *act_cpp = NULL, *act_cc = NULL;
	char *s, *path;

	create_file("foo.c", "");
	mkdir("some", 0777);
	mkdir("some/dir", 0777);
	path = x_realpath("some/dir");
	s = format("-fprofile-generate=%s", path);
	free(path);
	args_add(exp_cpp, s);
	args_add(exp_cc, s);
	args_add(exp_cc, "-c");
	free(s);

	CHECK(cc_process_args(orig, &act_cpp, &act_cc));
	CHECK_ARGS_EQ_FREE12(exp_cpp, act_cpp);
	CHECK_ARGS_EQ_FREE12(exp_cc, act_cc);

	args_free(orig);
}

TEST(fprofile_flag_with_nonexisting_dir_should_not_be_rewritten)
{
	struct args *orig = args_init_from_string(
		"gcc -c -fprofile-generate=some/dir foo.c");
	struct args *exp_cpp = args_init_from_string(
		"gcc -fprofile-generate=some/dir");
	struct args *exp_cc = args_init_from_string(
		"gcc -fprofile-generate=some/dir -c");
	struct args *act_cpp = NULL, *act_cc = NULL;

	create_file("foo.c", "");

	CHECK(cc_process_args(orig, &act_cpp, &act_cc));
	CHECK_ARGS_EQ_FREE12(exp_cpp, act_cpp);
	CHECK_ARGS_EQ_FREE12(exp_cc, act_cc);

	args_free(orig);
}

TEST(isystem_flag_with_separate_arg_should_be_rewritten_if_basedir_is_used)
{
	extern char *current_working_dir;
	char *arg_string;
	struct args *orig;
	struct args *act_cpp = NULL, *act_cc = NULL;

	create_file("foo.c", "");
	free(conf->base_dir);
	conf->base_dir = get_root();
	current_working_dir = get_cwd();
	arg_string = format("cc -isystem %s/foo -c foo.c", current_working_dir);
	orig = args_init_from_string(arg_string);
	free(arg_string);

	CHECK(cc_process_args(orig, &act_cpp, &act_cc));
	CHECK_STR_EQ("./foo", act_cpp->argv[2]);

	args_free(orig);
	args_free(act_cpp);
	args_free(act_cc);
}

TEST(isystem_flag_with_concat_arg_should_be_rewritten_if_basedir_is_used)
{
	extern char *current_working_dir;
	char *cwd;
	char *arg_string;
	struct args *orig;
	struct args *act_cpp = NULL, *act_cc = NULL;

	create_file("foo.c", "");
	free(conf->base_dir);
	conf->base_dir = get_root();   // posix
	current_working_dir = get_cwd();
	// Windows path doesn't work concatenated.
	cwd = get_posix_path(current_working_dir);
	arg_string = format("cc -isystem%s/foo -c foo.c", cwd);
	orig = args_init_from_string(arg_string);
	free(arg_string);

	CHECK(cc_process_args(orig, &act_cpp, &act_cc));
	CHECK_STR_EQ("-isystem./foo", act_cpp->argv[1]);

	free(cwd);
	args_free(orig);
	args_free(act_cpp);
	args_free(act_cc);
}

TEST(I_flag_with_concat_arg_should_be_rewritten_if_basedir_is_used)
{
	extern char *current_working_dir;
	char *cwd;
	char *arg_string;
	struct args *orig;
	struct args *act_cpp = NULL, *act_cc = NULL;

	create_file("foo.c", "");
	free(conf->base_dir);
	conf->base_dir = get_root();
	current_working_dir = get_cwd();
	// Windows path doesn't work concatenated.
	cwd = get_posix_path(current_working_dir);
	arg_string = format("cc -I%s/foo -c foo.c", cwd);
	orig = args_init_from_string(arg_string);
	free(arg_string);

	CHECK(cc_process_args(orig, &act_cpp, &act_cc));
	CHECK_STR_EQ("-I./foo", act_cpp->argv[1]);

	free(cwd);
	args_free(orig);
	args_free(act_cpp);
	args_free(act_cc);
}

TEST(debug_flag_order_with_known_option_first)
{
	struct args *orig = args_init_from_string("cc -g1 -gsplit-dwarf foo.c -c");
	struct args *exp_cpp = args_init_from_string("cc -g1 -gsplit-dwarf");
	struct args *exp_cc = args_init_from_string("cc -g1 -gsplit-dwarf -c");
	struct args *act_cpp = NULL;
	struct args *act_cc = NULL;

	create_file("foo.c", "");
	CHECK(cc_process_args(orig, &act_cpp, &act_cc));
	CHECK_ARGS_EQ_FREE12(exp_cpp, act_cpp);
	CHECK_ARGS_EQ_FREE12(exp_cc, act_cc);

	args_free(orig);
}

TEST(debug_flag_order_with_known_option_last)
{
	struct args *orig = args_init_from_string("cc -gsplit-dwarf -g1 foo.c -c");
	struct args *exp_cpp = args_init_from_string("cc -gsplit-dwarf -g1");
	struct args *exp_cc = args_init_from_string("cc -gsplit-dwarf -g1 -c");
	struct args *act_cpp = NULL;
	struct args *act_cc = NULL;

	create_file("foo.c", "");
	CHECK(cc_process_args(orig, &act_cpp, &act_cc));
	CHECK_ARGS_EQ_FREE12(exp_cpp, act_cpp);
	CHECK_ARGS_EQ_FREE12(exp_cc, act_cc);

	args_free(orig);
}

TEST(CL_slash_E_should_result_in_called_for_preprocessing)
{
	struct args *orig = args_init_from_string("cl /E");
	struct args *act_cpp = NULL, *act_cc = NULL;

	CHECK(!cc_process_args(orig, &act_cpp, &act_cc));
	CHECK_INT_EQ(2, stats_get_pending(STATS_PREPROCESSING));

	args_free(orig);
	args_free(act_cpp);
	args_free(act_cc);
}

TEST(cl_dash_EP_should_result_in_called_for_preprocessing)
{
	struct args *orig = args_init_from_string("cl -c foo.c -E -P foo.i");
	struct args *preprocessed, *compiler;

	create_file("foo.c", "");
	CHECK(!cc_process_args(orig, &preprocessed, &compiler));
	CHECK_INT_EQ(3, stats_get_pending(STATS_PREPROCESSING));

	args_free(orig);
}

TEST(CL_at_file_expension)
{
	create_file("foo.c", "//Cannot be empty");
	char *current_working_dir = get_cwd();
	char *file_string = format("%s/foo.c", current_working_dir);
	create_file("file.jom", file_string);
	free(conf->base_dir);
	conf->base_dir = x_strdup(current_working_dir);
	struct args *orig = args_init_from_string("cl /c @file.jom");
	struct args *act_cpp = NULL, *act_cc = NULL;

	CHECKM(cc_process_args(orig, &act_cpp, &act_cc),
				 format("cwd=%s\n@file.jom=%s", current_working_dir, file_string));
	CHECK_INT_EQ(2, act_cc->argc);
	CHECK_STR_EQ("cl", act_cc->argv[0]);
	CHECK_STR_EQ("-c", act_cc->argv[1]);
	CHECK_INT_EQ(1, act_cpp->argc);
	CHECK_STR_EQ("cl",  act_cpp->argv[0]);
	CHECK_INT_EQ(0, stats_get_pending(STATS_NOINPUT));
	CHECK_STR_EQ("foo.c", input_file);
	CHECK_STR_EQ("foo.obj", output_obj);

	free(file_string);
	args_free(orig);
	args_free(act_cpp);
	args_free(act_cc);
}

TEST(CL_dash_Fo)
{
	create_file("foo.c", "//Not empty file");
	char *current_working_dir = get_cwd();
	free(conf->base_dir);
	conf->base_dir = get_root();
	char *arg_string = format("cl -Fo%s/bar.obj -c %s/foo.c",
														current_working_dir, current_working_dir);
	struct args *orig = args_init_from_string(arg_string);
	struct args *act_cpp = NULL, *act_cc = NULL;

	CHECKM(cc_process_args(orig, &act_cpp, &act_cc),
				 format("args=%s", arg_string));
	CHECK_INT_EQ(2, act_cc->argc);
	CHECK_STR_EQ("-c",  act_cc->argv[1]);
	CHECK_INT_EQ(1, act_cpp->argc);
	CHECK_STR_EQ("foo.c", input_file);
	CHECK_STR_EQ("./bar.obj", output_obj);

	free(arg_string);
	args_free(orig);
	args_free(act_cpp);
	args_free(act_cc);
}

TEST(CL_slash_Fo)
{
	create_file("foo.c", "//Not empty file");
	char *current_working_dir = get_cwd();
	free(conf->base_dir);
	conf->base_dir = get_root();
	char *arg_string = format("cl /Fo%s/bar.obj /c %s/foo.c",
														current_working_dir, current_working_dir);
	struct args *orig = args_init_from_string(arg_string);
	struct args *act_cpp = NULL, *act_cc = NULL;

	CHECKM(cc_process_args(orig, &act_cpp, &act_cc),
				 format("args=%s", arg_string));
	CHECK_INT_EQ(2, act_cc->argc);
	CHECK_STR_EQ("-c",  act_cc->argv[1]);
	CHECK_INT_EQ(1, act_cpp->argc);
	CHECK_STR_EQ("foo.c", input_file);
	CHECK_STR_EQ("./bar.obj", output_obj);

	free(arg_string);
	args_free(orig);
	args_free(act_cpp);
	args_free(act_cc);
}

TEST(CL_slash_Fo_pwd)
{
	create_file("foo.c", "//Not empty file");
	char *current_working_dir = get_cwd();
	free(conf->base_dir);
	conf->base_dir = get_root();
	char *arg_string = format("cl /Fo%s\\ /c %s\\foo.c",
														current_working_dir, current_working_dir);
	struct args *orig = args_init_from_string(arg_string);
	struct args *act_cpp = NULL, *act_cc = NULL;

	CHECKM(cc_process_args(orig, &act_cpp, &act_cc),
				 format("args=%s", arg_string));
	CHECK_INT_EQ(2, act_cc->argc);
	CHECK_STR_EQ("-c",  act_cc->argv[1]);
	CHECK_INT_EQ(1, act_cpp->argc);
	CHECK_STR_EQ("foo.c", input_file);
	CHECK_STR_EQ("./foo.obj", output_obj);

	free(arg_string);
	args_free(orig);
	args_free(act_cpp);
	args_free(act_cc);
}

TEST(CL_slash_Fo_colon)
{
	create_file("foo.c", "//Not empty file");
	char *current_working_dir = get_cwd();
	free(conf->base_dir);
	conf->base_dir = get_root();
	char *arg_string = format("cl /Fo:%s/bar.obj /c %s/foo.c",
														current_working_dir, current_working_dir);
	struct args *orig = args_init_from_string(arg_string);
	struct args *act_cpp = NULL, *act_cc = NULL;

	CHECKM(cc_process_args(orig, &act_cpp, &act_cc),
				 format("args=%s", arg_string));
	CHECK_INT_EQ(2, act_cc->argc);
	CHECK_STR_EQ("-c",  act_cc->argv[1]);
	CHECK_INT_EQ(1, act_cpp->argc);
	CHECK_STR_EQ("foo.c", input_file);
	CHECK_STR_EQ("./bar.obj", output_obj);

	free(arg_string);
	args_free(orig);
	args_free(act_cpp);
	args_free(act_cc);
}

TEST(CL_slash_Fo_colon_pwd)
{
	create_file("foo.c", "//Not empty file");
	char *current_working_dir = get_cwd();
	free(conf->base_dir);
	conf->base_dir = get_root();
	char *arg_string = format("cl /Fo:%s\\ /c %s\\foo.c",
														current_working_dir, current_working_dir);
	struct args *orig = args_init_from_string(arg_string);
	struct args *act_cpp = NULL, *act_cc = NULL;

	CHECKM(cc_process_args(orig, &act_cpp, &act_cc),
				 format("args=%s", arg_string));
	CHECK_INT_EQ(2, act_cc->argc);
	CHECK_STR_EQ("-c",  act_cc->argv[1]);
	CHECK_INT_EQ(1, act_cpp->argc);
	CHECK_STR_EQ("foo.c", input_file);
	CHECK_STR_EQ("./foo.obj", output_obj);

	free(arg_string);
	args_free(orig);
	args_free(act_cpp);
	args_free(act_cc);
}

TEST(CL_slash_Fo_colon_local_dir)
{
	create_file("foo.c", "//Not empty file");
	char *current_working_dir = get_cwd();
	free(conf->base_dir);
	conf->base_dir = get_root();
	char *arg_string = format("cl /Fo:Objects/ /c %s/foo.c",
														current_working_dir);
	struct args *orig = args_init_from_string(arg_string);
	struct args *act_cpp = NULL, *act_cc = NULL;

	CHECKM(cc_process_args(orig, &act_cpp, &act_cc),
				 format("args=%s", arg_string));
	CHECK_INT_EQ(2, act_cc->argc);
	CHECK_STR_EQ("-c",  act_cc->argv[1]);
	CHECK_INT_EQ(1, act_cpp->argc);
	CHECK_STR_EQ("foo.c", input_file);
	CHECK_STR_EQ("Objects/foo.obj", output_obj);

	free(arg_string);
	args_free(orig);
	args_free(act_cpp);
	args_free(act_cc);
}

TEST(CL_slash_Fo_with_quotes)
{
	create_dir("Src Dir");
	create_dir("Obj Dir");
	create_file("Src Dir/foo.c", "//Not empty file");
	free(conf->base_dir);
	conf->base_dir = get_root();
	char *arg_string = format("cl /Fo\"Obj Dir/\" /c \"Src Dir/foo.c\"");
	struct args *orig = args_init_from_string(arg_string);

	CHECK_INT_EQ(4, orig->argc);
	CHECK_STR_EQ("/Fo\"Obj Dir/\"", orig->argv[1]);
	CHECK_STR_EQ("Src Dir/foo.c", orig->argv[3]);

	struct args *act_cpp = NULL, *act_cc = NULL;

	CHECKM(cc_process_args(orig, &act_cpp, &act_cc),
				 format("args=%s", arg_string));
	CHECK_INT_EQ(2, act_cc->argc);
	CHECK_STR_EQ("-c",  act_cc->argv[1]);
	CHECK_INT_EQ(1, act_cpp->argc);
	CHECK_STR_EQ("Src Dir/foo.c", input_file);
	CHECK_STR_EQ("Obj Dir/foo.obj", output_obj);

	free(arg_string);
	args_free(orig);
	args_free(act_cpp);
	args_free(act_cc);
}

TEST(CL_dash_P_should_be_unsupported)
{
	struct args *orig = args_init_from_string("cl -c foo.c -P");
	struct args *preprocessed, *compiler;

	create_file("foo.c", "");

	CHECK(!cc_process_args(orig, &preprocessed, &compiler));
	CHECK_INT_EQ(2, stats_get_pending(STATS_UNSUPPORTED_OPTION));

	args_free(orig);
}

TEST(CL_dependency_slash_flags_should_only_be_sent_to_the_preprocessor)
{
#define CMD \
	"cl /C /DA=1 /FIbar.h /Iinclude/Dir /UNDEBUG /u /X"
	struct args *orig = args_init_from_string(CMD " -c foo.c -Fofoo.obj");
	struct args *exp_cpp = args_init_from_string(CMD);
#undef CMD
	struct args *exp_cc = args_init_from_string("cl -c");
	struct args *act_cpp = NULL, *act_cc = NULL;
	create_file("foo.c", "");

	conf->run_second_cpp = false;
	CHECK(cc_process_args(orig, &act_cpp, &act_cc));
	CHECK_ARGS_EQ_FREE12(exp_cpp, act_cpp);
	CHECK_ARGS_EQ_FREE12(exp_cc, act_cc);

	args_free(orig);
}

TEST(CL_dependency_dash_flags_should_only_be_sent_to_the_preprocessor)
{
#define CMDDASH \
	"-C -DA=1 -FIbar.h -Iinclude/Dir -UNDEBUG -u -X"
#define CMDSLASH \
	"/C /DA=1 /FIbar.h /Iinclude/Dir /UNDEBUG /u /X"
	struct args *orig =
		args_init_from_string("cl " CMDDASH " -c foo.c -Fofoo.obj");
	struct args *exp_cpp = args_init_from_string("cl " CMDSLASH);
#undef CMD
	struct args *exp_cc = args_init_from_string("cl -c");
	struct args *act_cpp = NULL, *act_cc = NULL;
	create_file("foo.c", "");

	conf->run_second_cpp = false;
	CHECK(cc_process_args(orig, &act_cpp, &act_cc));
	CHECK_ARGS_EQ_FREE12(exp_cpp, act_cpp);
	CHECK_ARGS_EQ_FREE12(exp_cc, act_cc);

	args_free(orig);
}

TEST(CL_full_path_with_slash_U)
{
	struct args *orig = args_init_from_string(
		"cl /c foo.c "
		"/UNDEBUG "
		"/Users/home/my/foo.c "
		);

	struct args *act_cpp = NULL, *act_cc = NULL;
	create_file("foo.c", "");

	conf->run_second_cpp = false;
	CHECK(cc_process_args(orig, &act_cpp, &act_cc));
	CHECK_INT_EQ(3, act_cpp->argc);
	CHECK_STR_EQ("/Users/home/my/foo.c", act_cpp->argv[1]);
	CHECK_STR_EQ("/UNDEBUG", act_cpp->argv[2]);
	CHECK_INT_EQ(3, act_cc->argc);
	CHECK_STR_EQ("/Users/home/my/foo.c", act_cc->argv[1]);
	CHECK_STR_EQ("-c", act_cc->argv[2]);
	args_free(orig);
}

TEST(CL_output_filenames)
{
	struct args *orig =
		args_init_from_string("cl -c foo.c "
													"/Faassembly_listing.txt "
													"/Fddebug.pdb "
													"/Fefoo.exe "
													"/Fifoo.i "
													"/Fmmap.txt "
													"/Fofoo.obj "
													"/Fpheaders.pch "
													"/Frsource_browser.sbr "
													"/FRextended.sbr "
													"/Fa: assembly_listing.txt "
													"/Fd: debug.pdb "
													"/Fe: foo.exe "
													"/Fi: foo.i "
													"/Fm: map.txt "
													"/Fo: foo.obj "
													"/Fp: headers.pch "
													"/Fr: source_browser.sbr "
													"/FR: extended.sbr ");

	struct args *act_cpp = NULL, *act_cc = NULL;
	create_file("foo.c", "");

	conf->run_second_cpp = false;
	CHECK(cc_process_args(orig, &act_cpp, &act_cc));

	CHECK_INT_EQ(1 + 3*8, act_cpp->argc);   // cl ...
	CHECK_STR_EQ("/Faassembly_listing.txt", act_cpp->argv[1]);
	CHECK_STR_EQ("/Fddebug.pdb", act_cpp->argv[2]);
	CHECK_STR_EQ("extended.sbr", act_cpp->argv[24]);

	CHECK_INT_EQ(2 + 3*8, act_cc->argc);    // cl ... -c
	CHECK_STR_EQ("/Faassembly_listing.txt", act_cc->argv[1]);
	CHECK_STR_EQ("extended.sbr", act_cc->argv[24]);
	CHECK_STR_EQ("-c", act_cc->argv[25]);

	CHECK_STR_EQ("foo.obj", output_obj);

	args_free(orig);
	args_free(act_cpp);
	args_free(act_cc);
}

TEST_SUITE_END
