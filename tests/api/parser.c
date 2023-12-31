/*
 * ProFTPD - FTP server testsuite
 * Copyright (c) 2014-2022 The ProFTPD Project team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA 02110-1335, USA.
 *
 * As a special exemption, The ProFTPD Project team and other respective
 * copyright holders give permission to link this program with OpenSSL, and
 * distribute the resulting executable, without including the source code for
 * OpenSSL in the source distribution.
 */

/* Parser API tests. */

#include "tests.h"

static pool *p = NULL;

static const char *config_path = "/tmp/prt-parser.conf";
static const char *config_path2 = "/tmp/prt-parser2.conf";
static const char *config_path3 = "/tmp/prt-parser3.d";
static const char *config_tmp_path = "/tmp/prt-parser.conf~";

static void set_up(void) {
  (void) unlink(config_path);
  (void) unlink(config_path2);
  (void) rmdir(config_path3);
  (void) unlink(config_tmp_path);

  if (p == NULL) {
    p = permanent_pool = make_sub_pool(NULL);
  }

  init_fs();
  pr_fs_statcache_set_policy(PR_TUNABLE_FS_STATCACHE_SIZE,
    PR_TUNABLE_FS_STATCACHE_MAX_AGE, 0);
  init_stash();
  modules_init();

  if (getenv("TEST_VERBOSE") != NULL) {
    pr_trace_set_levels("config", 1, 20);
  }
}

static void tear_down(void) {
  (void) pr_parser_cleanup();

  (void) unlink(config_path);
  (void) unlink(config_path2);
  (void) rmdir(config_path3);
  (void) unlink(config_tmp_path);

  if (getenv("TEST_VERBOSE") != NULL) {
    pr_trace_set_levels("config", 0, 0);
  }

  if (p) {
    destroy_pool(p);
    p = permanent_pool = NULL;
  } 
}

/* Tests */

START_TEST (parser_prepare_test) {
  int res;
  xaset_t *parsed_servers = NULL;

  res = pr_parser_prepare(NULL, NULL);
  ck_assert_msg(res == 0, "Failed to handle null arguments: %s", strerror(errno));

  res = pr_parser_prepare(p, NULL);
  ck_assert_msg(res == 0, "Failed to handle null parsed_servers: %s",
    strerror(errno));

  res = pr_parser_prepare(NULL, &parsed_servers);
  ck_assert_msg(res == 0, "Failed to handle null pool: %s", strerror(errno));

  (void) pr_parser_cleanup();
}
END_TEST

START_TEST (parser_cleanup_test) {
  int res;
  server_rec *ctx;

  mark_point();
  res = pr_parser_cleanup();
  ck_assert_msg(res == 0, "Failed to handle unprepared parser: %s",
    strerror(errno));

  pr_parser_prepare(NULL, NULL);

  mark_point();
  ctx = pr_parser_server_ctxt_open("127.0.0.1");
  ck_assert_msg(ctx != NULL, "Failed to open server context: %s",
    strerror(errno));

  mark_point();
  res = pr_parser_cleanup();
  ck_assert_msg(res < 0, "Failed to handle existing contexts");
  ck_assert_msg(errno == EPERM, "Expected EPERM (%d), got %s (%d)", EPERM,
    strerror(errno), errno);

  mark_point();
  (void) pr_parser_server_ctxt_close();
  res = pr_parser_cleanup();
  ck_assert_msg(res == 0, "Failed to cleanup parser: %s", strerror(errno));
}
END_TEST

START_TEST (parser_server_ctxt_test) {
  server_rec *ctx, *res;

  ctx = pr_parser_server_ctxt_get();
  ck_assert_msg(ctx == NULL, "Found server context unexpectedly");
  ck_assert_msg(errno == ENOENT, "Expected ENOENT (%d), got %s (%d)", ENOENT,
    strerror(errno), errno);

  pr_parser_prepare(p, NULL);

  mark_point();
  res = pr_parser_server_ctxt_close();
  ck_assert_msg(res == NULL, "Closed server context unexpectedly");
  ck_assert_msg(errno == EPERM, "Expected EPERM (%d), got %s (%d)", EPERM,
    strerror(errno), errno);

  mark_point();
  res = pr_parser_server_ctxt_open("127.0.0.1");
  ck_assert_msg(res != NULL, "Failed to open server context: %s",
    strerror(errno));

  mark_point();
  ctx = pr_parser_server_ctxt_get();
  ck_assert_msg(ctx != NULL, "Failed to get current server context: %s",
    strerror(errno));
  ck_assert_msg(ctx == res, "Expected server context %p, got %p", res, ctx);

  mark_point();
  (void) pr_parser_server_ctxt_close();
  (void) pr_parser_cleanup();
}
END_TEST

START_TEST (parser_server_ctxt_push_test) {
  int res;
  server_rec *ctx, *ctx2;

  res = pr_parser_server_ctxt_push(NULL);
  ck_assert_msg(res < 0, "Failed to handle null argument");
  ck_assert_msg(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  ctx = pcalloc(p, sizeof(server_rec));

  mark_point();
  res = pr_parser_server_ctxt_push(ctx);
  ck_assert_msg(res < 0, "Failed to handle unprepared parser");
  ck_assert_msg(errno == EPERM, "Expected EPERM (%d), got %s (%d)", EPERM,
    strerror(errno), errno);

  pr_parser_prepare(p, NULL);

  mark_point();
  res = pr_parser_server_ctxt_push(ctx);
  ck_assert_msg(res == 0, "Failed to push server rec: %s", strerror(errno));

  mark_point();
  ctx2 = pr_parser_server_ctxt_get();
  ck_assert_msg(ctx2 != NULL, "Failed to get current server context: %s",
    strerror(errno));
  ck_assert_msg(ctx2 == ctx, "Expected server context %p, got %p", ctx, ctx2);

  (void) pr_parser_server_ctxt_close();
  (void) pr_parser_cleanup();
}
END_TEST

START_TEST (parser_config_ctxt_test) {
  int is_empty = FALSE;
  config_rec *ctx, *res;

  ctx = pr_parser_config_ctxt_get();
  ck_assert_msg(ctx == NULL, "Found config context unexpectedly");
  ck_assert_msg(errno == ENOENT, "Expected ENOENT (%d), got %s (%d)", ENOENT,
    strerror(errno), errno);

  pr_parser_prepare(p, NULL);
  pr_parser_server_ctxt_open("127.0.0.1");

  res = pr_parser_config_ctxt_open(NULL);
  ck_assert_msg(res == NULL, "Failed to handle null arguments");
  ck_assert_msg(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  mark_point();
  res = pr_parser_config_ctxt_open("<TestSuite>");
  ck_assert_msg(res != NULL, "Failed to open config context: %s",
    strerror(errno));

  mark_point();
  ctx = pr_parser_config_ctxt_get();
  ck_assert_msg(ctx != NULL, "Failed to get current config context: %s",
    strerror(errno));
  ck_assert_msg(ctx == res, "Expected config context %p, got %p", res, ctx);

  mark_point();
  (void) pr_parser_config_ctxt_close(&is_empty);
  ck_assert_msg(is_empty == TRUE, "Expected config context to be empty");

  mark_point();
  res = pr_parser_config_ctxt_open("<Global>");
  ck_assert_msg(res != NULL, "Failed to open config context: %s",
    strerror(errno));
  (void) pr_parser_config_ctxt_close(&is_empty);
  ck_assert_msg(is_empty == TRUE, "Expected config context to be empty");

  (void) pr_parser_server_ctxt_close();
  (void) pr_parser_cleanup();
}
END_TEST

START_TEST (parser_config_ctxt_push_test) {
  int res;
  config_rec *ctx, *ctx2;

  res = pr_parser_config_ctxt_push(NULL);
  ck_assert_msg(res < 0, "Failed to handle null argument");
  ck_assert_msg(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  ctx = pcalloc(p, sizeof(config_rec));

  mark_point();
  res = pr_parser_config_ctxt_push(ctx);
  ck_assert_msg(res < 0, "Failed to handle unprepared parser");
  ck_assert_msg(errno == EPERM, "Expected EPERM (%d), got %s (%d)", EPERM,
    strerror(errno), errno);

  pr_parser_prepare(p, NULL);

  mark_point();
  res = pr_parser_config_ctxt_push(ctx);
  ck_assert_msg(res == 0, "Failed to push config rec: %s", strerror(errno));

  mark_point();
  ctx2 = pr_parser_config_ctxt_get();
  ck_assert_msg(ctx2 != NULL, "Failed to get current config context: %s",
    strerror(errno));
  ck_assert_msg(ctx2 == ctx, "Expected config context %p, got %p", ctx, ctx2);

  (void) pr_parser_config_ctxt_close(NULL);
  (void) pr_parser_cleanup();
}
END_TEST

START_TEST (parser_get_lineno_test) {
  unsigned int res;

  res = pr_parser_get_lineno();
  ck_assert_msg(res == 0, "Expected 0, got %u", res);

  res = pr_parser_get_lineno();
  ck_assert_msg(res == 0, "Expected 0, got %u", res);
}
END_TEST

START_TEST (parser_read_line_test) {
  char *res;

  res = pr_parser_read_line(NULL, 0);
  ck_assert_msg(res == NULL, "Failed to handle null arguments");
  ck_assert_msg(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);
}
END_TEST

START_TEST (parser_parse_line_test) {
  cmd_rec *cmd;
  char *text;
  unsigned int lineno;

  mark_point();
  cmd = pr_parser_parse_line(NULL, NULL, 0);
  ck_assert_msg(cmd == NULL, "Failed to handle null arguments");
  ck_assert_msg(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  mark_point();
  cmd = pr_parser_parse_line(p, NULL, 0);
  ck_assert_msg(cmd == NULL, "Failed to handle null input");
  ck_assert_msg(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  cmd = pr_parser_parse_line(p, "", 0);
  ck_assert_msg(cmd == NULL, "Failed to handle empty input");
  ck_assert_msg(errno == ENOENT, "Expected ENOENT (%d), got %s (%d)", ENOENT,
    strerror(errno), errno);

  pr_parser_prepare(p, NULL);
  pr_parser_server_ctxt_open("127.0.0.1");

  text = pstrdup(p, "FooBar");
  cmd = pr_parser_parse_line(p, text, 0);
  ck_assert_msg(cmd != NULL, "Failed to parse text '%s': %s", text,
    strerror(errno));
  ck_assert_msg(cmd->argc == 1, "Expected 1, got %d", cmd->argc);
  ck_assert_msg(strcmp(cmd->argv[0], text) == 0,
    "Expected '%s', got '%s'", text, (char *) cmd->argv[0]);
  lineno = pr_parser_get_lineno();
  ck_assert_msg(lineno != 1, "Expected lineno 1, got %u", lineno);

  text = pstrdup(p, "FooBar baz quxx");
  cmd = pr_parser_parse_line(p, text, 0);
  ck_assert_msg(cmd != NULL, "Failed to parse text '%s': %s", text,
    strerror(errno));
  ck_assert_msg(cmd->argc == 3, "Expected 3, got %d", cmd->argc);
  ck_assert_msg(strcmp(cmd->argv[0], "FooBar") == 0,
    "Expected 'FooBar', got '%s'", (char *) cmd->argv[0]);
  ck_assert_msg(strcmp(cmd->arg, "baz quxx") == 0,
    "Expected 'baz quxx', got '%s'", cmd->arg);
  lineno = pr_parser_get_lineno();
  ck_assert_msg(lineno != 2, "Expected lineno 2, got %u", lineno);

  /* Deliberately omit the trailing '}', to test our handling of malformed
   * inputs.
   */
  mark_point();
  text = pstrdup(p, "BarBaz %{env:FOO_TEST");
  cmd = pr_parser_parse_line(p, text, 0);
  ck_assert_msg(cmd != NULL, "Failed to parse text '%s': %s", text,
    strerror(errno));
  ck_assert_msg(cmd->argc == 2, "Expected 2, got %d", cmd->argc);
  ck_assert_msg(strcmp(cmd->argv[0], "BarBaz") == 0,
    "Expected 'BarBaz', got '%s'", (char *) cmd->argv[0]);
  ck_assert_msg(strcmp(cmd->arg, "%{env:FOO_TEST") == 0,
    "Expected '%s', got '%s'", "%{env:FOO_TEST", cmd->arg);
  lineno = pr_parser_get_lineno();
  ck_assert_msg(lineno != 3, "Expected lineno 3, got %u", lineno);

  mark_point();
  pr_env_set(p, "FOO_TEST", "BAR");
  text = pstrdup(p, "BarBaz %{env:FOO_TEST}");
  cmd = pr_parser_parse_line(p, text, 0);
  ck_assert_msg(cmd != NULL, "Failed to parse text '%s': %s", text,
    strerror(errno));
  ck_assert_msg(cmd->argc == 2, "Expected 2, got %d", cmd->argc);
  ck_assert_msg(strcmp(cmd->argv[0], "BarBaz") == 0,
    "Expected 'BarBaz', got '%s'", (char *) cmd->argv[0]);
  ck_assert_msg(strcmp(cmd->arg, "BAR") == 0,
    "Expected 'BAR', got '%s'", cmd->arg);
  lineno = pr_parser_get_lineno();
  ck_assert_msg(lineno != 3, "Expected lineno 3, got %u", lineno);

  /* This time, without the requested environment variable present. */
  pr_env_unset(p, "FOO_TEST");
  cmd = pr_parser_parse_line(p, text, 0);
  ck_assert_msg(cmd != NULL, "Failed to parse text '%s': %s", text,
    strerror(errno));
  ck_assert_msg(cmd->argc == 1, "Expected 1, got %d", cmd->argc);
  ck_assert_msg(strcmp(cmd->argv[0], "BarBaz") == 0,
    "Expected 'BarBaz', got '%s'", (char *) cmd->argv[0]);
  ck_assert_msg(strcmp(cmd->arg, "") == 0, "Expected '', got '%s'", cmd->arg);
  lineno = pr_parser_get_lineno();
  ck_assert_msg(lineno != 3, "Expected lineno 3, got %u", lineno);

  /* This time, with a single word containing multiple environment variables
   * (Issue #507).
   */
  pr_env_set(p, "FOO_TEST", "Foo");
  pr_env_set(p, "BAR_TEST", "baR");
  text = pstrdup(p, "BarBaz %{env:FOO_TEST}@%{env:BAR_TEST}");
  cmd = pr_parser_parse_line(p, text, 0);
  ck_assert_msg(cmd != NULL, "Failed to parse text '%s': %s", text,
    strerror(errno));
  ck_assert_msg(cmd->argc == 2, "Expected 2, got %d", cmd->argc);
  ck_assert_msg(strcmp(cmd->argv[0], "BarBaz") == 0,
    "Expected 'BarBaz', got '%s'", (char *) cmd->argv[0]);
  ck_assert_msg(strcmp(cmd->arg, "Foo@baR") == 0,
    "Expected 'Foo@baR', got '%s'", cmd->arg);
  lineno = pr_parser_get_lineno();
  ck_assert_msg(lineno != 3, "Expected lineno 3, got %u", lineno);

  /* This time, with a single word containing multiple environment variables
   * where one of the variables is NOT present (Issue #857).
   */
  pr_env_set(p, "FOO_TEST", "Foo");
  text = pstrdup(p, "BarBaz %{env:FOO_TEST}@%{env:BAZ_TEST}");
  cmd = pr_parser_parse_line(p, text, 0);
  ck_assert_msg(cmd != NULL, "Failed to parse text '%s': %s", text,
    strerror(errno));
  ck_assert_msg(cmd->argc == 2, "Expected 2, got %d", cmd->argc);
  ck_assert_msg(strcmp(cmd->argv[0], "BarBaz") == 0,
    "Expected 'BarBaz', got '%s'", (char *) cmd->argv[0]);
  ck_assert_msg(strcmp(cmd->arg, "Foo@") == 0,
    "Expected 'Foo@', got '%s'", cmd->arg);
  lineno = pr_parser_get_lineno();
  ck_assert_msg(lineno != 3, "Expected lineno 3, got %u", lineno);

  text = pstrdup(p, "<FooBar baz>");
  cmd = pr_parser_parse_line(p, text, 0);
  ck_assert_msg(cmd != NULL, "Failed to parse text '%s': %s", text,
    strerror(errno));
  ck_assert_msg(cmd->argc == 2, "Expected 2, got %d", cmd->argc);
  ck_assert_msg(strcmp(cmd->argv[0], "<FooBar>") == 0,
    "Expected '<FooBar>', got '%s'", (char *) cmd->argv[0]);
  lineno = pr_parser_get_lineno();
  ck_assert_msg(lineno != 5, "Expected lineno 5, got %u", lineno);

  mark_point();
  (void) pr_parser_server_ctxt_close();
  (void) pr_parser_cleanup();
}
END_TEST

MODRET parser_set_testsuite_enabled(cmd_rec *cmd) {
  return PR_HANDLED(cmd);
}

MODRET parser_set_testsuite_engine(cmd_rec *cmd) {
  return PR_HANDLED(cmd);
}

static module parser_module;

static conftable parser_conftab[] = {
  { "TestSuiteEnabled",	parser_set_testsuite_enabled, NULL },
  { "TestSuiteEngine",	parser_set_testsuite_engine, NULL },
  { NULL },
};

static int load_parser_module(void) {
  /* Load the module's config handlers. */
  memset(&parser_module, 0, sizeof(parser_module));
  parser_module.name = "parser";
  parser_module.conftable = parser_conftab;

  return pr_module_load_conftab(&parser_module);
}

START_TEST (parse_config_path_test) {
  int res;
  char *text;
  const char *path;
  struct stat st;
  unsigned long include_opts;
  pr_fh_t *fh;

  (void) pr_parser_cleanup();

  mark_point();
  res = parse_config_path(NULL, NULL);
  ck_assert_msg(res < 0, "Failed to handle null pool");
  ck_assert_msg(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  mark_point();
  res = parse_config_path2(p, NULL, 0);
  ck_assert_msg(res < 0, "Failed to handle null path");
  ck_assert_msg(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  mark_point();
  path = "foo";
  ck_assert_msg(res < 0, "Failed to handle relative path");
  ck_assert_msg(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  mark_point();
  res = parse_config_path2(p, path, 1024);
  ck_assert_msg(res < 0, "Failed to handle excessive depth");
  ck_assert_msg(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  mark_point();
  res = parse_config_path2(p, path, 0);
  ck_assert_msg(res < 0, "Failed to handle invalid path");
  ck_assert_msg(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  mark_point();

  /* Note that `/tmp/` may be a large/wide directory on some systems; we
   * thus make a more predictable directory for our testing.
   */
  res = mkdir(config_path3, 0775);
  ck_assert_msg(res == 0, "Failed to mkdir '%s': %s", config_path3,
    strerror(errno));

  path = config_path3;
  res = lstat(path, &st);
  ck_assert_msg(res == 0, "Failed lstat(2) on '%s': %s", path, strerror(errno));

  mark_point();
  res = parse_config_path2(p, path, 0);
  if (S_ISLNK(st.st_mode)) {
    ck_assert_msg(res < 0, "Failed to handle uninitialized parser");
    ck_assert_msg(errno == EPERM, "Expected EPERM (%d), got %s (%d)", EPERM,
      strerror(errno), errno);

  } else if (S_ISDIR(st.st_mode)) {
    ck_assert_msg(res == 0, "Failed to handle empty directory");
  }

  mark_point();
  pr_parser_prepare(p, NULL);
  pr_parser_server_ctxt_open("127.0.0.1");

  res = parse_config_path2(p, path, 0);
  if (S_ISLNK(st.st_mode)) {
    ck_assert_msg(res < 0, "Failed to handle directory-only path");
    ck_assert_msg(errno == EISDIR, "Expected EISDIR (%d), got %s (%d)", EISDIR,
      strerror(errno), errno);

  } else if (S_ISDIR(st.st_mode)) {
    ck_assert_msg(res == 0, "Failed to handle empty directory");
  }

  res = rmdir(config_path3);
  ck_assert_msg(res == 0, "Failed to rmdir '%s': %s", config_path3,
    strerror(errno));

  mark_point();
  path = config_path;
  res = parse_config_path2(p, path, 0);
  ck_assert_msg(res < 0, "Failed to handle nonexistent file");
  ck_assert_msg(errno == ENOENT, "Expected ENOENT (%d), got %s (%d)", ENOENT,
    strerror(errno), errno);

  include_opts = pr_parser_set_include_opts(PR_PARSER_INCLUDE_OPT_IGNORE_WILDCARDS);
  mark_point();
  path = "/tmp*/foo.conf";
  res = parse_config_path2(p, path, 0);
  ck_assert_msg(res < 0, "Failed to handle directory-only path");
  ck_assert_msg(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);
  (void) pr_parser_set_include_opts(include_opts);

  /* On Mac, `/tmp` is a symlink.  And currently, parse_config_path() does
   * not allow following of symlinked directories.  So this MIGHT fail, if
   * we're on a Mac.
   */
  res = lstat("/tmp", &st);
  ck_assert_msg(res == 0, "Failed lstat(2) on '/tmp': %s", strerror(errno));

  mark_point();
  path = "/tmp/prt*foo*bar*.conf";
  res = parse_config_path2(p, path, 0);

  if (S_ISLNK(st.st_mode)) {
    ck_assert_msg(res < 0, "Failed to handle nonexistent file");
    ck_assert_msg(errno == ENOTDIR, "Expected ENOTDIR (%d), got %s (%d)", ENOTDIR,
      strerror(errno), errno);

    include_opts = pr_parser_set_include_opts(PR_PARSER_INCLUDE_OPT_ALLOW_SYMLINKS);

    /* By default, we ignore the case where there are no matching files. */
    res = parse_config_path2(p, path, 0);
    ck_assert_msg(res == 0, "Failed to handle nonexistent file: %s",
      strerror(errno));

    (void) pr_parser_set_include_opts(include_opts);

  } else {
    /* By default, we ignore the case where there are no matching files. */
    ck_assert_msg(res == 0, "Failed to handle nonexistent file: %s",
      strerror(errno));
  }

  /* Load the module's config handlers. */
  res = load_parser_module();
  ck_assert_msg(res == 0, "Failed to load module conftab: %s", strerror(errno));

  include_opts = pr_parser_set_include_opts(PR_PARSER_INCLUDE_OPT_ALLOW_SYMLINKS);

  /* Parse single file. */
  path = config_path;
  fh = pr_fsio_open(path, O_CREAT|O_EXCL|O_WRONLY);
  ck_assert_msg(fh != NULL, "Failed to open '%s': %s", path, strerror(errno));

  text = "TestSuiteEngine on\r\n";
  res = pr_fsio_write(fh, text, strlen(text));
  ck_assert_msg(res >= 0, "Failed to write '%s': %s", text, strerror(errno));

  res = pr_fsio_close(fh);
  ck_assert_msg(res == 0, "Failed to write '%s': %s", path, strerror(errno));

  mark_point();
  res = parse_config_path2(p, path, 0);
  ck_assert_msg(res >= 0, "Failed to parse '%s': %s", path, strerror(errno));

  path = "/tmp/prt*.conf";
  res = parse_config_path2(p, path, 0);
  ck_assert_msg(res >= 0, "Failed to parse '%s': %s", path, strerror(errno));

  (void) pr_parser_set_include_opts(PR_PARSER_INCLUDE_OPT_ALLOW_SYMLINKS|PR_PARSER_INCLUDE_OPT_IGNORE_TMP_FILES|PR_PARSER_INCLUDE_OPT_IGNORE_WILDCARDS);

  path = config_tmp_path;
  fh = pr_fsio_open(path, O_CREAT|O_EXCL|O_WRONLY);
  ck_assert_msg(fh != NULL, "Failed to open '%s': %s", path, strerror(errno));

  text = "TestSuiteEnabled off\r\n";
  res = pr_fsio_write(fh, text, strlen(text));
  ck_assert_msg(res >= 0, "Failed to write '%s': %s", text, strerror(errno));

  res = pr_fsio_close(fh);
  ck_assert_msg(res == 0, "Failed to write '%s': %s", path, strerror(errno));

  mark_point();
  path = "/tmp/prt*.conf*";
  res = parse_config_path2(p, path, 0);
  ck_assert_msg(res >= 0, "Failed to parse '%s': %s", path, strerror(errno));

  mark_point();
  path = "/t*p/prt*.conf*";
  res = parse_config_path2(p, path, 0);
  ck_assert_msg(res < 0, "Failed to handle wildcard path '%s'", path);
  ck_assert_msg(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  (void) pr_parser_set_include_opts(PR_PARSER_INCLUDE_OPT_ALLOW_SYMLINKS|PR_PARSER_INCLUDE_OPT_IGNORE_TMP_FILES);

  mark_point();
  path = "/t*p/prt*.conf*";
  res = parse_config_path2(p, path, 0);
  ck_assert_msg(res >= 0, "Failed to parse '%s': %s", path, strerror(errno));

  (void) pr_parser_server_ctxt_close();
  (void) pr_parser_cleanup();
  (void) pr_module_unload(&parser_module);
  (void) pr_parser_set_include_opts(include_opts);
}
END_TEST

START_TEST (parser_parse_file_test) {
  int res;
  pr_fh_t *fh;
  char *text;

  (void) unlink(config_path);

  mark_point();
  res = pr_parser_parse_file(NULL, NULL, NULL, 0);
  ck_assert_msg(res < 0, "Failed to handle null arguments");
  ck_assert_msg(errno == EINVAL, "Expected EINVAL (%d), got %s (%d)", EINVAL,
    strerror(errno), errno);

  mark_point();
  res = pr_parser_parse_file(p, config_path, NULL, 0);
  ck_assert_msg(res < 0, "Failed to handle invalid file");
  ck_assert_msg(errno == EPERM, "Expected EPERM (%d), got %s (%d)", EPERM,
    strerror(errno), errno);

  pr_parser_prepare(p, NULL);
  pr_parser_server_ctxt_open("127.0.0.1");

  mark_point();
  res = pr_parser_parse_file(p, config_path, NULL, 0);
  ck_assert_msg(res < 0, "Failed to handle invalid file");
  ck_assert_msg(errno == ENOENT, "Expected ENOENT (%d), got %s (%d)", ENOENT,
    strerror(errno), errno);

  mark_point();
  res = pr_parser_parse_file(p, "/tmp", NULL, 0);
  ck_assert_msg(res < 0, "Failed to handle directory");
  ck_assert_msg(errno == EISDIR, "Expected EISDIR (%d), got %s (%d)", EISDIR,
    strerror(errno), errno);

  fh = pr_fsio_open(config_path, O_CREAT|O_EXCL|O_WRONLY);
  ck_assert_msg(fh != NULL, "Failed to open '%s': %s", config_path,
    strerror(errno));

  text = "TestSuiteEngine on\r\n";
  res = pr_fsio_write(fh, text, strlen(text));
  ck_assert_msg(res >= 0, "Failed to write '%s': %s", text, strerror(errno));

  text = "TestSuiteEnabled on\n";
  res = pr_fsio_write(fh, text, strlen(text));
  ck_assert_msg(res >= 0, "Failed to write '%s': %s", text, strerror(errno));

  text = "Include ";
  res = pr_fsio_write(fh, text, strlen(text));
  ck_assert_msg(res >= 0, "Failed to write '%s': %s", text, strerror(errno));

  text = (char *) config_path2;
  res = pr_fsio_write(fh, text, strlen(text));
  ck_assert_msg(res >= 0, "Failed to write '%s': %s", text, strerror(errno));

  text = "\n";
  res = pr_fsio_write(fh, text, strlen(text));
  ck_assert_msg(res >= 0, "Failed to write '%s': %s", text, strerror(errno));

  res = pr_fsio_close(fh);
  ck_assert_msg(res == 0, "Failed to write '%s': %s", config_path,
    strerror(errno));

  fh = pr_fsio_open(config_path2, O_CREAT|O_EXCL|O_WRONLY);
  ck_assert_msg(fh != NULL, "Failed to open '%s': %s", config_path2,
    strerror(errno));

  text = "TestSuiteOptions Bebugging\n";
  res = pr_fsio_write(fh, text, strlen(text));
  ck_assert_msg(res >= 0, "Failed to write '%s': %s", text, strerror(errno));

  res = pr_fsio_close(fh);
  ck_assert_msg(res == 0, "Failed to write '%s': %s", config_path2,
    strerror(errno));

  mark_point();

  /* Load the module's config handlers. */
  res = load_parser_module();
  ck_assert_msg(res == 0, "Failed to load module conftab: %s", strerror(errno));

  res = pr_parser_parse_file(p, config_path, NULL, PR_PARSER_FL_DYNAMIC_CONFIG);
  ck_assert_msg(res == 0, "Failed to parse '%s': %s", config_path,
    strerror(errno));

  res = pr_parser_parse_file(p, config_path, NULL, 0);
  ck_assert_msg(res < 0, "Parsed '%s' unexpectedly", config_path);
  ck_assert_msg(errno == EPERM, "Expected EPERM (%d), got %s (%d)", EPERM,
    strerror(errno), errno);

  (void) pr_parser_server_ctxt_close();
  (void) pr_parser_cleanup();
  (void) pr_module_unload(&parser_module);
  (void) pr_fsio_unlink(config_path);
  (void) pr_fsio_unlink(config_path2);
}
END_TEST

Suite *tests_get_parser_suite(void) {
  Suite *suite;
  TCase *testcase;

  suite = suite_create("parser");

  testcase = tcase_create("base");
  tcase_add_checked_fixture(testcase, set_up, tear_down);

  tcase_add_test(testcase, parser_prepare_test);
  tcase_add_test(testcase, parser_cleanup_test);
  tcase_add_test(testcase, parser_server_ctxt_test);
  tcase_add_test(testcase, parser_server_ctxt_push_test);
  tcase_add_test(testcase, parser_config_ctxt_test);
  tcase_add_test(testcase, parser_config_ctxt_push_test);
  tcase_add_test(testcase, parser_get_lineno_test);
  tcase_add_test(testcase, parser_read_line_test);
  tcase_add_test(testcase, parser_parse_line_test);
  tcase_add_test(testcase, parse_config_path_test);
  tcase_add_test(testcase, parser_parse_file_test);

  /* Some of these tests may take a little longer. */
  tcase_set_timeout(testcase, 30);

  suite_add_tcase(suite, testcase);
  return suite;
}
