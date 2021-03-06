/**
 * Copyright (c) 2007-2012, Timothy Stack
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * * Neither the name of Timothy Stack nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ''AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @file lnav.hh
 */

#ifndef __lnav_hh
#define __lnav_hh

#include "config.h"

#include <signal.h>

#include <map>
#include <set>
#include <list>
#include <stack>
#include <memory>

#include "byte_array.hh"
#include "grapher.hh"
#include "logfile.hh"
#include "hist_source.hh"
#include "statusview_curses.hh"
#include "listview_curses.hh"
#include "top_status_source.hh"
#include "bottom_status_source.hh"
#include "grep_highlighter.hh"
#include "db_sub_source.hh"
#include "textfile_sub_source.hh"
#include "log_vtab_impl.hh"
#include "readline_curses.hh"
#include "xterm_mouse.hh"
#include "piper_proc.hh"
#include "term_extra.hh"
#include "ansi_scrubber.hh"

/** The command modes that are available while viewing a file. */
typedef enum {
    LNM_PAGING,
    LNM_COMMAND,
    LNM_SEARCH,
    LNM_CAPTURE,
    LNM_SQL,
} ln_mode_t;

enum {
    LNB_SYSLOG,
    LNB__MAX,

    LNB_TIMESTAMP,
    LNB_HELP,
    LNB_HEADLESS,
    LNB_QUIET,
    LNB_ROTATED,
    LNB_CHECK_CONFIG,
};

/** Flags set on the lnav command-line. */
typedef enum {
    LNF_SYSLOG    = (1L << LNB_SYSLOG),

    LNF_ROTATED   = (1L << LNB_ROTATED),

    LNF_TIMESTAMP = (1L << LNB_TIMESTAMP),
    LNF_HELP      = (1L << LNB_HELP),
    LNF_HEADLESS  = (1L << LNB_HEADLESS),
    LNF_QUIET     = (1L << LNB_QUIET),
    LNF_CHECK_CONFIG = (1L << LNB_CHECK_CONFIG),

    LNF__ALL      = (LNF_SYSLOG|LNF_HELP)
} lnav_flags_t;

/** The different views available. */
typedef enum {
    LNV_LOG,
    LNV_TEXT,
    LNV_HELP,
    LNV_HISTOGRAM,
    LNV_GRAPH,
    LNV_DB,
    LNV_EXAMPLE,
    LNV_SCHEMA,

    LNV__MAX
} lnav_view_t;

extern const char *lnav_view_strings[LNV__MAX + 1];

/** The status bars. */
typedef enum {
    LNS_TOP,
    LNS_BOTTOM,

    LNS__MAX
} lnav_status_t;

typedef enum {
    LG_GRAPH,
    LG_CAPTURE,

    LG__MAX
} lnav_grep_t;

void sqlite_close_wrapper(void *mem);

typedef std::pair<int, int>                      ppid_time_pair_t;
typedef std::pair<ppid_time_pair_t, std::string> session_pair_t;

class level_filter : public logfile_filter {
public:
    level_filter()
        : logfile_filter(EXCLUDE, ""),
          lf_min_level(logline::LEVEL_UNKNOWN) {
    };

    bool matches(const logline &ll, const std::string &line)
    {
        return (ll.get_level() & ~logline::LEVEL__FLAGS) < this->lf_min_level;
    };

    std::string to_command(void)
    {
        return ("set-min-log-level " +
            std::string(logline::level_names[this->lf_min_level]));
    };

    logline::level_t lf_min_level;
};

struct _lnav_data {
    std::string                             ld_session_id;
    time_t                                  ld_session_time;
    time_t                                  ld_session_load_time;
    time_t                                  ld_session_save_time;
    std::list<session_pair_t>               ld_session_file_names;
    int                                     ld_session_file_index;
    const char *                            ld_program_name;
    const char *                            ld_debug_log_name;

    std::list<std::string>                  ld_commands;
    std::vector<std::string>                ld_config_paths;
    std::set<std::pair<std::string, int> >  ld_file_names;
    std::list<logfile *>                    ld_files;
    std::list<std::string>                  ld_other_files;
    std::list<std::pair<std::string, int> > ld_files_to_front;
    bool                                    ld_stdout_used;
    sig_atomic_t                            ld_looping;
    sig_atomic_t                            ld_winched;
    sig_atomic_t                            ld_child_terminated;
    unsigned long                           ld_flags;
    WINDOW *                                ld_window;
    ln_mode_t                               ld_mode;

    statusview_curses                       ld_status[LNS__MAX];
    top_status_source                       ld_top_source;
    bottom_status_source                    ld_bottom_source;
    listview_curses::action::broadcaster    ld_scroll_broadcaster;

    time_t                                  ld_top_time;
    int                                     ld_top_time_millis;
    time_t                                  ld_bottom_time;
    int                                     ld_bottom_time_millis;

    textview_curses                         ld_match_view;

    std::stack<textview_curses *>           ld_view_stack;
    textview_curses                         ld_views[LNV__MAX];
    std::auto_ptr<grep_highlighter>         ld_search_child[LNV__MAX];
    vis_line_t                              ld_search_start_line;
    readline_curses *                       ld_rl_view;

    level_filter                            ld_level_filter;
    logfile_sub_source                      ld_log_source;
    hist_source                             ld_hist_source;
    int                                     ld_hist_zoom;

    textfile_sub_source                     ld_text_source;

    std::map<textview_curses *, int>        ld_last_user_mark;

    grapher                                 ld_graph_source;

    hist_source                             ld_db_source;
    db_label_source                         ld_db_rows;
    db_overlay_source                       ld_db_overlay;
    std::vector<std::string>                ld_db_key_names;

    int                                     ld_max_fd;
    fd_set                                  ld_read_fds;

    std::auto_ptr<grep_highlighter>         ld_grep_child[LG__MAX];
    std::string                             ld_previous_search;
    std::string                             ld_last_search[LNV__MAX];

    log_vtab_manager *                      ld_vtab_manager;
    auto_mem<sqlite3, sqlite_close_wrapper> ld_db;

    std::list<pid_t>                        ld_children;
    std::list<piper_proc *>                 ld_pipers;
    xterm_mouse ld_mouse;
    term_extra ld_term_extra;
};

extern struct _lnav_data lnav_data;

#define HELP_MSG_1(x, msg) \
    "Press '" ANSI_BOLD(#x) "' " msg

#define HELP_MSG_2(x, y, msg) \
    "Press " ANSI_BOLD(#x) "/" ANSI_BOLD(#y) " " msg

void rebuild_indexes(bool force);

void ensure_view(textview_curses *expected_tc);
bool toggle_view(textview_curses *toggle_tc);

std::string execute_command(std::string cmdline);

bool setup_logline_table();
int sql_callback(sqlite3_stmt *stmt);

void execute_search(lnav_view_t view, const std::string &regex);

void redo_search(lnav_view_t view_index);

#endif
