#include "kapi_curvo_icons.h"
#include <string.h>

static curvo_icon_package_t g_curvo_pkg;

static const char* s_cat_names[CURVO_CAT_COUNT] = {
    "1.用户界面",
    "2.媒体与科技",
    "3.编辑工具",
    "4.形状与数学",
    "5.表情",
    "6.游戏",
    "7.物品",
    "8.自然"
};

static void curvo_register_defaults(void)
{
    curvo_icon_entry_t* e = g_curvo_pkg.entries;
    int i = 0;

    e[i] = (curvo_icon_entry_t){i, "上锁", "lock", CURVO_CAT_UI, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "不可见", "invisible", CURVO_CAT_UI, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "休息", "rest", CURVO_CAT_UI, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "保存", "save", CURVO_CAT_UI, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "信息", "info", CURVO_CAT_UI, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "删除用户", "delete_user", CURVO_CAT_UI, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "刷新", "refresh", CURVO_CAT_UI, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "加载", "loading", CURVO_CAT_UI, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "叉号", "close", CURVO_CAT_UI, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "对勾", "check", CURVO_CAT_UI, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "搜索", "search", CURVO_CAT_UI, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "放大", "zoom_in", CURVO_CAT_UI, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "缩小", "zoom_out", CURVO_CAT_UI, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "用户", "user", CURVO_CAT_UI, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "用户头像", "avatar", CURVO_CAT_UI, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "开关", "toggle", CURVO_CAT_UI, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "开关开启", "toggle_on", CURVO_CAT_UI, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "开关关闭", "toggle_off", CURVO_CAT_UI, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "菜单", "menu", CURVO_CAT_UI, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "网格", "grid", CURVO_CAT_UI, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "深色模式", "dark_mode", CURVO_CAT_UI, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "浅色模式", "light_mode", CURVO_CAT_UI, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "感叹号", "warning", CURVO_CAT_UI, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "滑块", "slider", CURVO_CAT_UI, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "展开", "expand", CURVO_CAT_UI, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "圆圈", "circle", CURVO_CAT_UI, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "可见", "visible", CURVO_CAT_UI, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "设置", "settings", CURVO_CAT_UI, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "禁止", "forbidden", CURVO_CAT_UI, 1,1,1,1}; i++;

    e[i] = (curvo_icon_entry_t){i, "无线网络", "wifi", CURVO_CAT_MEDIA_TECH, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "蓝牙", "bluetooth", CURVO_CAT_MEDIA_TECH, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "音量", "volume", CURVO_CAT_MEDIA_TECH, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "静音", "mute", CURVO_CAT_MEDIA_TECH, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "麦克风", "microphone", CURVO_CAT_MEDIA_TECH, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "摄像头", "camera", CURVO_CAT_MEDIA_TECH, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "显示器", "display", CURVO_CAT_MEDIA_TECH, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "电池正", "battery_full", CURVO_CAT_MEDIA_TECH, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "地球", "globe", CURVO_CAT_MEDIA_TECH, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "下载", "download", CURVO_CAT_MEDIA_TECH, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "上传", "upload", CURVO_CAT_MEDIA_TECH, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "代码", "code", CURVO_CAT_MEDIA_TECH, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "通知", "notification", CURVO_CAT_MEDIA_TECH, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "邮件", "email", CURVO_CAT_MEDIA_TECH, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "消息", "message", CURVO_CAT_MEDIA_TECH, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "日历", "calendar", CURVO_CAT_MEDIA_TECH, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "闹钟", "alarm", CURVO_CAT_MEDIA_TECH, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "时间", "clock", CURVO_CAT_MEDIA_TECH, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "播放", "play", CURVO_CAT_MEDIA_TECH, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "暂停", "pause", CURVO_CAT_MEDIA_TECH, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "音乐", "music", CURVO_CAT_MEDIA_TECH, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "视频", "video", CURVO_CAT_MEDIA_TECH, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "图像", "image", CURVO_CAT_MEDIA_TECH, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "文件夹", "folder", CURVO_CAT_MEDIA_TECH, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "文档", "document", CURVO_CAT_MEDIA_TECH, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "互联网", "internet", CURVO_CAT_MEDIA_TECH, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "手机", "phone", CURVO_CAT_MEDIA_TECH, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "笔记本", "laptop", CURVO_CAT_MEDIA_TECH, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "芯片", "chip", CURVO_CAT_MEDIA_TECH, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "键盘", "keyboard", CURVO_CAT_MEDIA_TECH, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "鼠标", "mouse", CURVO_CAT_MEDIA_TECH, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "闪存盘", "usb_drive", CURVO_CAT_MEDIA_TECH, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "主机", "server", CURVO_CAT_MEDIA_TECH, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "电源开关", "power", CURVO_CAT_MEDIA_TECH, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "连接", "link", CURVO_CAT_MEDIA_TECH, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "分享", "share", CURVO_CAT_MEDIA_TECH, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "书签", "bookmark", CURVO_CAT_MEDIA_TECH, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "位置", "location", CURVO_CAT_MEDIA_TECH, 1,1,1,1}; i++;

    e[i] = (curvo_icon_entry_t){i, "复制", "copy", CURVO_CAT_EDIT, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "撤销", "undo", CURVO_CAT_EDIT, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "重做", "redo", CURVO_CAT_EDIT, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "粗体", "bold", CURVO_CAT_EDIT, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "斜体", "italic", CURVO_CAT_EDIT, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "画笔", "paintbrush", CURVO_CAT_EDIT, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "橡皮", "eraser", CURVO_CAT_EDIT, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "取色器", "eyedropper", CURVO_CAT_EDIT, 1,1,1,1}; i++;

    e[i] = (curvo_icon_entry_t){i, "爱心", "heart", CURVO_CAT_EMOJI, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "星星", "star", CURVO_CAT_EMOJI, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "表情", "smile", CURVO_CAT_EMOJI, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "点赞", "thumbs_up", CURVO_CAT_EMOJI, 1,1,1,1}; i++;

    e[i] = (curvo_icon_entry_t){i, "游戏手柄", "gamepad", CURVO_CAT_GAME, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "奖杯", "trophy", CURVO_CAT_GAME, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "骰子", "dice", CURVO_CAT_GAME, 1,1,1,1}; i++;

    e[i] = (curvo_icon_entry_t){i, "钥匙", "key", CURVO_CAT_OBJECT, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "锤子", "hammer", CURVO_CAT_OBJECT, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "盾", "shield", CURVO_CAT_OBJECT, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "王冠", "crown", CURVO_CAT_OBJECT, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "钻石", "diamond", CURVO_CAT_OBJECT, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "箱子", "chest", CURVO_CAT_OBJECT, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "书", "book", CURVO_CAT_OBJECT, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "地图", "map", CURVO_CAT_OBJECT, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "指南针", "compass", CURVO_CAT_OBJECT, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "灯", "lamp", CURVO_CAT_OBJECT, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "商店", "shop", CURVO_CAT_OBJECT, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "银行", "bank", CURVO_CAT_OBJECT, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "房子", "house", CURVO_CAT_OBJECT, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "火箭", "rocket", CURVO_CAT_OBJECT, 1,1,1,1}; i++;

    e[i] = (curvo_icon_entry_t){i, "太阳", "sun", CURVO_CAT_NATURE, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "月亮", "moon", CURVO_CAT_NATURE, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "云", "cloud", CURVO_CAT_NATURE, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "火", "fire", CURVO_CAT_NATURE, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "水", "water", CURVO_CAT_NATURE, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "树", "tree", CURVO_CAT_NATURE, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "花", "flower", CURVO_CAT_NATURE, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "山", "mountain", CURVO_CAT_NATURE, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "雨", "rain", CURVO_CAT_NATURE, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "雪", "snow", CURVO_CAT_NATURE, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "雷电", "lightning", CURVO_CAT_NATURE, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "风", "wind", CURVO_CAT_NATURE, 1,1,1,1}; i++;
    e[i] = (curvo_icon_entry_t){i, "温度", "temperature", CURVO_CAT_NATURE, 1,1,1,1}; i++;

    while (i < CURVO_ICON_TOTAL) {
        e[i] = (curvo_icon_entry_t){i, "", "", CURVO_CAT_UI, 0,0,0,0};
        i++;
    }
}

int kapi_curvo_init(const char* icon_base_path)
{
    memset(&g_curvo_pkg, 0, sizeof(curvo_icon_package_t));
    g_curvo_pkg.base_path = icon_base_path;
    g_curvo_pkg.cache_count = 0;
    curvo_register_defaults();
    g_curvo_pkg.initialized = true;
    return 0;
}

void kapi_curvo_shutdown(void)
{
    for (uint32_t i = 0; i < g_curvo_pkg.cache_count; i++) {
        if (g_curvo_pkg.cache[i].pixels) {
            g_curvo_pkg.cache[i].pixels = NULL;
        }
    }
    g_curvo_pkg.initialized = false;
}

const curvo_icon_entry_t* kapi_curvo_lookup_by_name(const char* name_cn)
{
    if (!name_cn || !g_curvo_pkg.initialized) return NULL;
    for (int i = 0; i < CURVO_ICON_TOTAL; i++) {
        if (g_curvo_pkg.entries[i].name_cn[0] && strcmp(g_curvo_pkg.entries[i].name_cn, name_cn) == 0)
            return &g_curvo_pkg.entries[i];
    }
    return NULL;
}

const curvo_icon_entry_t* kapi_curvo_lookup_by_id(uint32_t id)
{
    if (!g_curvo_pkg.initialized || id >= CURVO_ICON_TOTAL) return NULL;
    if (g_curvo_pkg.entries[id].name_cn[0]) return &g_curvo_pkg.entries[id];
    return NULL;
}

const curvo_icon_entry_t* kapi_curvo_get_all(uint32_t* count)
{
    if (count) *count = CURVO_ICON_TOTAL;
    return g_curvo_pkg.entries;
}

const uint32_t* kapi_curvo_load_bitmap(uint32_t icon_id, uint32_t size, uint32_t* out_w, uint32_t* out_h)
{
    if (!g_curvo_pkg.initialized || icon_id >= CURVO_ICON_TOTAL) return NULL;
    if (out_w) *out_w = size;
    if (out_h) *out_h = size;
    (void)size;
    return NULL;
}

void kapi_curvo_release_bitmap(uint32_t icon_id)
{
    (void)icon_id;
}

void kapi_curvo_draw_icon(uint32_t* fb, int stride, int fb_w, int fb_h,
                           int x, int y, uint32_t icon_id, uint32_t size,
                           uint32_t tint_color, uint8_t alpha)
{
    if (!fb || !g_curvo_pkg.initialized || icon_id >= CURVO_ICON_TOTAL) return;
    (void)stride; (void)fb_w; (void)fb_h; (void)x; (void)y;
    (void)size; (void)tint_color; (void)alpha;
}

void kapi_curvo_draw_icon_scaled(uint32_t* fb, int stride, int fb_w, int fb_h,
                                  int dst_x, int dst_y, int dst_w, int dst_h,
                                  uint32_t icon_id, uint32_t tint_color, uint8_t alpha)
{
    if (!fb || !g_curvo_pkg.initialized || icon_id >= CURVO_ICON_TOTAL) return;
    (void)stride; (void)fb_w; (void)fb_h;
    (void)dst_x; (void)dst_y; (void)dst_w; (void)dst_h;
    (void)tint_color; (void)alpha;
}

uint32_t kapi_curvo_count_by_category(uint32_t category)
{
    if (category >= CURVO_CAT_COUNT) return 0;
    uint32_t count = 0;
    for (int i = 0; i < CURVO_ICON_TOTAL; i++) {
        if (g_curvo_pkg.entries[i].category == category && g_curvo_pkg.entries[i].name_cn[0])
            count++;
    }
    return count;
}

const curvo_icon_entry_t* kapi_curvo_get_by_category(uint32_t category, uint32_t* count)
{
    (void)category;
    if (count) *count = 0;
    return NULL;
}

const char* kapi_curvo_category_name(uint32_t category)
{
    if (category >= CURVO_CAT_COUNT) return "Unknown";
    return s_cat_names[category];
}