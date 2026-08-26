/*
 * Kenux OS - Firefox-like Browser (Minimal)
 * Implementation
 */

#include "firefox.h"

/* Purge flags for firefox_purge_private_data() */
#define FF_PURGE_COOKIES     (1 << 0)
#define FF_PURGE_CACHE       (1 << 1)
#define FF_PURGE_HISTORY     (1 << 2)
#define FF_PURGE_FORMDATA    (1 << 3)
#define FF_PURGE_DOWNLOADS   (1 << 4)
#define FF_PURGE_SESSIONS    (1 << 5)
#define FF_PURGE_PASSWORDS   (1 << 6)
#define FF_PURGE_ALL         0x7FFFFFFF

/* ---------- helpers ---------- */

static const char *ff_engine_name(FfEngine e) {
    switch (e) {
        case FF_ENGINE_BASIC:       return "basic";
        case FF_ENGINE_GECKO_LITE: return "gecko-lite";
        case FF_ENGINE_SERVO:       return "servo";
        default: return "basic";
    }
}

static const char *ff_security_state_name(FfSecurityState s) {
    switch (s) {
        case FF_SSL_NONE:                return "none";
        case FF_SSL_HTTPS_PADLOCK:       return "https-ev";
        case FF_SSL_HTTPS_NORMAL:        return "https";
        case FF_SSL_HTTPS_WARN_MIXED:   return "https-mixed";
        case FF_SSL_HTTPS_BROKEN:        return "https-broken";
        case FF_SSL_HTTPS_HSTS_ENFORCED: return "https-hsts";
        default: return "unknown";
    }
}

static int ff_str_ends_with(const char *s, const char *suffix) {
    if (!s || !suffix) return 0;
    size_t ls = strlen(s), lf = strlen(suffix);
    if (lf == 0) return 1;
    if (lf > ls) return 0;
    return strcmp(s + ls - lf, suffix) == 0;
}

static int ff_str_starts_with(const char *s, const char *prefix) {
    if (!s || !prefix) return 0;
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

static int ff_str_copy(char *dst, size_t dst_sz, const char *src) {
    if (!dst || dst_sz == 0) return -1;
    if (!src) { dst[0] = 0; return 0; }
    size_t l = strlen(src);
    if (l >= dst_sz) l = dst_sz - 1;
    memcpy(dst, src, l);
    dst[l] = 0;
    return 0;
}

static void ff_generate_guid(char *out, size_t n) {
    static unsigned long counter = 0;
    snprintf(out, n, "{%08lx-%08lx-%04lx}",
             (unsigned long)time(NULL),
             ++counter,
             (unsigned long)(counter & 0xFFFF));
}

/* Replace first occurrence of "{searchTerms}" in tpl with query, write to out. */
static void ff_search_substitute(const char *tpl, const char *query,
                                 char *out, size_t out_sz) {
    const char *marker = "{searchTerms}";
    const char *p = strstr(tpl, marker);
    if (!p) { ff_str_copy(out, out_sz, tpl); return; }
    size_t pre = (size_t)(p - tpl);
    size_t mlen = strlen(marker);
    size_t qlen = strlen(query);
    size_t total = pre + qlen + strlen(p + mlen);
    if (total >= out_sz) total = out_sz - 1;
    size_t i = 0;
    for (size_t k = 0; k < pre && i < out_sz - 1; k++) out[i++] = tpl[k];
    for (size_t k = 0; k < qlen && i < out_sz - 1; k++) out[i++] = query[k];
    for (size_t k = 0; (p + mlen + k)[0] && i < out_sz - 1; k++)
        out[i++] = (p + mlen + k)[0];
    out[i] = 0;
}

/* ---------- init / cleanup ---------- */

void firefox_init(FirefoxState *s) {
    memset(s, 0, sizeof(*s));

    s->engine = FF_ENGINE_BASIC;
    s->active_window = -1;
    s->current_profile = -1;
    s->next_tab_id = 1;

    s->restore_on_start = 2;     /* home */
    ff_str_copy(s->home_url, sizeof(s->home_url), "https://start.kenuxk.local/");
    ff_str_copy(s->newtab_url, sizeof(s->newtab_url), "about:newtab");
    ff_str_copy(s->about_blank, sizeof(s->about_blank), "about:blank");

    /* Default search engines */
    {
        FfSearchEngine *e = &s->search_engines[s->search_engine_count++];
        ff_str_copy(e->name, sizeof(e->name), "Google");
        ff_str_copy(e->alias, sizeof(e->alias), "g");
        ff_str_copy(e->search_url, sizeof(e->search_url),
                    "https://www.google.com/search?q={searchTerms}");
        ff_str_copy(e->suggest_url, sizeof(e->suggest_url),
                    "https://suggestqueries.google.com/complete/search?q={searchTerms}");
        e->is_default = 1; e->order = 0;
    }
    {
        FfSearchEngine *e = &s->search_engines[s->search_engine_count++];
        ff_str_copy(e->name, sizeof(e->name), "DuckDuckGo");
        ff_str_copy(e->alias, sizeof(e->alias), "ddg");
        ff_str_copy(e->search_url, sizeof(e->search_url),
                    "https://duckduckgo.com/?q={searchTerms}");
        e->order = 1;
    }
    {
        FfSearchEngine *e = &s->search_engines[s->search_engine_count++];
        ff_str_copy(e->name, sizeof(e->name), "Bing");
        ff_str_copy(e->alias, sizeof(e->alias), "b");
        ff_str_copy(e->search_url, sizeof(e->search_url),
                    "https://www.bing.com/search?q={searchTerms}");
        e->order = 2;
    }
    s->default_search_engine = 0;
    s->keyword_enabled = 1;
    ff_str_copy(s->search_bar_mode, sizeof(s->search_bar_mode), "unified");

    /* Tabs behaviour */
    s->tabs_load_discarded_on_restore = 1;
    s->browser_tabs_animate = 1;
    s->tab_max_pinned_count = 64;

    /* History */
    s->history_file_purge_old_days = 90;
    s->remember_history = 1;
    s->remember_search_form_history = 1;

    /* Cache */
    s->cache_enabled = 1;
    s->cache_max_bytes = (size_t)350 * 1024 * 1024; /* 350 MiB */

    /* Cookies */
    s->accept_cookies = 1;
    s->thirdparty_cookie_policy = 2;       /* from visited */
    s->lifetime_policy = 0;                /* forever */

    /* Downloads */
    ff_str_copy(s->download_dir, sizeof(s->download_dir), "~/Downloads");

    /* Passwords */
    s->remember_passwords = 1;

    /* Network */
    s->proxy.type = FF_PROXY_SYSTEM;
    ff_str_copy(s->accept_language, sizeof(s->accept_language), "en-US,en;q=0.9");
    ff_str_copy(s->doh_provider, sizeof(s->doh_provider), "cloudflare");
    s->ocsp_stapling_enabled = 1;
    s->quic_enabled = 1;
    s->http3_enabled = 1;
    s->max_http_connections = 256;
    s->max_persistent_connections_per_server = 6;

    /* TLS */
    ff_str_copy(s->tls_min_version, sizeof(s->tls_min_version), "1.2");
    ff_str_copy(s->tls_max_version, sizeof(s->tls_max_version), "1.3");

    /* Content behaviour */
    s->dom_storage_enabled = 1;
    s->indexdb_enabled = 1;
    s->service_workers_enabled = 1;
    s->notifications_enabled = 1;
    s->popup_blocker = 1;
    s->javascript_enabled = 1;
    s->fullscreen_allowed = 1;
    s->pointer_lock_allowed = 1;
    s->webaudio_enabled = 1;
    s->media_hardware_acceleration = 1;

    /* Privacy */
    s->privacy.tracking_protection = 1;
    s->privacy.etp_level = FF_ETP_STANDARD;
    s->privacy.block_trackers = 1;
    s->privacy.block_cookies_third_party = 1;
    s->privacy.block_fingerprinters = 1;
    s->privacy.block_cryptominers = 1;
    s->privacy.block_social_trackers = 1;
    s->privacy.cookie_behaviour = 1;
    s->privacy.dnt = 1;
    s->privacy.global_privacy_control = 1;
    s->privacy.https_only_mode = 1;        /* private-only */
    s->privacy.enable_webrtc = 1;
    s->privacy.privacy_reduce_timer_resolution = 1;
    s->privacy.block_popups = 1;
    s->privacy.block_redirects = 1;

    /* Extensions */
    s->xpinstall_allowed = 1;
    s->extensions_auto_update = 1;

    /* Updates */
    s->app_update_auto = 1;
    s->addons_update_auto = 1;
    s->search_update_auto = 1;
    ff_str_copy(s->update_channel, sizeof(s->update_channel), "release");

    /* Telemetry (off by default) */
    s->crash_reporter_enabled = 1;
    s->new_tab_top_sites = 1;
    s->new_tab_highlights = 1;
    s->activity_stream = 1;

    /* Geo */
    s->geo_enabled = 1;

    /* DRM */
    s->eme_enabled = 1;
    s->widevine_enabled = 1;

    /* GPU */
    s->gpu_process_enabled = 1;
    s->compositor_enabled = 1;
    s->webrender_enabled = 1;
    s->hardware_video_decoding = 1;

    s->is_first_run = 1;
    s->started_at = time(NULL);

    ff_str_copy(s->profile_path, sizeof(s->profile_path), "~/.kenuxk/firefox");
}

static void ff_free_bookmark_tree(FfBookmark *b) {
    if (!b) return;
    for (int i = 0; i < b->child_count; i++) {
        ff_free_bookmark_tree(b->children[i]);
    }
    if (b->children) free(b->children);
    free(b);
}

void firefox_cleanup(FirefoxState *s) {
    if (!s) return;
    ff_free_bookmark_tree(s->root_bookmark);
    s->root_bookmark = NULL;
    s->bookmarks_menu = NULL;
    s->bookmarks_toolbar = NULL;
    s->bookmarks_unfiled = NULL;
    s->bookmarks_mobile = NULL;

    for (int i = 0; i < s->tab_count; i++) {
        if (s->tabs[i].page_bytes) {
            free(s->tabs[i].page_bytes);
            s->tabs[i].page_bytes = NULL;
            s->tabs[i].page_len = 0;
        }
    }
    for (int i = 0; i < s->cache_count; i++) {
        if (s->cache[i].content) {
            free(s->cache[i].content);
            s->cache[i].content = NULL;
            s->cache[i].content_length = 0;
        }
    }
    s->tab_count = 0;
    s->window_count = 0;
    s->cache_count = 0;
    s->cookie_count = 0;
    s->history_count = 0;
    s->download_count = 0;
}

/* ---------- argument parsing ---------- */

int firefox_parse_arguments(FirefoxState *s, int argc, char **argv) {
    s->argc = argc;
    s->argv = argv;
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (!a) continue;

        if (strcmp(a, "-P") == 0 || strcmp(a, "--profile") == 0) {
            const char *v = (++i < argc) ? argv[i] : "";
            /* find or pick profile by name */
            int found = -1;
            for (int p = 0; p < s->profile_count; p++) {
                if (strcmp(s->profiles[p].name, v) == 0) { found = p; break; }
            }
            if (found < 0 && s->profile_count < FF_MAX_PROFILES) {
                found = s->profile_count;
                ff_str_copy(s->profiles[found].name,
                            sizeof(s->profiles[found].name), v);
                s->profile_count++;
            }
            if (found >= 0) s->current_profile = found;
        } else if (strcmp(a, "-ProfileManager") == 0 ||
                   strcmp(a, "--profilemanager") == 0) {
            s->is_first_run = 1;
        } else if (strcmp(a, "-private") == 0 ||
                   strcmp(a, "--private-window") == 0) {
            firefox_apply_privacy_mode(s, FF_MODE_PRIVATE);
        } else if (strcmp(a, "-private-window") == 0) {
            firefox_apply_privacy_mode(s, FF_MODE_PRIVATE);
        } else if (strcmp(a, "-tor") == 0) {
            firefox_apply_privacy_mode(s, FF_MODE_TOR);
        } else if (strcmp(a, "-safe-mode") == 0) {
            s->safe_mode = 1;
        } else if (strcmp(a, "-headless") == 0) {
            s->headless = 1;
        } else if (strcmp(a, "-no-remote") == 0) {
            s->no_remote = 1;
        } else if (strcmp(a, "-kiosk") == 0) {
            s->kiosk_mode = 1; s->allow_kiosk = 1;
        } else if (strcmp(a, "-kiosk-printing") == 0) {
            s->kiosk_print = 1;
        } else if (strcmp(a, "-CreateProfile") == 0 && i + 1 < argc) {
            firefox_profile_create(s, argv[++i], 0);
        } else if (strcmp(a, "-CreateProfile-default") == 0 && i + 1 < argc) {
            firefox_profile_create(s, argv[++i], 1);
        } else if (strcmp(a, "-purgecaches") == 0) {
            s->purge_caches_on_exit = 1;
        } else if (strcmp(a, "--purge-history") == 0) {
            s->purge_history_on_exit = 1;
        } else if (strcmp(a, "-new-window") == 0 && i + 1 < argc) {
            const char *v = argv[++i];
            int wid = 0;
            firefox_window_new(s, &wid);
            firefox_tab_new(s, wid, v, NULL);
            s->new_window_count_cmdline++;
        } else if (strcmp(a, "-new-tab") == 0 && i + 1 < argc) {
            const char *v = argv[++i];
            if (s->new_tab_url_count < 16)
                ff_str_copy(s->new_tab_urls[s->new_tab_url_count++],
                            FF_MAX_URL_LEN, v);
        } else if (strcmp(a, "-url") == 0 && i + 1 < argc) {
            const char *v = argv[++i];
            if (s->new_tab_url_count < 16)
                ff_str_copy(s->new_tab_urls[s->new_tab_url_count++],
                            FF_MAX_URL_LEN, v);
        } else if (strcmp(a, "-search") == 0 && i + 1 < argc) {
            char out[FF_MAX_URL_LEN];
            firefox_search_run(s, NULL, argv[++i], out);
            if (s->new_tab_url_count < 16)
                ff_str_copy(s->new_tab_urls[s->new_tab_url_count++],
                            FF_MAX_URL_LEN, out);
        } else if (strcmp(a, "--engine") == 0 && i + 1 < argc) {
            firefox_search_engine_set_default(s, argv[++i]);
        } else if (strcmp(a, "-width") == 0 && i + 1 < argc) {
            /* ignore; for headless dump */
            i++;
        } else if (strcmp(a, "-height") == 0 && i + 1 < argc) {
            i++;
        } else if (strcmp(a, "-screenshot") == 0) {
            s->headless = 1;
        } else if (strcmp(a, "-foreground") == 0 ||
                   strcmp(a, "--foreground") == 0) {
            /* no-op stub: bring existing window to foreground */
        } else if (strcmp(a, "-offline") == 0 ||
                   strcmp(a, "--offline") == 0) {
            s->proxy.type = FF_PROXY_DIRECT;
        } else if (strcmp(a, "-v") == 0 || strcmp(a, "--verbose") == 0) {
            s->verbose++;
        } else if (strcmp(a, "-debug") == 0) {
            s->debug = 1; s->verbose++;
        } else if (strcmp(a, "--version") == 0 || strcmp(a, "-V") == 0) {
            firefox_print_version();
            s->exit_code = 0;
            return 0;
        } else if (strcmp(a, "--help") == 0 || strcmp(a, "-h") == 0 ||
                   strcmp(a, "-?") == 0) {
            firefox_print_help();
            s->exit_code = 0;
            return 0;
        } else if (a[0] != '-') {
            /* Treat as URL */
            if (s->new_tab_url_count < 16)
                ff_str_copy(s->new_tab_urls[s->new_tab_url_count++],
                            FF_MAX_URL_LEN, a);
        }
    }
    return 0;
}

/* ---------- Profile management ---------- */

int firefox_profile_scan(FirefoxState *s) {
    /* Skeleton: populate with a single default profile if none exist */
    if (s->profile_count == 0) {
        FfProfile *p = &s->profiles[s->profile_count++];
        ff_str_copy(p->name, sizeof(p->name), "default");
        snprintf(p->root_dir, sizeof(p->root_dir), "%s/default",
                 s->profile_path);
        ff_str_copy(p->local_dir, sizeof(p->local_dir), p->root_dir);
        p->is_default = 1;
        p->is_relative = 1;
        s->current_profile = 0;
    }
    /* Mark default */
    for (int i = 0; i < s->profile_count; i++) {
        if (s->profiles[i].is_default) {
            s->current_profile = i;
            break;
        }
    }
    return s->profile_count;
}

int firefox_profile_create(FirefoxState *s, const char *name, int make_default) {
    if (!s || !name) return -1;
    if (s->profile_count >= FF_MAX_PROFILES) return -1;
    /* Reject duplicates */
    for (int i = 0; i < s->profile_count; i++) {
        if (strcmp(s->profiles[i].name, name) == 0) return i;
    }
    FfProfile *p = &s->profiles[s->profile_count];
    memset(p, 0, sizeof(*p));
    ff_str_copy(p->name, sizeof(p->name), name);
    snprintf(p->root_dir, sizeof(p->root_dir), "%s/%s",
             s->profile_path, name);
    ff_str_copy(p->local_dir, sizeof(p->local_dir), p->root_dir);
    p->is_relative = 1;
    if (make_default) {
        for (int i = 0; i < s->profile_count; i++)
            s->profiles[i].is_default = 0;
        p->is_default = 1;
        s->current_profile = s->profile_count;
    }
    return s->profile_count++;
}

int firefox_profile_load(FirefoxState *s, int idx) {
    if (!s || idx < 0 || idx >= s->profile_count) return -1;
    s->current_profile = idx;
    s->profiles[idx].is_locked = 1;
    s->profiles[idx].locked_reason = 0;
    s->last_profile_save = time(NULL);
    /* Skeleton: would read prefs.js / places.sqlite / cookies.sqlite here */
    firefox_bookmarks_load(s);
    return 0;
}

int firefox_profile_save(FirefoxState *s) {
    if (!s || s->current_profile < 0) return -1;
    s->last_profile_save = time(NULL);
    s->profiles[s->current_profile].is_locked = 0;
    /* Skeleton: would write prefs.js / session.json / places.sqlite here */
    firefox_bookmarks_save(s);
    return 0;
}

int firefox_purge_private_data(FirefoxState *s, int flags) {
    if (!s) return -1;
    if (flags & FF_PURGE_COOKIES) {
        s->cookie_count = 0;
    }
    if (flags & FF_PURGE_CACHE) {
        firefox_cache_clear_all(s);
    }
    if (flags & FF_PURGE_HISTORY) {
        s->history_count = 0;
    }
    if (flags & FF_PURGE_FORMDATA) {
        /* form history not modelled in this skeleton */
    }
    if (flags & FF_PURGE_DOWNLOADS) {
        s->download_count = 0;
    }
    if (flags & FF_PURGE_SESSIONS) {
        s->restore_last_session = 0;
    }
    if (flags & FF_PURGE_PASSWORDS) {
        s->saved_login_count = 0;
    }
    return 0;
}

/* ---------- Window / Tabs ---------- */

FfWindow *firefox_window_get(FirefoxState *s, int id) {
    if (!s) return NULL;
    for (int i = 0; i < s->window_count; i++) {
        if (s->windows[i].id == id) return &s->windows[i];
    }
    return NULL;
}

int firefox_window_new(FirefoxState *s, int *out_id) {
    if (!s) return -1;
    if (s->window_count >= FF_MAX_WINDOWS) return -1;
    FfWindow *w = &s->windows[s->window_count];
    memset(w, 0, sizeof(*w));
    w->id = s->window_count + 1;
    w->x = 80; w->y = 80; w->w = 1280; w->h = 800;
    w->show_menu_bar = 0;
    w->show_tab_bar = 1;
    w->show_nav_bar = 1;
    w->show_bookmarks_bar = 1;
    w->show_status_bar = 1;
    w->active_tab = -1;
    w->created_at = time(NULL);
    w->last_activated = w->created_at;
    ff_str_copy(w->search_engine, sizeof(w->search_engine),
                s->search_engines[s->default_search_engine].name);
    if (out_id) *out_id = w->id;
    s->active_window = s->window_count;
    return s->window_count++;
}

int firefox_window_close(FirefoxState *s, int id) {
    if (!s) return -1;
    FfWindow *w = firefox_window_get(s, id);
    if (!w) return -1;
    /* Close all tabs in this window. Each firefox_tab_close shrinks
       w->tab_count, so we always close index 0 until empty. */
    while (w->tab_count > 0) {
        firefox_tab_close(s, w->tabs[0]);
    }
    /* Compact windows array */
    int idx = (int)(w - s->windows);
    if (idx < s->window_count - 1) {
        memmove(&s->windows[idx], &s->windows[idx + 1],
                (s->window_count - idx - 1) * sizeof(FfWindow));
    }
    s->window_count--;
    if (s->active_window >= s->window_count && s->window_count > 0)
        s->active_window = s->window_count - 1;
    return 0;
}

int firefox_window_activate_tab(FirefoxState *s, int win_id, int tab_id) {
    FfWindow *w = firefox_window_get(s, win_id);
    if (!w) return -1;
    if (firefox_tab_find_by_id(s, tab_id) < 0) return -1;
    w->active_tab = tab_id;
    w->last_activated = time(NULL);
    return 0;
}

int firefox_tab_find_by_id(FirefoxState *s, int id) {
    if (!s) return -1;
    for (int i = 0; i < s->tab_count; i++) {
        if (s->tabs[i].id == id) return i;
    }
    return -1;
}

FfTab *firefox_tab_get(FirefoxState *s, int id) {
    int idx = firefox_tab_find_by_id(s, id);
    return (idx < 0) ? NULL : &s->tabs[idx];
}

int firefox_tab_new(FirefoxState *s, int window_id, const char *url, int *out_id) {
    if (!s) return -1;
    FfWindow *w = firefox_window_get(s, window_id);
    if (!w) return -1;
    if (w->tab_count >= FF_MAX_TABS) return -1;
    if (s->tab_count >= FF_MAX_TABS) return -1;

    FfTab *t = &s->tabs[s->tab_count];
    memset(t, 0, sizeof(*t));
    t->id = s->next_tab_id++;
    t->window_id = window_id;
    t->state = FF_TAB_IDLE;
    t->security = FF_SSL_NONE;
    t->privacy = FF_MODE_NORMAL;
    t->loading_pct = 0;
    t->zoom_pct = 100;
    t->opened_at = time(NULL);
    t->last_active = t->opened_at;
    ff_str_copy(t->home_page, sizeof(t->home_page), s->home_url);
    ff_str_copy(t->user_agent, sizeof(t->user_agent),
                "Mozilla/5.0 (KenuxK; Minimal) Gecko/20240101 Firefox/125.0");
    ff_str_copy(t->accept_lang, sizeof(t->accept_lang), s->accept_language);

    const char *target = (url && url[0]) ? url : s->newtab_url;
    ff_str_copy(t->url, sizeof(t->url), target);
    ff_str_copy(t->original_url, sizeof(t->original_url), target);
    ff_str_copy(t->pending_url, sizeof(t->pending_url), "");

    /* Init history stack */
    t->history_idx = 0;
    t->history_len = 0;
    if (target[0]) {
        ff_str_copy(t->history_stack[0], FF_MAX_URL_LEN, target);
        t->history_len = 1;
    }

    w->tabs[w->tab_count++] = t->id;
    if (w->active_tab < 0) w->active_tab = t->id;

    if (out_id) *out_id = t->id;
    s->tab_count++;
    if (target[0] && ff_str_starts_with(target, "about:"))
        t->state = FF_TAB_LOADED;
    else if (target[0])
        firefox_load_url(s, t->id, target);
    return t->id;
}

int firefox_tab_close(FirefoxState *s, int tab_id) {
    int idx = firefox_tab_find_by_id(s, tab_id);
    if (idx < 0) return -1;
    FfTab *t = &s->tabs[idx];
    FfWindow *w = firefox_window_get(s, t->window_id);
    if (w) {
        for (int i = 0; i < w->tab_count; i++) {
            if (w->tabs[i] == tab_id) {
                if (i < w->tab_count - 1)
                    memmove(&w->tabs[i], &w->tabs[i + 1],
                            (w->tab_count - i - 1) * sizeof(int));
                w->tab_count--;
                if (w->active_tab == tab_id) {
                    w->active_tab = (w->tab_count > 0)
                        ? w->tabs[(i > 0 ? i - 1 : 0)]
                        : -1;
                }
                break;
            }
        }
    }
    if (t->page_bytes) { free(t->page_bytes); t->page_bytes = NULL; }
    if (idx < s->tab_count - 1)
        memmove(&s->tabs[idx], &s->tabs[idx + 1],
                (s->tab_count - idx - 1) * sizeof(FfTab));
    s->tab_count--;
    return 0;
}

int firefox_tab_navigate(FirefoxState *s, int tab_id, const char *url) {
    FfTab *t = firefox_tab_get(s, tab_id);
    if (!t || !url) return -1;
    /* truncate forward history */
    if (t->history_idx + 1 < t->history_len) {
        t->history_len = t->history_idx + 1;
    }
    if (t->history_len < 256) {
        ff_str_copy(t->history_stack[t->history_len],
                    FF_MAX_URL_LEN, url);
        t->history_len++;
        t->history_idx = t->history_len - 1;
    } else {
        /* shift */
        for (int i = 1; i < 256; i++)
            memcpy(t->history_stack[i - 1], t->history_stack[i],
                   FF_MAX_URL_LEN);
        ff_str_copy(t->history_stack[255], FF_MAX_URL_LEN, url);
        t->history_idx = 255;
        t->history_len = 256;
    }
    ff_str_copy(t->original_url, sizeof(t->original_url), url);
    return firefox_load_url(s, tab_id, url);
}

int firefox_tab_reload(FirefoxState *s, int tab_id) {
    FfTab *t = firefox_tab_get(s, tab_id);
    if (!t) return -1;
    if (t->url[0] == 0) return -1;
    return firefox_load_url(s, tab_id, t->url);
}

int firefox_tab_go_back(FirefoxState *s, int tab_id) {
    FfTab *t = firefox_tab_get(s, tab_id);
    if (!t) return -1;
    if (t->history_idx <= 0) return -1;
    t->history_idx--;
    const char *url = t->history_stack[t->history_idx];
    ff_str_copy(t->url, sizeof(t->url), url);
    firefox_load_url(s, tab_id, url);
    return 0;
}

int firefox_tab_go_forward(FirefoxState *s, int tab_id) {
    FfTab *t = firefox_tab_get(s, tab_id);
    if (!t) return -1;
    if (t->history_idx + 1 >= t->history_len) return -1;
    t->history_idx++;
    const char *url = t->history_stack[t->history_idx];
    ff_str_copy(t->url, sizeof(t->url), url);
    firefox_load_url(s, tab_id, url);
    return 0;
}

/* ---------- URL loading & minimal rendering ---------- */

int firefox_load_url(FirefoxState *s, int tab_id, const char *url) {
    FfTab *t = firefox_tab_get(s, tab_id);
    if (!t || !url) return -1;
    ff_str_copy(t->url, sizeof(t->url), url);
    ff_str_copy(t->pending_url, sizeof(t->pending_url), url);
    t->state = FF_TAB_LOADING;
    t->loading_pct = 0;
    t->error_code = 0;
    t->error_description[0] = 0;

    /* about: pages are immediately "loaded" */
    if (ff_str_starts_with(url, "about:")) {
        t->state = FF_TAB_LOADED;
        t->loading_pct = 100;
        t->security = FF_SSL_NONE;
        if (strcmp(url, "about:blank") == 0) {
            t->title[0] = 0;
        } else if (strcmp(url, "about:newtab") == 0 ||
                   strcmp(url, "about:home") == 0) {
            ff_str_copy(t->title, sizeof(t->title), "New Tab");
        } else {
            ff_str_copy(t->title, sizeof(t->title), url);
        }
        return 0;
    }

    /* Set security state based on scheme */
    if (ff_str_starts_with(url, "https://")) {
        t->security = FF_SSL_HTTPS_NORMAL;
    } else if (ff_str_starts_with(url, "http://")) {
        t->security = FF_SSL_NONE;
    } else if (ff_str_starts_with(url, "file://")) {
        t->security = FF_SSL_NONE;
    } else if (ff_str_starts_with(url, "ftp://")) {
        t->security = FF_SSL_NONE;
    }

    firefox_history_add(s, url, t->title, t->original_url, 0);
    firefox_render_minimal(s, tab_id);
    t->loading_pct = 100;
    t->state = FF_TAB_LOADED;
    t->last_active = time(NULL);
    return 0;
}

int firefox_render_minimal(FirefoxState *s, int tab_id) {
    FfTab *t = firefox_tab_get(s, tab_id);
    if (!t) return -1;
    /* Skeleton: synthesise a tiny placeholder DOM reflecting the URL */
    char buf[1024];
    snprintf(buf, sizeof(buf),
             "<!doctype html><html><head><title>%s</title></head>"
             "<body><h1>%s</h1><p>Loaded by %s engine.</p></body></html>",
             t->url[0] ? t->url : "about:blank",
             t->url[0] ? t->url : "about:blank",
             ff_engine_name(s->engine));
    size_t need = strlen(buf);
    if (t->page_bytes) { free(t->page_bytes); t->page_bytes = NULL; }
    t->page_bytes = (uint8_t *)malloc(need + 1);
    if (!t->page_bytes) { t->page_len = 0; return -1; }
    memcpy(t->page_bytes, buf, need);
    t->page_bytes[need] = 0;
    t->page_len = need;
    if (t->title[0] == 0)
        ff_str_copy(t->title, sizeof(t->title), t->url);
    return 0;
}

int firefox_extract_plain_text(FirefoxState *s, int tab_id,
                               char *out, size_t out_size) {
    FfTab *t = firefox_tab_get(s, tab_id);
    if (!t || !out || out_size == 0) return -1;
    if (!t->page_bytes || t->page_len == 0) {
        out[0] = 0;
        return 0;
    }
    size_t oi = 0;
    int in_tag = 0;
    for (size_t i = 0; i < t->page_len && oi < out_size - 1; i++) {
        char c = (char)t->page_bytes[i];
        if (c == '<') { in_tag = 1; continue; }
        if (c == '>') { in_tag = 0; continue; }
        if (in_tag) continue;
        if (c == '\n' || c == '\r') continue;
        out[oi++] = c;
    }
    out[oi] = 0;
    return (int)oi;
}

/* ---------- History ---------- */

int firefox_history_add(FirefoxState *s, const char *url, const char *title,
                        const char *referrer, int transition) {
    if (!s || !url) return -1;
    if (!s->remember_history) return 0;
    if (ff_str_starts_with(url, "about:")) return 0;
    if (s->history_count >= FF_MAX_HISTORY) {
        /* shift oldest */
        memmove(&s->history[0], &s->history[1],
                (FF_MAX_HISTORY - 1) * sizeof(FfHistoryItem));
        s->history_count = FF_MAX_HISTORY - 1;
    }
    FfHistoryItem *h = &s->history[s->history_count++];
    memset(h, 0, sizeof(*h));
    ff_str_copy(h->url, sizeof(h->url), url);
    ff_str_copy(h->title, sizeof(h->title), title ? title : "");
    ff_str_copy(h->referrer, sizeof(h->referrer), referrer ? referrer : "");
    h->visit_time = time(NULL);
    h->visit_count = 1;
    h->transition = transition;
    h->typed = (transition == 1);
    return 0;
}

int firefox_history_query(FirefoxState *s, const char *needle,
                          int *indices_out, int max_out) {
    if (!s || !indices_out || max_out <= 0) return 0;
    int found = 0;
    for (int i = s->history_count - 1; i >= 0 && found < max_out; i--) {
        if (!needle || needle[0] == 0) {
            indices_out[found++] = i;
        } else if (strstr(s->history[i].url, needle) ||
                   strstr(s->history[i].title, needle)) {
            indices_out[found++] = i;
        }
    }
    return found;
}

int firefox_history_clear_range(FirefoxState *s, time_t from, time_t to) {
    if (!s) return -1;
    if (to == 0) to = time(NULL);
    int j = 0;
    for (int i = 0; i < s->history_count; i++) {
        if (s->history[i].visit_time >= from &&
            s->history[i].visit_time <= to) {
            continue;
        }
        if (i != j) s->history[j] = s->history[i];
        j++;
    }
    s->history_count = j;
    return 0;
}

/* ---------- Bookmarks ---------- */

FfBookmark *firefox_bookmark_new(FfBookmarkType type, const char *title,
                                 const char *url) {
    FfBookmark *b = (FfBookmark *)calloc(1, sizeof(FfBookmark));
    if (!b) return NULL;
    b->type = type;
    ff_generate_guid(b->guid, sizeof(b->guid));
    ff_str_copy(b->title, sizeof(b->title), title ? title : "");
    ff_str_copy(b->url, sizeof(b->url), url ? url : "");
    b->id = 0;
    b->parent_id = -1;
    b->position = 0;
    b->date_added = (int)time(NULL);
    b->last_modified = b->date_added;
    b->child_count = 0;
    b->child_cap = 0;
    b->children = NULL;
    return b;
}

int firefox_bookmark_add_to_folder(FfBookmark *folder, FfBookmark *child) {
    if (!folder || !child) return -1;
    if (folder->type != FF_BMARK_FOLDER && folder->type != FF_BMARK_NONE) {
        folder->type = FF_BMARK_FOLDER;
    }
    if (folder->child_count >= folder->child_cap) {
        int newcap = folder->child_cap ? folder->child_cap * 2 : 8;
        FfBookmark **na = (FfBookmark **)realloc(folder->children,
                                                  newcap * sizeof(FfBookmark *));
        if (!na) return -1;
        folder->children = na;
        folder->child_cap = newcap;
    }
    child->parent_id = folder->id;
    child->position = folder->child_count;
    folder->children[folder->child_count++] = child;
    folder->last_modified = (int)time(NULL);
    return 0;
}

static FfBookmark *ff_bookmark_find_url_recursive(const FfBookmark *root,
                                                  const char *url) {
    if (!root || !url) return NULL;
    if (root->type == FF_BMARK_URL && strcmp(root->url, url) == 0)
        return (FfBookmark *)root;
    for (int i = 0; i < root->child_count; i++) {
        FfBookmark *r = ff_bookmark_find_url_recursive(root->children[i], url);
        if (r) return r;
    }
    return NULL;
}

FfBookmark *firefox_bookmark_find_url(const FfBookmark *root, const char *url) {
    return ff_bookmark_find_url_recursive(root, url);
}

int firefox_bookmark_remove(FfBookmark *parent, const char *guid) {
    if (!parent || !guid) return -1;
    for (int i = 0; i < parent->child_count; i++) {
        if (strcmp(parent->children[i]->guid, guid) == 0) {
            ff_free_bookmark_tree(parent->children[i]);
            if (i < parent->child_count - 1)
                memmove(&parent->children[i], &parent->children[i + 1],
                        (parent->child_count - i - 1) * sizeof(FfBookmark *));
            parent->child_count--;
            parent->last_modified = (int)time(NULL);
            return 0;
        }
    }
    return -1;
}

int firefox_bookmarks_save(const FirefoxState *s) {
    if (!s) return -1;
    /* Skeleton: would serialise to places.sqlite / bookmarks.json */
    return 0;
}

int firefox_bookmarks_load(FirefoxState *s) {
    if (!s) return -1;
    if (s->root_bookmark) return 0;
    s->root_bookmark = firefox_bookmark_new(FF_BMARK_FOLDER, "root", "");
    s->bookmarks_menu    = firefox_bookmark_new(FF_BMARK_FOLDER, "menu", "");
    s->bookmarks_toolbar = firefox_bookmark_new(FF_BMARK_FOLDER, "toolbar", "");
    s->bookmarks_unfiled = firefox_bookmark_new(FF_BMARK_FOLDER, "unfiled", "");
    s->bookmarks_mobile  = firefox_bookmark_new(FF_BMARK_FOLDER, "mobile", "");
    firefox_bookmark_add_to_folder(s->root_bookmark, s->bookmarks_menu);
    firefox_bookmark_add_to_folder(s->root_bookmark, s->bookmarks_toolbar);
    firefox_bookmark_add_to_folder(s->root_bookmark, s->bookmarks_unfiled);
    firefox_bookmark_add_to_folder(s->root_bookmark, s->bookmarks_mobile);
    s->bookmark_id_counter = 1;
    return 0;
}

/* ---------- Cookies ---------- */

int firefox_cookie_add(FirefoxState *s, const FfCookie *cookie) {
    if (!s || !cookie) return -1;
    if (!s->accept_cookies) return 0;
    if (s->cookie_count >= FF_MAX_COOKIES) {
        /* drop oldest */
        memmove(&s->cookies[0], &s->cookies[1],
                (FF_MAX_COOKIES - 1) * sizeof(FfCookie));
        s->cookie_count = FF_MAX_COOKIES - 1;
    }
    s->cookies[s->cookie_count++] = *cookie;
    return 0;
}

int firefox_cookie_matches(const FfCookie *c, const char *host,
                           const char *path, int is_secure) {
    if (!c || !host || !path) return 0;
    /* domain match (suffix, ignoring leading dot) */
    const char *dom = c->domain;
    if (dom[0] == '.') dom++;
    size_t hlen = strlen(host), dlen = strlen(dom);
    if (hlen < dlen) return 0;
    if (strcmp(host + hlen - dlen, dom) != 0) {
        if (!c->host_only) return 0;
        if (strcmp(host, c->domain) != 0) return 0;
    }
    /* path prefix match */
    size_t plen = strlen(path);
    size_t clen = strlen(c->path);
    if (plen < clen) return 0;
    if (strncmp(path, c->path, clen) != 0) return 0;
    /* secure cookies require secure transport */
    if (c->secure && !is_secure) return 0;
    /* expiry */
    if (c->expires != 0 && c->expires < time(NULL)) return 0;
    return 1;
}

int firefox_cookie_delete_expired(FirefoxState *s) {
    if (!s) return -1;
    time_t now = time(NULL);
    int j = 0, dropped = 0;
    for (int i = 0; i < s->cookie_count; i++) {
        if (s->cookies[i].expires != 0 && s->cookies[i].expires < now) {
            dropped++;
            continue;
        }
        if (i != j) s->cookies[j] = s->cookies[i];
        j++;
    }
    s->cookie_count = j;
    return dropped;
}

int firefox_cookie_delete_domain(FirefoxState *s, const char *domain_suffix) {
    if (!s || !domain_suffix) return -1;
    int j = 0, dropped = 0;
    for (int i = 0; i < s->cookie_count; i++) {
        if (ff_str_ends_with(s->cookies[i].domain, domain_suffix)) {
            dropped++;
            continue;
        }
        if (i != j) s->cookies[j] = s->cookies[i];
        j++;
    }
    s->cookie_count = j;
    return dropped;
}

/* ---------- Cache ---------- */

int firefox_cache_store(FirefoxState *s, const FfCacheEntry *entry) {
    if (!s || !entry) return -1;
    if (!s->cache_enabled) return 0;
    /* replace if same URL exists */
    for (int i = 0; i < s->cache_count; i++) {
        if (strcmp(s->cache[i].url, entry->url) == 0) {
            if (s->cache[i].content) free(s->cache[i].content);
            s->cache[i] = *entry;
            s->cache[i].hit_count = 0;
            return 0;
        }
    }
    if (s->cache_count >= FF_MAX_CACHE_ENTRIES) {
        firefox_cache_evict_lru(s);
    }
    if (s->cache_count >= FF_MAX_CACHE_ENTRIES) return -1;
    s->cache[s->cache_count++] = *entry;
    return 0;
}

int firefox_cache_fetch(FirefoxState *s, const char *url, FfCacheEntry *out) {
    if (!s || !url || !out) return -1;
    for (int i = 0; i < s->cache_count; i++) {
        if (strcmp(s->cache[i].url, url) == 0) {
            *out = s->cache[i];
            s->cache[i].hit_count++;
            /* MRU: move to end */
            if (i < s->cache_count - 1) {
                FfCacheEntry tmp = s->cache[i];
                memmove(&s->cache[i], &s->cache[i + 1],
                        (s->cache_count - i - 1) * sizeof(FfCacheEntry));
                s->cache[s->cache_count - 1] = tmp;
            }
            return 0;
        }
    }
    return -1;
}

int firefox_cache_evict_lru(FirefoxState *s) {
    if (!s || s->cache_count == 0) return 0;
    /* evict lowest hit_count */
    int worst = 0;
    for (int i = 1; i < s->cache_count; i++) {
        if (s->cache[i].hit_count < s->cache[worst].hit_count)
            worst = i;
    }
    if (s->cache[worst].content) {
        free(s->cache[worst].content);
        s->cache[worst].content = NULL;
    }
    if (worst < s->cache_count - 1)
        memmove(&s->cache[worst], &s->cache[worst + 1],
                (s->cache_count - worst - 1) * sizeof(FfCacheEntry));
    s->cache_count--;
    return 0;
}

int firefox_cache_clear_all(FirefoxState *s) {
    if (!s) return -1;
    for (int i = 0; i < s->cache_count; i++) {
        if (s->cache[i].content) {
            free(s->cache[i].content);
            s->cache[i].content = NULL;
        }
    }
    s->cache_count = 0;
    return 0;
}

/* ---------- Downloads ---------- */

int firefox_download_start(FirefoxState *s, const FfDownload *dl, int *out_id) {
    if (!s || !dl) return -1;
    if (s->download_count >= 256) return -1;
    FfDownload *d = &s->downloads[s->download_count];
    *d = *dl;
    d->start_time = time(NULL);
    d->received_bytes = 0;
    d->paused = 0;
    d->completed = 0;
    d->cancelled = 0;
    d->failed = 0;
    int id = s->download_count;
    if (out_id) *out_id = id;
    s->download_count++;
    return id;
}

int firefox_download_cancel(FirefoxState *s, int id) {
    if (!s || id < 0 || id >= s->download_count) return -1;
    s->downloads[id].cancelled = 1;
    return 0;
}

int firefox_download_pause(FirefoxState *s, int id, int pause) {
    if (!s || id < 0 || id >= s->download_count) return -1;
    s->downloads[id].paused = pause ? 1 : 0;
    return 0;
}

int firefox_download_progress(FirefoxState *s, int id,
                              size_t *out_received, size_t *out_total) {
    if (!s || id < 0 || id >= s->download_count) return -1;
    FfDownload *d = &s->downloads[id];
    if (out_received) *out_received = d->received_bytes;
    if (out_total)   *out_total   = d->total_bytes;
    return d->completed ? 0 : (d->cancelled || d->failed ? -1 : 1);
}

/* ---------- Search ---------- */

int firefox_search_engine_set_default(FirefoxState *s, const char *name) {
    if (!s || !name) return -1;
    for (int i = 0; i < s->search_engine_count; i++) {
        if (strcmp(s->search_engines[i].name, name) == 0 ||
            strcmp(s->search_engines[i].alias, name) == 0) {
            for (int j = 0; j < s->search_engine_count; j++)
                s->search_engines[j].is_default = 0;
            s->search_engines[i].is_default = 1;
            s->default_search_engine = i;
            return 0;
        }
    }
    return -1;
}

int firefox_search_run(FirefoxState *s, const char *engine_alias,
                       const char *query, char out_url[FF_MAX_URL_LEN]) {
    if (!s || !query || !out_url) return -1;
    FfSearchEngine *e = NULL;
    if (engine_alias && engine_alias[0]) {
        for (int i = 0; i < s->search_engine_count; i++) {
            if (strcmp(s->search_engines[i].alias, engine_alias) == 0 ||
                strcmp(s->search_engines[i].name,  engine_alias) == 0) {
                e = &s->search_engines[i];
                break;
            }
        }
    }
    if (!e) e = &s->search_engines[s->default_search_engine];
    ff_search_substitute(e->search_url, query, out_url, FF_MAX_URL_LEN);
    return 0;
}

/* ---------- Privacy ---------- */

int firefox_apply_privacy_mode(FirefoxState *s, FfPrivacyMode mode) {
    if (!s) return -1;
    FfTab *t = (s->tab_count > 0) ? &s->tabs[s->tab_count - 1] : NULL;
    switch (mode) {
        case FF_MODE_NORMAL:
            s->privacy.tracking_protection = 1;
            s->privacy.etp_level = FF_ETP_STANDARD;
            s->privacy.resist_fingerprinting = 0;
            s->privacy.https_only_mode = 1;
            s->remember_history = 1;
            s->accept_cookies = 1;
            if (t) t->privacy = FF_MODE_NORMAL;
            break;
        case FF_MODE_PRIVATE:
            s->privacy.tracking_protection = 1;
            s->privacy.etp_level = FF_ETP_STRICT;
            s->privacy.resist_fingerprinting = 1;
            s->privacy.https_only_mode = 2;
            s->remember_history = 0;
            s->accept_cookies = 1;          /* session only */
            s->lifetime_policy = 2;         /* session */
            s->privacy.cookie_behaviour = 2;
            if (t) t->privacy = FF_MODE_PRIVATE;
            break;
        case FF_MODE_TOR:
            s->privacy.tracking_protection = 1;
            s->privacy.etp_level = FF_ETP_STRICT;
            s->privacy.resist_fingerprinting = 1;
            s->privacy.https_only_mode = 2;
            s->proxy.type = FF_PROXY_SOCKS;
            ff_str_copy(s->proxy.socks_host, sizeof(s->proxy.socks_host),
                        "127.0.0.1");
            s->proxy.socks_port = 9050;
            s->proxy.socks_version = 5;
            s->proxy.proxy_dns_over_socks = 1;
            if (t) t->privacy = FF_MODE_TOR;
            break;
        case FF_MODE_STRICT_ETP:
            s->privacy.etp_level = FF_ETP_STRICT;
            s->privacy.block_trackers = 1;
            s->privacy.block_cookies_third_party = 1;
            s->privacy.block_fingerprinters = 1;
            s->privacy.block_cryptominers = 1;
            s->privacy.block_social_trackers = 1;
            if (t) t->privacy = FF_MODE_STRICT_ETP;
            break;
        case FF_MODE_FINGERPRINTING:
            s->privacy.block_fingerprinters = 1;
            s->privacy.resist_fingerprinting = 1;
            s->privacy.can_fingerprint_canvas = 0;
            if (t) t->privacy = FF_MODE_FINGERPRINTING;
            break;
        case FF_MODE_RFP:
            s->privacy.resist_fingerprinting = 1;
            s->privacy.privacy_reduce_timer_resolution = 1;
            if (t) t->privacy = FF_MODE_RFP;
            break;
    }
    return 0;
}

/* ---------- Extensions ---------- */

int firefox_extension_install(FirefoxState *s, const char *xpi_or_dir) {
    if (!s || !xpi_or_dir) return -1;
    if (s->extension_count >= FF_MAX_EXTENSIONS) return -1;
    if (!s->xpinstall_allowed) return -1;
    FfExtension *e = &s->extensions[s->extension_count];
    memset(e, 0, sizeof(*e));
    ff_str_copy(e->id, sizeof(e->id), xpi_or_dir);
    ff_str_copy(e->name, sizeof(e->name), xpi_or_dir);
    ff_str_copy(e->version_str, sizeof(e->version_str), "1.0.0");
    ff_str_copy(e->manifest_version, sizeof(e->manifest_version), "2");
    ff_str_copy(e->path, sizeof(e->path), xpi_or_dir);
    e->enabled = 1;
    e->temporarily_installed = 0;
    e->can_access_private = 0;
    e->startup_run = 0;
    if (s->safe_mode) e->enabled = 0;
    s->extension_count++;
    return s->extension_count - 1;
}

int firefox_extension_enable(FirefoxState *s, const char *id, int enable) {
    if (!s || !id) return -1;
    for (int i = 0; i < s->extension_count; i++) {
        if (strcmp(s->extensions[i].id, id) == 0) {
            s->extensions[i].enabled = enable ? 1 : 0;
            return 0;
        }
    }
    return -1;
}

int firefox_extension_startup(FirefoxState *s) {
    if (!s) return -1;
    if (s->safe_mode) return 0;
    int started = 0;
    for (int i = 0; i < s->extension_count; i++) {
        if (s->extensions[i].enabled) {
            s->extensions[i].startup_run = 1;
            started++;
        }
    }
    return started;
}

/* ---------- CLI / runtime ---------- */

int firefox_start_headless_dump(FirefoxState *s, const char *url,
                                const char *out_file, int format,
                                int width, int height) {
    if (!s || !url || !out_file) return -1;
    s->headless = 1;
    int wid = 0, tid = 0;
    if (firefox_window_new(s, &wid) < 0) return -1;
    if (firefox_tab_new(s, wid, url, &tid) < 0) return -1;
    FfTab *t = firefox_tab_get(s, tid);
    if (!t) return -1;
    FILE *f = fopen(out_file, "w");
    if (!f) return -1;
    fprintf(f, "KenuxK-Firefox headless dump\n");
    fprintf(f, "URL: %s\n", t->url);
    fprintf(f, "Title: %s\n", t->title);
    fprintf(f, "Engine: %s\n", ff_engine_name(s->engine));
    fprintf(f, "Security: %s\n", ff_security_state_name(t->security));
    fprintf(f, "Format: %d  Viewport: %dx%d\n", format,
            width > 0 ? width : 1280, height > 0 ? height : 800);
    if (t->page_bytes && t->page_len > 0) {
        fprintf(f, "---- Content ----\n");
        fwrite(t->page_bytes, 1, t->page_len, f);
        fputc('\n', f);
    }
    fclose(f);
    return 0;
}

int firefox_run_browser_loop(FirefoxState *s) {
    if (!s) return -1;
    s->running = 1;
    /* Open initial windows / tabs requested on the command line */
    if (s->window_count == 0) {
        int wid = 0;
        firefox_window_new(s, &wid);
        if (s->new_tab_url_count == 0) {
            int tid = 0;
            firefox_tab_new(s, wid,
                            (s->restore_on_start == 2)
                                ? s->home_url : s->newtab_url,
                            &tid);
        } else {
            for (int i = 0; i < s->new_tab_url_count; i++) {
                int tid = 0;
                firefox_tab_new(s, wid, s->new_tab_urls[i], &tid);
            }
        }
    }
    if (s->headless) {
        s->exit_code = 0;
        s->running = 0;
        return s->exit_code;
    }
    /* Skeleton: no event loop in this minimal build; return immediately. */
    s->exit_code = 0;
    s->running = 0;
    return s->exit_code;
}

void firefox_print_version(void) {
    printf("%s\n", FIREFOX_VERSION_STR);
    printf("Engine: basic (custom), gecko-lite (stub), servo (stub)\n");
    printf("KenuxK browser skeleton; not a full Firefox build.\n");
}

void firefox_print_help(void) {
    printf("%s - KenuxK Firefox launcher (minimal)\n\n", FIREFOX_VERSION_STR);
    printf("USAGE: firefox [opts] [URL...]\n\n");
    printf("PROFILES:\n");
    printf("  -P NAME                Use profile NAME\n");
    printf("  -ProfileManager        Open profile manager\n");
    printf("  -CreateProfile NAME     Create a new profile\n");
    printf("  -CreateProfile-default NAME   Create and set as default\n");
    printf("MODES:\n");
    printf("  -private               Open a private window\n");
    printf("  -tor                   Apply Tor preset (socks proxy)\n");
    printf("  -safe-mode             Disable extensions\n");
    printf("  -headless              Run without UI\n");
    printf("  -kiosk                 Kiosk mode\n");
    printf("  -no-remote             Start a new process\n");
    printf("NAVIGATION:\n");
    printf("  -url URL               Open URL in new tab\n");
    printf("  -new-window URL        Open URL in new window\n");
    printf("  -new-tab URL           Open URL in new tab\n");
    printf("  -search TERMS          Search with default engine\n");
    printf("  --engine NAME          Set default search engine\n");
    printf("DEBUG / OUTPUT:\n");
    printf("  -screenshot FILE       Headless page dump\n");
    printf("  -v / --verbose         Increase verbosity\n");
    printf("  -debug                 Enable debug output\n");
    printf("  --version              Print version and exit\n");
    printf("  --help / -h            This help\n");
    printf("\nNOTE: This is a minimal skeleton implementation for the KenuxK project.\n");
}

/* ---------- main entry (optional) ---------- */

static int firefox_main_internal(int argc, char **argv) {
    static FirefoxState s;
    firefox_init(&s);
    if (firefox_parse_arguments(&s, argc, argv) != 0) {
        firefox_cleanup(&s);
        return s.exit_code;
    }
    if (s.exit_code != 0 || (argc == 1 && s.is_first_run)) {
        if (s.is_first_run && s.window_count == 0) {
            firefox_profile_scan(&s);
        }
    }
    firefox_profile_scan(&s);
    if (s.current_profile >= 0)
        firefox_profile_load(&s, s.current_profile);
    firefox_extension_startup(&s);
    int rc = firefox_run_browser_loop(&s);
    if (s.purge_caches_on_exit)   firefox_cache_clear_all(&s);
    if (s.purge_history_on_exit)  s.history_count = 0;
    firefox_profile_save(&s);
    firefox_cleanup(&s);
    return rc;
}

#ifndef KENUXK_NO_MAIN_FIREFOX
int main(int argc, char **argv) {
    return firefox_main_internal(argc, argv);
}
#endif
