/*
 * Kenux OS - Firefox-like Browser (Minimal)
 * Header file
 */

#ifndef _FIREFOX_H
#define _FIREFOX_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>

#define FIREFOX_VERSION_STR "KenuxK-Firefox 125.0 (Minimal)"

#define FF_MAX_TABS 16
#define FF_MAX_WINDOWS 4
#define FF_MAX_HISTORY 128
#define FF_MAX_BOOKMARKS 64
#define FF_MAX_COOKIES 64
#define FF_MAX_CACHE_ENTRIES 32
#define FF_MAX_PROFILES 8
#define FF_MAX_EXTENSIONS 16
#define FF_MAX_DOMAINS 32
#define FF_MAX_URL_LEN 4096
#define FF_MAX_TITLE_LEN 2048
#define FF_MAX_DATA 16 * 1024 * 1024

/* Rendering engine (minimal subset) */
typedef enum {
    FF_ENGINE_BASIC = 0,    /* Custom minimal + CSS2/HTML4 + tiny JS */
    FF_ENGINE_GECKO_LITE,   /* Gecko subset (stub) */
    FF_ENGINE_SERVO,        /* Servo subset (stub) */
} FfEngine;

/* Browser / security modes */
typedef enum {
    FF_MODE_NORMAL = 0,
    FF_MODE_PRIVATE,        /* Private window */
    FF_MODE_TOR,            /* Tor browser settings (stub) */
    FF_MODE_STRICT_ETP,     /* Strict Enhanced Tracking Protection */
    FF_MODE_FINGERPRINTING, /* fingerprinting protection */
    FF_MODE_RFP,            /* resistFingerprinting */
} FfPrivacyMode;

/* Tab state */
typedef enum {
    FF_TAB_LOADING = 0,
    FF_TAB_LOADED,
    FF_TAB_IDLE,
    FF_TAB_DISCARDED,   /* unloaded for memory */
    FF_TAB_CRASHED,
    FF_TAB_FROZEN,
} FfTabState;

typedef enum {
    FF_SSL_NONE = 0,
    FF_SSL_HTTPS_PADLOCK,       /* valid EV */
    FF_SSL_HTTPS_NORMAL,        /* valid DV/OV */
    FF_SSL_HTTPS_WARN_MIXED,
    FF_SSL_HTTPS_BROKEN,        /* invalid cert */
    FF_SSL_HTTPS_HSTS_ENFORCED,
} FfSecurityState;

/* History node */
typedef struct {
    char url[FF_MAX_URL_LEN];
    char title[FF_MAX_TITLE_LEN];
    char referrer[FF_MAX_URL_LEN];
    time_t visit_time;
    int visit_count;
    int visit_duration_ms;
    int typed;
    int transition;       /* 0 link, 1 typed, 2 auto-bookmark, 3 auto-subframe */
    int hidden;
    int from_restore;
} FfHistoryItem;

/* Bookmark */
typedef enum {
    FF_BMARK_NONE = 0,
    FF_BMARK_URL,
    FF_BMARK_FOLDER,
    FF_BMARK_SEPARATOR,
} FfBookmarkType;

typedef struct FfBookmark {
    FfBookmarkType type;
    char guid[64];
    char title[512];
    char url[FF_MAX_URL_LEN];
    char icon_uri[FF_MAX_URL_LEN];
    int parent_id;
    int id;
    int position;
    int date_added;
    int last_modified;
    char keyword[64];
    int tags[32];
    int tag_count;
    int child_count;
    int child_cap;
    struct FfBookmark **children;
} FfBookmark;

/* Cookie (RFC6265bis subset) */
typedef struct {
    char name[256];
    char value[4096];
    char domain[512];
    char path[1024];
    time_t expires;        /* 0 = session */
    int max_age;
    int secure;
    int http_only;
    int same_site;         /* 0 none, 1 lax, 2 strict */
    int partitioned;
    char same_party;
    int host_only;
    int persistent;
} FfCookie;

/* Cache entry (HTTP + assets) */
typedef struct {
    char url[FF_MAX_URL_LEN];
    char etag[512];
    char last_modified[128];
    char content_type[256];
    char content_encoding[64];
    time_t cached_at;
    time_t expires;
    int max_age_s;
    size_t content_length;
    uint8_t *content;
    int status_code;
    int hit_count;
    int stale_allowed;
} FfCacheEntry;

/* Content / Download */
typedef struct {
    char url[FF_MAX_URL_LEN];
    char filename[FF_MAX_URL_LEN];
    char save_path[FF_MAX_URL_LEN * 2];
    char mime_type[256];
    char referrer[FF_MAX_URL_LEN];
    size_t total_bytes;
    size_t received_bytes;
    time_t start_time;
    int paused;
    int completed;
    int cancelled;
    int failed;
    int error_code;
    int resume_capable;
    int auto_open_when_done;
} FfDownload;

/* Profile */
typedef struct {
    char name[128];
    char root_dir[FF_MAX_URL_LEN];
    char local_dir[FF_MAX_URL_LEN];
    int is_default;
    int is_relative;
    int is_locked;
    int locked_reason;
    char dedicated;       /* dedicated = 1 not to reuse */
    char user_js_path[FF_MAX_URL_LEN];
} FfProfile;

/* WebExtension */
typedef struct {
    char id[256];
    char name[512];
    char version_str[64];
    char manifest_version[16];
    char description[2048];
    char permissions[8192];
    char host_permissions[8192];
    char background_script[FF_MAX_URL_LEN];
    char options_ui[FF_MAX_URL_LEN];
    char popup[FF_MAX_URL_LEN];
    char icons[256];
    char path[FF_MAX_URL_LEN * 2];
    int enabled;
    int temporarily_installed;
    int can_access_private;
    int startup_run;
} FfExtension;

/* Password / credential (stub) */
typedef struct {
    char origin[FF_MAX_URL_LEN];
    char realm[512];
    char username[512];
    char password_enc[4096];   /* placeholder */
    char form_action_origin[FF_MAX_URL_LEN];
    char form_username_field[256];
    char form_password_field[256];
    time_t time_created;
    time_t time_last_used;
    time_t time_password_changed;
    int times_used;
} FfLoginInfo;

/* DOM content tab */
typedef struct FfTab {
    int id;
    int window_id;
    char url[FF_MAX_URL_LEN];
    char original_url[FF_MAX_URL_LEN];
    char pending_url[FF_MAX_URL_LEN];
    char title[FF_MAX_TITLE_LEN];
    char favicon_uri[FF_MAX_URL_LEN];
    char home_page[FF_MAX_URL_LEN];

    FfTabState state;
    FfSecurityState security;
    FfPrivacyMode privacy;
    int loading_pct;
    int error_code;
    char error_description[4096];

    char user_agent[4096];
    char accept_lang[256];
    int zoom_pct;           /* 30 .. 500 */
    int reader_view;
    int muted;
    int playing_audio;
    int picture_in_picture;
    int is_pinned;
    int container_id;

    /* Navigation */
    char history_stack[32][FF_MAX_URL_LEN];
    int history_idx;
    int history_len;

    /* Scroll position */
    int scroll_x;
    int scroll_y;
    int page_w;
    int page_h;

    /* DOM cache (minimal) */
    uint8_t *page_bytes;
    size_t page_len;

    time_t opened_at;
    time_t last_active;
    struct FfTab *next;
} FfTab;

/* Window */
typedef struct {
    int id;
    char title[256];
    int x, y;
    int w, h;
    int maximized;
    int minimized;
    int fullscreen;
    int always_on_top;

    /* Tab strip */
    int tabs[FF_MAX_TABS];
    int tab_count;
    int active_tab;

    /* Sidebar */
    int sidebar_open;
    char sidebar_panel[64];    /* bookmarks / history / synced-tabs / addons */

    /* Toolbar visibility */
    int show_menu_bar;
    int show_tab_bar;
    int show_nav_bar;
    int show_bookmarks_bar;
    int show_status_bar;

    int popup_allowed;
    int dnd_enabled;
    char search_engine[128];

    time_t created_at;
    time_t last_activated;
} FfWindow;

/* Search engine */
typedef struct {
    char name[256];
    char alias[64];
    char search_url[FF_MAX_URL_LEN];
    char suggest_url[FF_MAX_URL_LEN];
    char post_params[4096];
    char icon[FF_MAX_URL_LEN];
    int is_default;
    int order;
    int hidden;
} FfSearchEngine;

/* Proxy / Network */
typedef enum {
    FF_PROXY_DIRECT = 0,
    FF_PROXY_SYSTEM,
    FF_PROXY_MANUAL,
    FF_PROXY_AUTO_CONFIG,   /* PAC */
    FF_PROXY_SOCKS,
} FfProxyType;

typedef struct {
    FfProxyType type;
    char pac_url[FF_MAX_URL_LEN];
    char http_host[256];
    int  http_port;
    char https_host[256];
    int  https_port;
    char socks_host[256];
    int  socks_port;
    int  socks_version;    /* 4 or 5 */
    char no_proxy[4096];   /* comma separated */
    int  proxy_dns_over_socks;
    int  auth_required;
    char auth_user[256];
    char auth_pass[1024];   /* keep empty */
} FfProxySettings;

/* DNT / ETP */
typedef enum {
    FF_ETP_STANDARD = 0,
    FF_ETP_STRICT,
    FF_ETP_CUSTOM,
} FfEtpLevel;

typedef struct {
    int tracking_protection;
    FfEtpLevel etp_level;
    int block_trackers;
    int block_cookies_third_party;
    int block_fingerprinters;
    int block_cryptominers;
    int block_social_trackers;
    int cookie_behaviour;       /* 0 all, 1 reject-third-party, 2 isolate, 3 reject all */
    int resist_fingerprinting;
    int dnt;                    /* 1 = send DNT:1 */
    int global_privacy_control; /* 1 = send Sec-GPC:1 */
    int https_only_mode;        /* 0 off, 1 private-only, 2 all */
    int https_only_exceptions[FF_MAX_DOMAINS][256];
    int https_only_exception_count;
    int enable_webrtc;
    int webrtc_ip_policy;
    int privacy_reduce_timer_resolution;
    int block_popups;
    int block_redirects;
    int flash_storage_allowed;
    int offscreen_canvas_cross_origin_read_blocking;
    int webgl_min_version;     /* 0 off, 1 v1, 2 v2 */
    int webgpu_enabled;
    int can_fingerprint_canvas;
} FfPrivacySettings;

/* Main Firefox state */
typedef struct {
    int argc;
    char **argv;

    /* Profile */
    FfProfile profiles[FF_MAX_PROFILES];
    int profile_count;
    int current_profile;
    char profile_path[FF_MAX_URL_LEN * 2];
    char user_chrome_path[FF_MAX_URL_LEN * 2];
    char user_content_path[FF_MAX_URL_LEN * 2];

    /* Core engine (minimal) */
    FfEngine engine;
    int headless;
    int safe_mode;          /* no extensions */
    int no_remote;
    int allow_kiosk;
    int kiosk_mode;
    int kiosk_print;
    int purge_caches_on_exit;
    int purge_history_on_exit;

    /* Windows & Tabs */
    FfWindow windows[FF_MAX_WINDOWS];
    int window_count;
    int active_window;
    FfTab tabs[FF_MAX_TABS];
    int tab_count;
    int next_tab_id;

    /* Session restore */
    int restore_last_session;
    int restore_on_start;   /* 1 = previous, 2 = home, 3 = blank, 4 = custom urls */
    char startup_urls[16][FF_MAX_URL_LEN];
    int startup_url_count;
    char home_url[FF_MAX_URL_LEN];
    char newtab_url[FF_MAX_URL_LEN];
    char about_blank[64];

    /* Search */
    FfSearchEngine search_engines[16];
    int search_engine_count;
    int default_search_engine;
    int keyword_enabled;
    char search_bar_mode[64];   /* unified / separate */

    /* Tabs behaviour */
    int open_tabs_in_new_window;
    int tabs_load_discarded_on_restore;
    int browser_tabs_animate;
    int browser_tabs_auto_close;
    int tabs_close_confirmation;
    int tab_max_pinned_count;

    /* History */
    FfHistoryItem history[FF_MAX_HISTORY];
    int history_count;
    int history_file_purge_old_days;
    int remember_history;
    int remember_search_form_history;
    int history_sanitize_on_shutdown;
    int sanitize_history_on_shutdown;
    int sanitize_formdata_on_shutdown;
    int sanitize_cookies_on_shutdown;
    int sanitize_cache_on_shutdown;
    int sanitize_sessions_on_shutdown;

    /* Bookmarks */
    FfBookmark *root_bookmark;        /* root */
    FfBookmark *bookmarks_menu;
    FfBookmark *bookmarks_toolbar;
    FfBookmark *bookmarks_unfiled;
    FfBookmark *bookmarks_mobile;
    int bookmark_id_counter;
    char quick_bookmark[FF_MAX_URL_LEN];
    char bookmark_toolbar_visible;
    char sidebar_bookmark_separator;

    /* Cookie jar */
    FfCookie cookies[FF_MAX_COOKIES];
    int cookie_count;
    int accept_cookies;
    int thirdparty_cookie_policy;   /* 0 always,1 never,2 from visited,3 reject trackers */
    int lifetime_policy;            /* 0 forever,1 ask,2 session,3 days-n */
    int lifetime_days;

    /* Cache (HTTP) */
    FfCacheEntry cache[FF_MAX_CACHE_ENTRIES];
    int cache_count;
    int cache_enabled;
    size_t cache_max_bytes;
    int cache_clear_on_exit;

    /* Downloads */
    FfDownload downloads[256];
    int download_count;
    char download_dir[FF_MAX_URL_LEN * 2];
    int download_ask_where;
    int download_open_default_folder_when_done;
    int download_close_when_finished;
    int download_remove_finished_history_min;

    /* Passwords */
    FfLoginInfo saved_logins[1024];
    int saved_login_count;
    int remember_passwords;
    int signon_autofill_forms;
    int master_password_enabled;

    /* Network */
    FfProxySettings proxy;
    char user_agent_override[4096];
    char accept_language[256];
    int dns_over_https;
    char doh_provider[512];
    char doh_uri[FF_MAX_URL_LEN];
    int esni_enabled;
    int ech_enabled;
    int ocsp_stapling_enabled;
    int quic_enabled;
    int http3_enabled;
    int max_http_connections;
    int max_persistent_connections_per_server;

    /* TLS / certificates */
    char tls_min_version[8];
    char tls_max_version[8];
    int ocsp_require_stapling;
    int allow_legacy_tls;
    int enterprise_roots;
    int pkix_smartcard_enabled;
    int sha1_local_allowed;
    int sha1_allowed_for_local_anchors;

    /* Content behaviour */
    int dom_storage_enabled;
    int indexdb_enabled;
    int service_workers_enabled;
    int push_enabled;
    int notifications_enabled;
    int popup_blocker;
    int javascript_enabled;
    int javascript_options;        /* bitmask */
    int popups_allowed_domains[FF_MAX_DOMAINS][256];
    int popup_allowed_count;
    int js_allowed_domains[FF_MAX_DOMAINS][256];
    int js_allowed_count;
    int autoplay_policy;          /* 0=block-audio,1=block-all,2=allow */
    int fullscreen_allowed;
    int pointer_lock_allowed;
    int canvas_image_extraction_blocked;
    int webaudio_enabled;
    int media_hardware_acceleration;

    /* Privacy */
    FfPrivacySettings privacy;
    char tcms_cookie_policy[64];
    int tcms_enabled;

    /* Extensions */
    FfExtension extensions[FF_MAX_EXTENSIONS];
    int extension_count;
    int xpinstall_allowed;
    int extensions_allow_unsigned;
    int extensions_auto_update;

    /* Addons / Search / Components update */
    int app_update_auto;
    int addons_update_auto;
    int search_update_auto;
    char update_channel[64];

    /* Telemetry */
    int telemetry_enabled;
    int crash_reporter_enabled;
    int health_report_enabled;
    int studies_enabled;
    int new_tab_pocket_enabled;
    int new_tab_snippets_enabled;
    int new_tab_top_sites;
    int new_tab_highlights;
    int activity_stream;

    /* Geolocation / Media / Camera / Mic */
    int geo_enabled;
    int geo_wifi_logging;
    char geo_provider_uri[FF_MAX_URL_LEN];
    int media_cubeb_keep_alive;
    char camera_fallback_device[256];
    char mic_fallback_device[256];
    int camera_allowed_by_default;
    int microphone_allowed_by_default;
    int screen_share_enabled;
    int speaker_selection_enabled;

    /* DRM / Media */
    int eme_enabled;
    int widevine_enabled;
    char gmp_path[FF_MAX_URL_LEN * 2];

    /* Session / runtime status */
    int running;
    int exit_code;
    int is_first_run;
    time_t started_at;
    time_t last_profile_save;
    int memory_usage_mb;
    int verbose;
    int debug;

    /* Remote control & command-line URLs */
    char new_tab_urls[32][FF_MAX_URL_LEN];
    int new_tab_url_count;
    int new_window_count_cmdline;

    /* PID list for spawned content processes (minimal, single-process) */
    pid_t pid_main;
    pid_t pid_content[64];
    int pid_content_count;
    int single_process;

    /* GPU / compositor */
    int gpu_process_enabled;
    int compositor_enabled;
    int webrender_enabled;
    int hardware_video_decoding;
    int hardware_video_encoding;
    int d3d11_hardware_decode;
    int vaapi_enabled;
    int vdpau_enabled;

} FirefoxState;

/* Public API */
void firefox_init(FirefoxState *s);
void firefox_cleanup(FirefoxState *s);
int firefox_parse_arguments(FirefoxState *s, int argc, char **argv);

/* Profile */
int firefox_profile_scan(FirefoxState *s);
int firefox_profile_create(FirefoxState *s, const char *name, int make_default);
int firefox_profile_load(FirefoxState *s, int idx);
int firefox_profile_save(FirefoxState *s);
int firefox_purge_private_data(FirefoxState *s, int flags);

/* Window / Tabs */
int firefox_window_new(FirefoxState *s, int *out_id);
int firefox_window_close(FirefoxState *s, int id);
int firefox_tab_new(FirefoxState *s, int window_id, const char *url, int *out_id);
int firefox_tab_close(FirefoxState *s, int tab_id);
int firefox_tab_navigate(FirefoxState *s, int tab_id, const char *url);
int firefox_tab_reload(FirefoxState *s, int tab_id);
int firefox_tab_go_back(FirefoxState *s, int tab_id);
int firefox_tab_go_forward(FirefoxState *s, int tab_id);
int firefox_tab_find_by_id(FirefoxState *s, int id);
int firefox_window_activate_tab(FirefoxState *s, int win_id, int tab_id);
FfTab *firefox_tab_get(FirefoxState *s, int id);
FfWindow *firefox_window_get(FirefoxState *s, int id);

/* Navigation (stub render) */
int firefox_load_url(FirefoxState *s, int tab_id, const char *url);
int firefox_render_minimal(FirefoxState *s, int tab_id);
int firefox_extract_plain_text(FirefoxState *s, int tab_id, char *out, size_t out_size);

/* History */
int firefox_history_add(FirefoxState *s, const char *url, const char *title, const char *referrer, int transition);
int firefox_history_query(FirefoxState *s, const char *needle, int *indices_out, int max_out);
int firefox_history_clear_range(FirefoxState *s, time_t from, time_t to);

/* Bookmarks */
FfBookmark *firefox_bookmark_new(FfBookmarkType type, const char *title, const char *url);
int firefox_bookmark_add_to_folder(FfBookmark *folder, FfBookmark *child);
FfBookmark *firefox_bookmark_find_url(const FfBookmark *root, const char *url);
int firefox_bookmark_remove(FfBookmark *parent, const char *guid);
int firefox_bookmarks_save(const FirefoxState *s);
int firefox_bookmarks_load(FirefoxState *s);

/* Cookies */
int firefox_cookie_add(FirefoxState *s, const FfCookie *cookie);
int firefox_cookie_matches(const FfCookie *c, const char *host, const char *path, int is_secure);
int firefox_cookie_delete_expired(FirefoxState *s);
int firefox_cookie_delete_domain(FirefoxState *s, const char *domain_suffix);

/* Cache */
int firefox_cache_store(FirefoxState *s, const FfCacheEntry *entry);
int firefox_cache_fetch(FirefoxState *s, const char *url, FfCacheEntry *out);
int firefox_cache_evict_lru(FirefoxState *s);
int firefox_cache_clear_all(FirefoxState *s);

/* Downloads */
int firefox_download_start(FirefoxState *s, const FfDownload *dl, int *out_id);
int firefox_download_cancel(FirefoxState *s, int id);
int firefox_download_pause(FirefoxState *s, int id, int pause);
int firefox_download_progress(FirefoxState *s, int id,
                              size_t *out_received, size_t *out_total);

/* Search */
int firefox_search_engine_set_default(FirefoxState *s, const char *name);
int firefox_search_run(FirefoxState *s, const char *engine_alias,
                       const char *query, char out_url[FF_MAX_URL_LEN]);

/* Privacy */
int firefox_apply_privacy_mode(FirefoxState *s, FfPrivacyMode mode);

/* Extensions */
int firefox_extension_install(FirefoxState *s, const char *xpi_or_dir);
int firefox_extension_enable(FirefoxState *s, const char *id, int enable);
int firefox_extension_startup(FirefoxState *s);

/* CLI / runtime */
int firefox_start_headless_dump(FirefoxState *s, const char *url, const char *out_file,
                                int format, int width, int height);
int firefox_run_browser_loop(FirefoxState *s);
void firefox_print_help(void);
void firefox_print_version(void);

#endif
