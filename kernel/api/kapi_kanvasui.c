#include "kapi_kanvasui.h"
#include "kapi.h"
#include <string.h>

static kui_context_t kui_ctx;

int kui_init(int screen_w, int screen_h, int bpp, uint32_t* fb, int stride)
{
    memset(&kui_ctx, 0, sizeof(kui_context_t));
    kui_ctx.screen_width = screen_w;
    kui_ctx.screen_height = screen_h;
    kui_ctx.bpp = bpp;
    kui_ctx.framebuffer = fb;
    kui_ctx.stride = stride;
    kui_ctx.initialized = 1;
    kui_ctx.next_component_id = 1;
    kui_ctx.next_window_id = 1;
    kui_ctx.theme.background = (kui_color_t){30,30,30,255};
    kui_ctx.theme.foreground = (kui_color_t){240,240,240,255};
    kui_ctx.theme.accent = (kui_color_t){98,0,238,255};
    kui_ctx.theme.text_primary = (kui_color_t){240,240,240,255};
    kui_ctx.theme.text_secondary = (kui_color_t){180,180,180,255};
    kui_ctx.theme.border = (kui_color_t){80,80,80,255};
    kui_ctx.theme.corner_radius = 4;
    kui_ctx.theme.font_size = 14;
    kui_ctx.theme.padding = 8;
    kui_ctx.theme.spacing = 4;
    return 0;
}

void kui_shutdown(void)
{
    kui_window_t* w = kui_ctx.window_list;
    while (w) {
        kui_window_t* next = w->next;
        kui_destroy_window(w);
        w = next;
    }
    memset(&kui_ctx, 0, sizeof(kui_context_t));
}

const kui_theme_t* kui_get_theme(void) { return &kui_ctx.theme; }

void kui_set_theme(const kui_theme_t* theme)
{
    if (theme) kui_ctx.theme = *theme;
}

void kui_apply_theme_recursive(kui_window_t* win)
{
    (void)win;
}

kui_window_t* kui_create_window(const char* title, int x, int y, int w, int h,
                                   bool resizable, bool decorated)
{
    kui_window_t* win = (kui_window_t*)kapi_kmalloc(sizeof(kui_window_t));
    if (!win) return NULL;
    memset(win, 0, sizeof(kui_window_t));
    win->id = kui_ctx.next_window_id++;
    if (title) { strncpy(win->title, title, sizeof(win->title)-1); win->title[sizeof(win->title)-1]='\0'; }
    win->window_rect = (kui_rect_t){x, y, w, h};
    int bw = decorated ? 2 : 0;
    int tbh = decorated ? 24 : 0;
    win->client_rect = (kui_rect_t){x+bw, y+tbh, w-2*bw, h-tbh-bw};
    win->visible = 1;
    win->resizable = resizable;
    win->decorated = decorated;
    win->title_bar_color = kui_ctx.theme.background;
    win->client_area_color = kui_ctx.theme.background;
    win->title_bar_height = tbh;
    win->border_width = bw;
    win->buffer_stride = w * 4;
    size_t buf_sz = (size_t)w * h * 4;
    win->back_buffer = (uint32_t*)kapi_kmalloc(buf_sz);
    win->front_buffer = (uint32_t*)kapi_kmalloc(buf_sz);
    if (!win->back_buffer || !win->front_buffer) {
        if (win->back_buffer) kapi_kfree(win->back_buffer);
        if (win->front_buffer) kapi_kfree(win->front_buffer);
        kapi_kfree(win);
        return NULL;
    }
    memset(win->back_buffer, 0, buf_sz);
    memset(win->front_buffer, 0, buf_sz);
    if (!kui_ctx.window_list) {
        kui_ctx.window_list = win;
    } else {
        kui_window_t* tail = kui_ctx.window_list;
        while (tail->next) tail = tail->next;
        tail->next = win;
        win->prev = tail;
    }
    return win;
}

void kui_destroy_window(kui_window_t* win)
{
    if (!win) return;
    kui_component_t* comp = win->components_head;
    while (comp) {
        kui_component_t* next = comp->next;
        kui_destroy_component(comp);
        comp = next;
    }
    if (win->back_buffer) kapi_kfree(win->back_buffer);
    if (win->front_buffer) kapi_kfree(win->front_buffer);
    if (win->prev) win->prev->next = win->next;
    else kui_ctx.window_list = win->next;
    if (win->next) win->next->prev = win->prev;
    if (kui_ctx.focused_window == win) kui_ctx.focused_window = NULL;
    kapi_kfree(win);
}

void kui_show_window(kui_window_t* win) { if(win) win->visible=1; }
void kui_hide_window(kui_window_t* win) { if(win) win->visible=0; }
void kui_focus_window(kui_window_t* win)
{
    if(!win) return;
    if(kui_ctx.focused_window) kui_ctx.focused_window->focused=0;
    kui_ctx.focused_window = win;
    win->focused = 1;
}
void kui_minimize_window(kui_window_t* win) { if(win) win->minimized=1; }
void kui_maximize_window(kui_window_t* win) { if(win) win->maximized=1; }
void kui_restore_window(kui_window_t* win) { if(win){win->minimized=0;win->maximized=0;} }
void kui_set_window_title(kui_window_t* win, const char* title)
{
    if(!win||!title) return;
    strncpy(win->title,title,sizeof(win->title)-1); win->title[sizeof(win->title)-1]='\0';
}
void kui_set_window_rect(kui_window_t* win, int x, int y, int w, int h)
{
    if(!win) return;
    win->window_rect = (kui_rect_t){x,y,w,h};
    int bw=win->border_width, tbh=win->title_bar_height;
    win->client_rect = (kui_rect_t){x+bw,y+tbh,w-2*bw,h-tbh-bw};
}
bool kui_is_window_visible(kui_window_t* win) { return win?win->visible:0; }
bool kui_is_window_focused(kui_window_t* win) { return win?win->focused:0; }

kui_component_t* kui_add_component(kui_window_t* win, kui_component_t* comp)
{
    if(!win||!comp) return NULL;
    comp->parent_window = win;
    comp->id = kui_ctx.next_component_id++;
    if(!win->components_head) {
        win->components_head = comp;
        win->components_tail = comp;
    } else {
        win->components_tail->next = comp;
        comp->prev = win->components_tail;
        win->components_tail = comp;
    }
    win->component_count++;
    return comp;
}

void kui_remove_component(kui_window_t* win, kui_component_t* comp)
{
    if(!win||!comp) return;
    if(comp->prev) comp->prev->next = comp->next;
    else win->components_head = comp->next;
    if(comp->next) comp->next->prev = comp->prev;
    else win->components_tail = comp->prev;
    win->component_count--;
    comp->prev = NULL; comp->next = NULL;
}

void kui_destroy_component(kui_component_t* comp)
{
    if(!comp) return;
    kapi_kfree(comp);
}

kui_component_t* kui_find_component_by_id(uint32_t id)
{
    kui_window_t* w = kui_ctx.window_list;
    while(w) {
        kui_component_t* c = w->components_head;
        while(c) { if(c->id==id) return c; c=c->next; }
        w = w->next;
    }
    return NULL;
}

static kui_component_t* alloc_comp(size_t sz)
{
    kui_component_t* c = (kui_component_t*)kapi_kmalloc(sz);
    if(c) memset(c,0,sz);
    return c;
}

kui_button_t* kui_create_button(const char* text, kui_rect_t bounds, void (*on_click)(kui_button_t*))
{
    kui_button_t* b = (kui_button_t*)alloc_comp(sizeof(kui_button_t));
    if(!b) return NULL;
    b->base.bounds = bounds; b->base.visible = 1; b->base.enabled = 1;
    b->base.accepts_focus = 1; b->text = text;
    b->bg_color = kui_ctx.theme.accent;
    b->fg_color = kui_ctx.theme.text_primary;
    b->hover_color = (kui_color_t){120,120,120,255};
    b->pressed_color = (kui_color_t){80,80,80,255};
    b->on_click = on_click;
    return b;
}

kui_label_t* kui_create_label(const char* text, kui_rect_t bounds, int alignment)
{
    kui_label_t* l = (kui_label_t*)alloc_comp(sizeof(kui_label_t));
    if(!l) return NULL;
    l->base.bounds = bounds; l->base.visible = 1;
    l->text = (char*)text; l->alignment = alignment;
    l->text_color = kui_ctx.theme.text_primary;
    return l;
}

kui_textbox_t* kui_create_textbox(kui_rect_t bounds, int max_chars, void (*on_changed)(kui_textbox_t*))
{
    kui_textbox_t* t = (kui_textbox_t*)alloc_comp(sizeof(kui_textbox_t));
    if(!t) return NULL;
    t->base.bounds = bounds; t->base.visible = 1; t->base.enabled = 1;
    t->base.accepts_focus = 1; t->max_chars = max_chars;
    t->text_capacity = max_chars > 0 ? (size_t)max_chars + 1 : 256;
    t->text = (char*)kapi_kmalloc(t->text_capacity);
    if(t->text) t->text[0] = '\0';
    t->on_text_changed = on_changed;
    return t;
}

kui_progressbar_t* kui_create_progressbar(kui_rect_t bounds, int min_val, int max_val)
{
    kui_progressbar_t* p = (kui_progressbar_t*)alloc_comp(sizeof(kui_progressbar_t));
    if(!p) return NULL;
    p->base.bounds = bounds; p->base.visible = 1;
    p->min_value = min_val; p->max_value = max_val;
    p->bar_color = kui_ctx.theme.accent;
    p->background_color = kui_ctx.theme.border;
    return p;
}

kui_slider_t* kui_create_slider(kui_rect_t bounds, int min_val, int max_val, void (*on_changed)(kui_slider_t*,int))
{
    kui_slider_t* s = (kui_slider_t*)alloc_comp(sizeof(kui_slider_t));
    if(!s) return NULL;
    s->base.bounds = bounds; s->base.visible = 1; s->base.enabled = 1;
    s->base.accepts_focus = 1;
    s->min_value = min_val; s->max_value = max_val;
    s->track_color = kui_ctx.theme.border;
    s->thumb_color = kui_ctx.theme.accent;
    s->on_value_changed = on_changed;
    return s;
}

kui_checkbox_t* kui_create_checkbox(const char* label, kui_rect_t bounds, void (*on_changed)(kui_checkbox_t*,bool))
{
    kui_checkbox_t* cb = (kui_checkbox_t*)alloc_comp(sizeof(kui_checkbox_t));
    if(!cb) return NULL;
    cb->base.bounds = bounds; cb->base.visible = 1; cb->base.enabled = 1;
    cb->base.accepts_focus = 1; cb->label = label;
    cb->check_color = kui_ctx.theme.accent;
    cb->on_state_changed = on_changed;
    return cb;
}

kui_listbox_t* kui_create_listbox(kui_rect_t bounds, int visible_items, void (*on_sel)(kui_listbox_t*,int))
{
    kui_listbox_t* lb = (kui_listbox_t*)alloc_comp(sizeof(kui_listbox_t));
    if(!lb) return NULL;
    lb->base.bounds = bounds; lb->base.visible = 1; lb->base.enabled = 1;
    lb->base.accepts_focus = 1; lb->visible_items = visible_items;
    lb->item_height = 24; lb->capacity = 32;
    lb->items = (kui_listbox_item_t*)kapi_kmalloc(sizeof(kui_listbox_item_t)*lb->capacity);
    lb->item_bg_color = kui_ctx.theme.background;
    lb->item_selected_color = kui_ctx.theme.accent;
    lb->item_text_color = kui_ctx.theme.text_primary;
    lb->on_selection_changed = on_sel;
    return lb;
}

kui_panel_t* kui_create_panel(kui_rect_t bounds, kui_color_t bg)
{
    kui_panel_t* p = (kui_panel_t*)alloc_comp(sizeof(kui_panel_t));
    if(!p) return NULL;
    p->base.bounds = bounds; p->base.visible = 1;
    p->capacity = 16;
    p->children = (kui_component_t**)kapi_kmalloc(sizeof(kui_component_t*)*p->capacity);
    p->background_color = bg;
    p->corner_radius = kui_ctx.theme.corner_radius;
    return p;
}

kui_image_t* kui_create_image(kui_rect_t bounds, uint32_t* pixels, int img_w, int img_h)
{
    kui_image_t* im = (kui_image_t*)alloc_comp(sizeof(kui_image_t));
    if(!im) return NULL;
    im->base.bounds = bounds; im->base.visible = 1;
    im->pixel_data = pixels; im->image_width = img_w; im->image_height = img_h;
    return im;
}

kui_menubar_t* kui_create_menubar(kui_rect_t bounds)
{
    kui_menubar_t* mb = (kui_menubar_t*)alloc_comp(sizeof(kui_menubar_t));
    if(!mb) return NULL;
    mb->base.bounds = bounds; mb->base.visible = 1;
    mb->bg_color = kui_ctx.theme.background;
    mb->text_color = kui_ctx.theme.text_primary;
    return mb;
}

kui_statusbar_t* kui_create_statusbar(kui_rect_t bounds)
{
    kui_statusbar_t* sb = (kui_statusbar_t*)alloc_comp(sizeof(kui_statusbar_t));
    if(!sb) return NULL;
    sb->base.bounds = bounds; sb->base.visible = 1;
    sb->bg_color = kui_ctx.theme.background;
    sb->text_color = kui_ctx.theme.text_secondary;
    return sb;
}

kui_toolbar_t* kui_create_toolbar(kui_rect_t bounds, bool vertical)
{
    kui_toolbar_t* tb = (kui_toolbar_t*)alloc_comp(sizeof(kui_toolbar_t));
    if(!tb) return NULL;
    tb->base.bounds = bounds; tb->base.visible = 1;
    tb->vertical = vertical; tb->button_size = 32; tb->spacing = 2;
    tb->bg_color = kui_ctx.theme.background;
    tb->button_color = kui_ctx.theme.foreground;
    return tb;
}

kui_tabcontrol_t* kui_create_tabcontrol(kui_rect_t bounds)
{
    kui_tabcontrol_t* tc = (kui_tabcontrol_t*)alloc_comp(sizeof(kui_tabcontrol_t));
    if(!tc) return NULL;
    tc->base.bounds = bounds; tc->base.visible = 1;
    tc->tab_bg_color = kui_ctx.theme.background;
    tc->tab_active_color = kui_ctx.theme.accent;
    return tc;
}

kui_treeview_t* kui_create_treeview(kui_rect_t bounds)
{
    kui_treeview_t* tv = (kui_treeview_t*)alloc_comp(sizeof(kui_treeview_t));
    if(!tv) return NULL;
    tv->base.bounds = bounds; tv->base.visible = 1;
    tv->indent_size = 20;
    tv->node_text_color = kui_ctx.theme.text_primary;
    return tv;
}

kui_splitter_t* kui_create_splitter(kui_rect_t bounds, bool horizontal)
{
    kui_splitter_t* sp = (kui_splitter_t*)alloc_comp(sizeof(kui_splitter_t));
    if(!sp) return NULL;
    sp->base.bounds = bounds; sp->base.visible = 1;
    sp->horizontal = horizontal; sp->splitter_size = 4;
    sp->splitter_color = kui_ctx.theme.border;
    return sp;
}

kui_tooltip_t* kui_create_tooltip(const char* text, kui_window_t* owner, int timeout_ms)
{
    kui_tooltip_t* tt = (kui_tooltip_t*)alloc_comp(sizeof(kui_tooltip_t));
    if(!tt) return NULL;
    tt->base.visible = 0; tt->text = text; tt->owner = owner; tt->timeout_ms = timeout_ms;
    return tt;
}

static inline uint32_t kui_col32(kui_color_t c)
{
    return ((uint32_t)c.a<<24)|((uint32_t)c.r<<16)|((uint32_t)c.g<<8)|(uint32_t)c.b;
}

void kui_draw_rect(uint32_t* fb, int stride, int fw, int fh, int x, int y, int w, int h, uint32_t c)
{
    (void)fh;
    if(y>=0&&y<fh) for(int i=x;i<x+w&&i<fw;i++) if(i>=0) ((uint32_t*)((uint8_t*)fb+y*stride))[i]=c;
    if(y+h-1>=0&&y+h-1<fh) for(int i=x;i<x+w&&i<fw;i++) if(i>=0) ((uint32_t*)((uint8_t*)fb+(y+h-1)*stride))[i]=c;
    for(int j=y;j<y+h&&j<fh;j++) {
        if(j>=0) {
            if(x>=0&&x<fw) ((uint32_t*)((uint8_t*)fb+j*stride))[x]=c;
            if(x+w-1>=0&&x+w-1<fw) ((uint32_t*)((uint8_t*)fb+j*stride))[x+w-1]=c;
        }
    }
}

void kui_draw_filled_rect(uint32_t* fb, int stride, int fw, int fh, int x, int y, int w, int h, uint32_t c)
{
    for(int j=y;j<y+h&&j<fh;j++)
        if(j>=0) for(int i=x;i<x+w&&i<fw;i++)
            if(i>=0) ((uint32_t*)((uint8_t*)fb+j*stride))[i]=c;
}

void kui_draw_rounded_rect(uint32_t* fb, int stride, int fw, int fh, int x, int y, int w, int h, int r, uint32_t c)
{
    kui_draw_filled_rect(fb,stride,fw,fh,x+r,y,w-2*r,h,c);
    kui_draw_filled_rect(fb,stride,fw,fh,x,y+r,r,h-2*r,c);
    kui_draw_filled_rect(fb,stride,fw,fh,x+w-r,y+r,r,h-2*r,c);
    for(int dy=0;dy<=r;dy++){
        int dx=(int)kapi_sqrtf((float)(r*r-dy*dy));
        for(int i=-dx;i<=dx;i++){
            int px,py;
            py=y+r-dy; px=x+r+i; if(px>=0&&px<fw&&py>=0&&py<fh) ((uint32_t*)((uint8_t*)fb+py*stride))[px]=c;
            px=x+w-r+i; if(px>=0&&px<fw&&py>=0&&py<fh) ((uint32_t*)((uint8_t*)fb+py*stride))[px]=c;
            py=y+h-r+dy; px=x+r+i; if(px>=0&&px<fw&&py>=0&&py<fh) ((uint32_t*)((uint8_t*)fb+py*stride))[px]=c;
            px=x+w-r+i; if(px>=0&&px<fw&&py>=0&&py<fh) ((uint32_t*)((uint8_t*)fb+py*stride))[px]=c;
        }
    }
}

void kui_draw_rounded_rect_outline(uint32_t* fb, int stride, int fw, int fh, int x, int y, int w, int h, int r, uint32_t c)
{
    (void)r;
    kui_draw_rect(fb,stride,fw,fh,x,y,w,h,c);
}

void kui_draw_line(uint32_t* fb, int stride, int fw, int fh, int x0, int y0, int x1, int y1, uint32_t c)
{
    int dx=x1-x0; if(dx<0) dx=-dx;
    int dy=y1-y0; if(dy<0) dy=-dy;
    int sx=x0<x1?1:-1, sy=y0<y1?1:-1, err=dx-dy;
    while(1){
        if(x0>=0&&x0<fw&&y0>=0&&y0<fh) ((uint32_t*)((uint8_t*)fb+y0*stride))[x0]=c;
        if(x0==x1&&y0==y1) break;
        int e2=2*err; if(e2>-dy){err-=dy;x0+=sx;} if(e2<dx){err+=dx;y0+=sy;}
    }
}

void kui_draw_circle(uint32_t* fb, int stride, int fw, int fh, int cx, int cy, int r, uint32_t c, bool filled)
{
    for(int y=-r;y<=r;y++) for(int x=-r;x<=r;x++){
        int d=x*x+y*y;
        if(filled ? d<=r*r : (d>=(r-1)*(r-1)&&d<=r*r)) {
            int px=cx+x,py=cy+y;
            if(px>=0&&px<fw&&py>=0&&py<fh) ((uint32_t*)((uint8_t*)fb+py*stride))[px]=c;
        }
    }
}

void kui_draw_ellipse(uint32_t* fb, int stride, int fw, int fh, int cx, int cy, int rx, int ry, uint32_t c, bool filled)
{
    if(rx<=0||ry<=0) return;
    for(int y=-ry;y<=ry;y++) for(int x=-rx;x<=rx;x++){
        long long dx=(long long)x*ry, dy=(long long)y*rx;
        long long r2=(long long)rx*ry;
        if(filled ? dx*dx+dy*dy<=r2*r2 : dx*dx+dy*dy>=((r2-1)*(r2-1))&&dx*dx+dy*dy<=r2*r2) {
            int px=cx+x,py=cy+y;
            if(px>=0&&px<fw&&py>=0&&py<fh) ((uint32_t*)((uint8_t*)fb+py*stride))[px]=c;
        }
    }
}

void kui_draw_text(uint32_t* fb, int stride, int fw, int fh, int x, int y, const char* text, uint32_t c, int fs, int bold)
{
    (void)bold;
    if(!text) return;
    int cw=(int)(fs*0.6f), ch=fs;
    size_t len=kapi_strlen(text);
    int ix=x;
    for(size_t i=0;i<len;i++){
        kui_draw_filled_rect(fb,stride,fw,fh,ix+1,y+2,cw-2,ch-4,c);
        ix+=cw;
    }
}

void kui_draw_text_clipped(uint32_t* fb, int stride, int fw, int fh, int x, int y, const char* text, uint32_t c, int fs, int bold, int cx, int cy, int cw, int ch)
{
    (void)cx;(void)cy;(void)cw;(void)ch;
    kui_draw_text(fb,stride,fw,fh,x,y,text,c,fs,bold);
}

void kui_draw_icon(uint32_t* fb, int stride, int fw, int fh, int x, int y, int icon_id, int size, uint32_t c)
{
    (void)icon_id;
    kui_draw_filled_rect(fb,stride,fw,fh,x,y,size,size,c);
}

void kui_draw_checkmark(uint32_t* fb, int stride, int fw, int fh, int x, int y, int size, uint32_t c)
{
    kui_draw_line(fb,stride,fw,fh,x,y+size/2,x+size/3,y+size,c);
    kui_draw_line(fb,stride,fw,fh,x+size/3,y+size,x+size,y,c);
}

void kui_draw_shadow(uint32_t* fb, int stride, int fw, int fh, int x, int y, int w, int h, int ox, int oy, int blur, uint8_t a)
{
    (void)blur;
    kui_draw_alpha_rect(fb,stride,fw,fh,x+ox,y+oy,w,h,0,a);
}

void kui_draw_gradient_v(uint32_t* fb, int stride, int fw, int fh, int x, int y, int w, int h, uint32_t ct, uint32_t cb)
{
    for(int row=0;row<h;row++){
        float t=h>1?(float)row/(h-1):0.0f;
        uint8_t r=(uint8_t)(((ct>>16)&0xFF)*(1-t)+((cb>>16)&0xFF)*t);
        uint8_t g=(uint8_t)(((ct>>8)&0xFF)*(1-t)+((cb>>8)&0xFF)*t);
        uint8_t b=(uint8_t)((ct&0xFF)*(1-t)+(cb&0xFF)*t);
        uint32_t c=0xFF000000|(r<<16)|(g<<8)|b;
        for(int col=0;col<w;col++){
            int px=x+col,py=y+row;
            if(px>=0&&px<fw&&py>=0&&py<fh) ((uint32_t*)((uint8_t*)fb+py*stride))[px]=c;
        }
    }
}

void kui_draw_gradient_h(uint32_t* fb, int stride, int fw, int fh, int x, int y, int w, int h, uint32_t cl, uint32_t cr)
{
    for(int col=0;col<w;col++){
        float t=w>1?(float)col/(w-1):0.0f;
        uint8_t r=(uint8_t)(((cl>>16)&0xFF)*(1-t)+((cr>>16)&0xFF)*t);
        uint8_t g=(uint8_t)(((cl>>8)&0xFF)*(1-t)+((cr>>8)&0xFF)*t);
        uint8_t b=(uint8_t)((cl&0xFF)*(1-t)+(cr&0xFF)*t);
        uint32_t c=0xFF000000|(r<<16)|(g<<8)|b;
        for(int row=0;row<h;row++){
            int px=x+col,py=y+row;
            if(px>=0&&px<fw&&py>=0&&py<fh) ((uint32_t*)((uint8_t*)fb+py*stride))[px]=c;
        }
    }
}

void kui_draw_alpha_rect(uint32_t* fb, int stride, int fw, int fh, int x, int y, int w, int h, uint32_t c, uint8_t a)
{
    for(int j=y;j<y+h&&j<fh;j++)
        if(j>=0) for(int i=x;i<x+w&&i<fw;i++)
            if(i>=0) {
                uint32_t* p = &((uint32_t*)((uint8_t*)fb+j*stride))[i];
                uint32_t d=*p;
                uint32_t ia=255-a;
                uint8_t dr=(d>>16)&0xFF,dg=(d>>8)&0xFF,db=d&0xFF;
                uint8_t sr=(c>>16)&0xFF,sg=(c>>8)&0xFF,sb=c&0xFF;
                *p=0xFF000000|(((sr*a+dr*ia)/255)<<16)|(((sg*a+dg*ia)/255)<<8)|((sb*a+db*ia)/255);
            }
}

void kui_draw_image(uint32_t* fb, int stride, int fw, int fh, int x, int y, uint32_t* img, int iw, int ih, int is)
{
    for(int j=0;j<ih;j++){
        int dy=y+j; if(dy<0||dy>=fh) continue;
        for(int i=0;i<iw;i++){
            int dx=x+i; if(dx<0||dx>=fw) continue;
            ((uint32_t*)((uint8_t*)fb+dy*stride))[dx]=((uint32_t*)((uint8_t*)img+j*is))[i];
        }
    }
}

void kui_draw_image_scaled(uint32_t* fb, int stride, int fw, int fh, int x, int y, int w, int h, uint32_t* img, int iw, int ih, int is)
{
    for(int j=0;j<h;j++){
        int dy=y+j; if(dy<0||dy>=fh) continue;
        int sy=j*ih/h;
        for(int i=0;i<w;i++){
            int dx=x+i; if(dx<0||dx>=fw) continue;
            int sx=i*iw/w;
            ((uint32_t*)((uint8_t*)fb+dy*stride))[dx]=((uint32_t*)((uint8_t*)img+sy*is))[sx];
        }
    }
}

void kui_blit(uint32_t* dst, int ds, int dx, int dy, uint32_t* src, int ss, int sx, int sy, int w, int h)
{
    for(int j=0;j<h;j++){
        int ddy=dy+j, ssy=sy+j;
        for(int i=0;i<w;i++){
            int ddx=dx+i, ssx=sx+i;
            ((uint32_t*)((uint8_t*)dst+ddy*ds))[ddx]=((uint32_t*)((uint8_t*)src+ssy*ss))[ssx];
        }
    }
}

void kui_blit_alpha(uint32_t* dst, int ds, int dx, int dy, uint32_t* src, int ss, int sx, int sy, int w, int h, uint8_t a)
{
    for(int j=0;j<h;j++){
        int ddy=dy+j, ssy=sy+j;
        for(int i=0;i<w;i++){
            int ddx=dx+i, ssx=sx+i;
            uint32_t* dp=&((uint32_t*)((uint8_t*)dst+ddy*ds))[ddx];
            uint32_t sv=((uint32_t*)((uint8_t*)src+ssy*ss))[ssx];
            uint32_t dv=*dp;
            uint32_t ia=255-a;
            uint8_t dr=(dv>>16)&0xFF,dg=(dv>>8)&0xFF,db=dv&0xFF;
            uint8_t sr=(sv>>16)&0xFF,sg=(sv>>8)&0xFF,sb=sv&0xFF;
            *dp=0xFF000000|(((sr*a+dr*ia)/255)<<16)|(((sg*a+dg*ia)/255)<<8)|((sb*a+db*ia)/255);
        }
    }
}

bool kui_point_in_rect(int x, int y, kui_rect_t r)
{
    return x>=r.x && x<r.x+r.width && y>=r.y && y<r.y+r.height;
}

bool kui_rect_intersect(kui_rect_t a, kui_rect_t b)
{
    return a.x<b.x+b.width && a.x+a.width>b.x && a.y<b.y+b.height && a.y+a.height>b.y;
}

kui_rect_t kui_rect_union(kui_rect_t a, kui_rect_t b)
{
    int x1=a.x<a.y?a.x:b.x, y1=a.y<b.y?a.y:b.y;
    int x2=a.x+a.width>b.x+b.width?a.x+a.width:b.x+b.width;
    int y2=a.y+a.height>b.y+b.height?a.y+a.height:b.y+b.height;
    kui_rect_t r; r.x=x1; r.y=y1; r.width=x2-x1; r.height=y2-y1; return r;
}

kui_rect_t kui_rect_clip(kui_rect_t a, kui_rect_t b)
{
    int x1=a.x>b.x?a.x:b.x, y1=a.y>b.y?a.y:b.y;
    int x2=a.x+a.width<b.x+b.width?a.x+a.width:b.x+b.width;
    int y2=a.y+a.height<b.y+b.height?a.y+a.height:b.y+b.height;
    kui_rect_t r; r.x=x1; r.y=y1; r.width=x2>x1?x2-x1:0; r.height=y2>y1?y2-y1:0; return r;
}

void kui_invalidate(kui_component_t* comp) { if(comp) comp->needs_redraw=1; }
void kui_invalidate_rect(kui_window_t* win, kui_rect_t rect)
{
    if(!win||win->dirty_count>=KANVASUI_MAX_DIRTY_RECTS) return;
    win->dirty_rects[win->dirty_count++] = rect;
}
void kui_invalidate_all(kui_window_t* win)
{
    if(!win) return;
    win->dirty_count = 1;
    win->dirty_rects[0] = win->client_rect;
}

void kui_process_mouse_move(int x, int y)
{
    kui_ctx.mouse_pos = (kui_point_t){x,y};
    kui_window_t* w = kui_ctx.window_list;
    while(w) {
        if(w->visible && w->message_handler)
            w->message_handler(w, KUI_MSG_MOUSE_MOVE, (uint64_t)x, (uint64_t)y);
        w = w->next;
    }
}

void kui_process_mouse_button(int button, bool down, int x, int y)
{
    kui_msg_type_t msg = down ?
        (button==0 ? KUI_MSG_LBUTTON_DOWN : button==1 ? KUI_MSG_MBUTTON_DOWN : KUI_MSG_RBUTTON_DOWN) :
        (button==0 ? KUI_MSG_LBUTTON_UP : button==1 ? KUI_MSG_MBUTTON_UP : KUI_MSG_RBUTTON_UP);
    kui_window_t* w = kui_ctx.window_list;
    while(w) {
        if(w->visible && w->message_handler)
            w->message_handler(w, msg, (uint64_t)x, (uint64_t)y);
        w = w->next;
    }
}

void kui_process_scroll(int delta, int x, int y)
{
    kui_msg_type_t msg = delta > 0 ? KUI_MSG_SCROLL_UP : KUI_MSG_SCROLL_DOWN;
    kui_window_t* w = kui_ctx.window_list;
    while(w) {
        if(w->visible && w->message_handler)
            w->message_handler(w, msg, (uint64_t)x, (uint64_t)y);
        w = w->next;
    }
}

void kui_process_key(int key, bool down, uint32_t modifiers)
{
    (void)modifiers;
    kui_msg_type_t msg = down ? KUI_MSG_KEY_DOWN : KUI_MSG_KEY_UP;
    kui_window_t* w = kui_ctx.focused_window;
    if(w && w->visible && w->message_handler)
        w->message_handler(w, msg, (uint64_t)key, 0);
}

void kui_process_char(uint32_t ch)
{
    kui_window_t* w = kui_ctx.focused_window;
    if(w && w->visible && w->message_handler)
        w->message_handler(w, KUI_MSG_CHAR_INPUT, (uint64_t)ch, 0);
}

void kui_render(void)
{
    kui_window_t* w = kui_ctx.window_list;
    while(w) {
        if(w->visible) kui_render_window(w);
        w = w->next;
    }
}

void kui_render_window(kui_window_t* win)
{
    if(!win||!win->back_buffer) return;
    int fw=win->window_rect.width, fh=win->window_rect.height;
    uint32_t* fb=win->back_buffer;
    int stride=win->buffer_stride;
    uint32_t bg=kui_col32(win->client_area_color);
    kui_draw_filled_rect(fb,stride,fw,fh,0,0,fw,fh,bg);
    if(win->decorated) {
        uint32_t tb=kui_col32(win->title_bar_color);
        kui_draw_filled_rect(fb,stride,fw,fh,0,0,fw,win->title_bar_height,tb);
        uint32_t tc=kui_col32(kui_ctx.theme.text_primary);
        kui_draw_text(fb,stride,fw,fh,8,4,win->title,tc,kui_ctx.theme.font_size,0);
        uint32_t bc=kui_col32(kui_ctx.theme.border);
        kui_draw_rect(fb,stride,fw,fh,0,0,fw,fh,bc);
    }
    kui_component_t* comp = win->components_head;
    while(comp) {
        if(comp->visible && comp->draw) {
            comp->draw(comp, fb, stride, fw, fh);
        }
        comp = comp->next;
    }
}

void kui_present(void)
{
    kui_window_t* w = kui_ctx.window_list;
    while(w) {
        if(w->visible && w->back_buffer && w->front_buffer) {
            memcpy(w->front_buffer, w->back_buffer, (size_t)w->window_rect.width*w->window_rect.height*4);
        }
        w = w->next;
    }
}

void kui_update(void)
{
    kui_render();
    kui_present();
}

kui_animator_t* kui_animate(kui_component_t* target, float from, float to, uint64_t duration_ms, void (*on_update)(kui_animator_t*,float))
{
    if(kui_ctx.animator_count>=KANVASUI_ANIMATOR_POOL) return NULL;
    kui_animator_t* a = &kui_ctx.animators[kui_ctx.animator_count++];
    a->target = target; a->start_value = from; a->end_value = to;
    a->current_value = from; a->duration_ms = duration_ms;
    a->active = 1; a->on_update = on_update;
    return a;
}

void kui_animator_update_all(uint64_t now_ms)
{
    for(int i=0;i<kui_ctx.animator_count;i++){
        kui_animator_t* a=&kui_ctx.animators[i];
        if(!a->active) continue;
        uint64_t elapsed=now_ms-a->start_time;
        if(elapsed>=a->duration_ms){
            a->current_value=a->end_value; a->active=0;
            if(a->on_complete) a->on_complete(a);
        } else {
            float t=(float)elapsed/(float)a->duration_ms;
            a->current_value=a->start_value+(a->end_value-a->start_value)*t;
        }
        if(a->on_update) a->on_update(a,a->current_value);
    }
}

void kui_animator_cancel(kui_animator_t* a) { if(a) a->active=0; }

void kui_show_tooltip(kui_tooltip_t* tip)
{
    if(!tip||kui_ctx.tooltip_count>=KANVASUI_MAX_TOOLTIPS) return;
    kui_ctx.active_tooltips[kui_ctx.tooltip_count++]=tip;
    tip->base.visible=1;
}
void kui_hide_tooltip(kui_tooltip_t* tip)
{
    if(!tip) return; tip->base.visible=0;
    for(int i=0;i<kui_ctx.tooltip_count;i++){
        if(kui_ctx.active_tooltips[i]==tip){
            kui_ctx.active_tooltips[i]=kui_ctx.active_tooltips[--kui_ctx.tooltip_count];
            break;
        }
    }
}
void kui_update_tooltips(uint64_t now_ms)
{
    for(int i=0;i<kui_ctx.tooltip_count;i++){
        kui_tooltip_t* t=kui_ctx.active_tooltips[i];
        if(now_ms-t->show_time>=(uint64_t)t->timeout_ms) kui_hide_tooltip(t);
    }
}

kui_context_t* kui_get_context(void) { return &kui_ctx; }

int kui_window_init(void) { return kui_init(1024,768,32,NULL,1024*4); }

static void button_draw(kui_component_t* comp, uint32_t* fb, int stride, int fw, int fh)
{
    kui_button_t* btn = (kui_button_t*)comp;
    kui_rect_t b = comp->bounds;
    kui_color_t bg = btn->bg_color;
    if (!comp->enabled) bg = btn->disabled_color;
    else if (btn->is_pressed) bg = btn->pressed_color;
    else if (btn->is_hovered) bg = btn->hover_color;
    uint32_t bg32 = kui_col32(bg);
    uint32_t fg32 = kui_col32(btn->fg_color);
    int r = kui_ctx.theme.corner_radius;
    if (r > 0)
        kui_draw_rounded_rect(fb, stride, fw, fh, b.x, b.y, b.width, b.height, r, bg32);
    else
        kui_draw_filled_rect(fb, stride, fw, fh, b.x, b.y, b.width, b.height, bg32);
    if (btn->text) {
        int fs = kui_ctx.theme.font_size;
        int cw = (int)(fs * 0.6f);
        size_t len = kapi_strlen(btn->text);
        int tw = (int)len * cw;
        int tx = b.x + (b.width - tw) / 2;
        int ty = b.y + (b.height - fs) / 2;
        kui_draw_text(fb, stride, fw, fh, tx, ty, btn->text, fg32, fs, 0);
    }
}

static bool button_on_mouse_move(kui_component_t* comp, int x, int y)
{
    kui_button_t* btn = (kui_button_t*)comp;
    bool was_hovered = btn->is_hovered;
    btn->is_hovered = kui_point_in_rect(x, y, comp->bounds);
    if (btn->is_hovered != was_hovered) kui_invalidate(comp);
    return btn->is_hovered;
}

static bool button_on_left_button(kui_component_t* comp, bool down, int x, int y)
{
    kui_button_t* btn = (kui_button_t*)comp;
    bool inside = kui_point_in_rect(x, y, comp->bounds);
    if (down) {
        btn->is_pressed = inside;
        kui_invalidate(comp);
        return inside;
    }
    if (btn->is_pressed && inside) {
        btn->is_pressed = false;
        kui_invalidate(comp);
        if (btn->on_click) btn->on_click(btn);
        return true;
    }
    btn->is_pressed = false;
    kui_invalidate(comp);
    return false;
}

static void label_draw(kui_component_t* comp, uint32_t* fb, int stride, int fw, int fh)
{
    kui_label_t* lbl = (kui_label_t*)comp;
    if (!lbl->text) return;
    uint32_t c = kui_col32(lbl->text_color);
    kui_draw_text(fb, stride, fw, fh, comp->bounds.x, comp->bounds.y, lbl->text, c, kui_ctx.theme.font_size, 0);
}

static void textbox_draw(kui_component_t* comp, uint32_t* fb, int stride, int fw, int fh)
{
    kui_textbox_t* tb = (kui_textbox_t*)comp;
    kui_rect_t b = comp->bounds;
    uint32_t bg = kui_col32(kui_ctx.theme.background);
    uint32_t border = kui_col32(kui_ctx.theme.border);
    uint32_t tc = kui_col32(kui_ctx.theme.text_primary);
    kui_draw_filled_rect(fb, stride, fw, fh, b.x, b.y, b.width, b.height, bg);
    kui_draw_rect(fb, stride, fw, fh, b.x, b.y, b.width, b.height, border);
    if (tb->text) {
        int fs = kui_ctx.theme.font_size;
        int pad = 4;
        kui_draw_text_clipped(fb, stride, fw, fh, b.x + pad, b.y + pad, tb->text, tc, fs, 0,
                               b.x + pad, b.y + pad, b.width - 2 * pad, b.height - 2 * pad);
    }
    if (comp->focused) {
        int fs = kui_ctx.theme.font_size;
        int cw = (int)(fs * 0.6f);
        int cx = b.x + 4 + (int)tb->cursor_pos * cw;
        int cy1 = b.y + 4, cy2 = b.y + b.height - 4;
        uint32_t cc = kui_col32(kui_ctx.theme.text_primary);
        kui_draw_line(fb, stride, fw, fh, cx, cy1, cx, cy2, cc);
    }
}

static bool textbox_on_left_button(kui_component_t* comp, bool down, int x, int y)
{
    kui_textbox_t* tb = (kui_textbox_t*)comp;
    if (!down) return false;
    bool inside = kui_point_in_rect(x, y, comp->bounds);
    if (inside) {
        comp->focused = true;
        int cw = (int)(kui_ctx.theme.font_size * 0.6f);
        if (cw > 0) {
            int rel_x = x - comp->bounds.x - 4;
            tb->cursor_pos = (size_t)(rel_x / cw);
            if (tb->text && tb->cursor_pos > kapi_strlen(tb->text))
                tb->cursor_pos = kapi_strlen(tb->text);
        }
        kui_invalidate(comp);
    }
    return inside;
}

static bool textbox_on_char_input(kui_component_t* comp, int key, bool down)
{
    kui_textbox_t* tb = (kui_textbox_t*)comp;
    if (!comp->focused || tb->read_only || !down) return false;
    if (key == 8) {
        if (tb->cursor_pos > 0 && tb->text) {
            size_t len = kapi_strlen(tb->text);
            if (tb->cursor_pos <= len) {
                memmove(tb->text + tb->cursor_pos - 1, tb->text + tb->cursor_pos, len - tb->cursor_pos + 1);
                tb->cursor_pos--;
                kui_invalidate(comp);
                if (tb->on_text_changed) tb->on_text_changed(tb);
            }
        }
        return true;
    }
    if (key == 13) {
        if (tb->on_enter_pressed) tb->on_enter_pressed(tb);
        return true;
    }
    if (key >= 32 && key < 127) {
        if (tb->text) {
            size_t len = kapi_strlen(tb->text);
            if (len + 1 < tb->text_capacity) {
                memmove(tb->text + tb->cursor_pos + 1, tb->text + tb->cursor_pos, len - tb->cursor_pos + 1);
                tb->text[tb->cursor_pos] = (char)key;
                tb->cursor_pos++;
                kui_invalidate(comp);
                if (tb->on_text_changed) tb->on_text_changed(tb);
            }
        }
        return true;
    }
    return false;
}

static void progressbar_draw(kui_component_t* comp, uint32_t* fb, int stride, int fw, int fh)
{
    kui_progressbar_t* pb = (kui_progressbar_t*)comp;
    kui_rect_t b = comp->bounds;
    uint32_t bg = kui_col32(pb->background_color);
    uint32_t bar = kui_col32(pb->bar_color);
    kui_draw_filled_rect(fb, stride, fw, fh, b.x, b.y, b.width, b.height, bg);
    int range = pb->max_value - pb->min_value;
    if (range > 0) {
        int fill_w = (int)((float)(pb->current_value - pb->min_value) / range * b.width);
        kui_draw_filled_rect(fb, stride, fw, fh, b.x, b.y, fill_w, b.height, bar);
    }
}

static void slider_draw(kui_component_t* comp, uint32_t* fb, int stride, int fw, int fh)
{
    kui_slider_t* sl = (kui_slider_t*)comp;
    kui_rect_t b = comp->bounds;
    uint32_t track = kui_col32(sl->track_color);
    uint32_t thumb = kui_col32(sl->thumb_color);
    int track_h = 4;
    int ty = b.y + (b.height - track_h) / 2;
    kui_draw_filled_rect(fb, stride, fw, fh, b.x, ty, b.width, track_h, track);
    int range = sl->max_value - sl->min_value;
    if (range > 0) {
        int thumb_x = b.x + (int)((float)(sl->current_value - sl->min_value) / range * b.width);
        int ts = sl->thumb_size > 0 ? sl->thumb_size : 16;
        kui_draw_circle(fb, stride, fw, fh, thumb_x, b.y + b.height / 2, ts / 2, thumb, true);
    }
}

static bool slider_on_left_button(kui_component_t* comp, bool down, int x, int y)
{
    kui_slider_t* sl = (kui_slider_t*)comp;
    if (!down) { sl->dragging = false; return false; }
    bool inside = kui_point_in_rect(x, y, comp->bounds);
    if (inside) {
        sl->dragging = true;
        int range = sl->max_value - sl->min_value;
        if (range > 0 && comp->bounds.width > 0) {
            int rel = x - comp->bounds.x;
            sl->current_value = sl->min_value + (int)((float)rel / comp->bounds.width * range);
            if (sl->current_value < sl->min_value) sl->current_value = sl->min_value;
            if (sl->current_value > sl->max_value) sl->current_value = sl->max_value;
            kui_invalidate(comp);
            if (sl->on_value_changed) sl->on_value_changed(sl, sl->current_value);
        }
    }
    return inside;
}

static bool slider_on_mouse_move(kui_component_t* comp, int x, int y)
{
    kui_slider_t* sl = (kui_slider_t*)comp;
    if (!sl->dragging) return false;
    (void)y;
    int range = sl->max_value - sl->min_value;
    if (range > 0 && comp->bounds.width > 0) {
        int rel = x - comp->bounds.x;
        sl->current_value = sl->min_value + (int)((float)rel / comp->bounds.width * range);
        if (sl->current_value < sl->min_value) sl->current_value = sl->min_value;
        if (sl->current_value > sl->max_value) sl->current_value = sl->max_value;
        kui_invalidate(comp);
        if (sl->on_value_changed) sl->on_value_changed(sl, sl->current_value);
    }
    return true;
}

static void checkbox_draw(kui_component_t* comp, uint32_t* fb, int stride, int fw, int fh)
{
    kui_checkbox_t* cb = (kui_checkbox_t*)comp;
    kui_rect_t b = comp->bounds;
    uint32_t box = kui_col32(cb->box_color);
    uint32_t check = kui_col32(cb->check_color);
    int sz = 16, pad = 4;
    int bx = b.x + pad, by = b.y + (b.height - sz) / 2;
    kui_draw_rect(fb, stride, fw, fh, bx, by, sz, sz, box);
    if (cb->checked) kui_draw_checkmark(fb, stride, fw, fh, bx + 2, by + 2, sz - 4, check);
    if (cb->label) {
        uint32_t tc = kui_col32(kui_ctx.theme.text_primary);
        kui_draw_text(fb, stride, fw, fh, bx + sz + pad, b.y + (b.height - kui_ctx.theme.font_size) / 2,
                       cb->label, tc, kui_ctx.theme.font_size, 0);
    }
}

static bool checkbox_on_left_button(kui_component_t* comp, bool down, int x, int y)
{
    kui_checkbox_t* cb = (kui_checkbox_t*)comp;
    if (!down) return false;
    bool inside = kui_point_in_rect(x, y, comp->bounds);
    if (inside) {
        cb->checked = !cb->checked;
        kui_invalidate(comp);
        if (cb->on_state_changed) cb->on_state_changed(cb, cb->checked);
    }
    return inside;
}

static void listbox_draw(kui_component_t* comp, uint32_t* fb, int stride, int fw, int fh)
{
    kui_listbox_t* lb = (kui_listbox_t*)comp;
    kui_rect_t b = comp->bounds;
    uint32_t bg = kui_col32(lb->item_bg_color);
    uint32_t sel = kui_col32(lb->item_selected_color);
    uint32_t tc = kui_col32(lb->item_text_color);
    uint32_t stc = kui_col32(lb->item_selected_text_color);
    kui_draw_filled_rect(fb, stride, fw, fh, b.x, b.y, b.width, b.height, bg);
    for (int i = 0; i < lb->item_count; i++) {
        int iy = b.y + (i - lb->scroll_offset) * lb->item_height;
        if (iy < b.y || iy + lb->item_height > b.y + b.height) continue;
        bool is_sel = (i == lb->selected_index);
        if (is_sel)
            kui_draw_filled_rect(fb, stride, fw, fh, b.x, iy, b.width, lb->item_height, sel);
        if (lb->items[i].text) {
            uint32_t c = is_sel ? stc : tc;
            kui_draw_text(fb, stride, fw, fh, b.x + 4, iy + 2, lb->items[i].text, c, kui_ctx.theme.font_size, 0);
        }
    }
}

static bool listbox_on_left_button(kui_component_t* comp, bool down, int x, int y)
{
    kui_listbox_t* lb = (kui_listbox_t*)comp;
    if (!down) return false;
    bool inside = kui_point_in_rect(x, y, comp->bounds);
    if (inside && lb->item_height > 0) {
        int rel_y = y - comp->bounds.y;
        int idx = rel_y / lb->item_height + lb->scroll_offset;
        if (idx >= 0 && idx < lb->item_count) {
            lb->selected_index = idx;
            kui_invalidate(comp);
            if (lb->on_selection_changed) lb->on_selection_changed(lb, idx);
        }
    }
    return inside;
}

static void panel_draw(kui_component_t* comp, uint32_t* fb, int stride, int fw, int fh)
{
    kui_panel_t* p = (kui_panel_t*)comp;
    kui_rect_t b = comp->bounds;
    uint32_t bg = kui_col32(p->background_color);
    if (p->corner_radius > 0)
        kui_draw_rounded_rect(fb, stride, fw, fh, b.x, b.y, b.width, b.height, p->corner_radius, bg);
    else
        kui_draw_filled_rect(fb, stride, fw, fh, b.x, b.y, b.width, b.height, bg);
    if (p->border_width > 0) {
        uint32_t bc = kui_col32(p->border_color);
        kui_draw_rect(fb, stride, fw, fh, b.x, b.y, b.width, b.height, bc);
    }
    for (int i = 0; i < p->child_count; i++) {
        kui_component_t* child = p->children[i];
        if (child && child->visible && child->draw)
            child->draw(child, fb, stride, fw, fh);
    }
}

static void image_draw(kui_component_t* comp, uint32_t* fb, int stride, int fw, int fh)
{
    kui_image_t* img = (kui_image_t*)comp;
    if (!img->pixel_data) return;
    kui_rect_t b = comp->bounds;
    if (img->stretch_to_fit)
        kui_draw_image_scaled(fb, stride, fw, fh, b.x, b.y, b.width, b.height,
                               img->pixel_data, img->image_width, img->image_height, img->image_width * 4);
    else
        kui_draw_image(fb, stride, fw, fh, b.x, b.y, img->pixel_data, img->image_width, img->image_height, img->image_width * 4);
}

static void menubar_draw(kui_component_t* comp, uint32_t* fb, int stride, int fw, int fh)
{
    kui_menubar_t* mb = (kui_menubar_t*)comp;
    kui_rect_t b = comp->bounds;
    uint32_t bg = kui_col32(mb->bg_color);
    uint32_t tc = kui_col32(mb->text_color);
    kui_draw_filled_rect(fb, stride, fw, fh, b.x, b.y, b.width, b.height, bg);
    int x = b.x + 8;
    for (int i = 0; i < mb->menu_count; i++) {
        if (mb->menus[i] && mb->menus[i]->text) {
            kui_draw_text(fb, stride, fw, fh, x, b.y + 4, mb->menus[i]->text, tc, kui_ctx.theme.font_size, 0);
            x += (int)(kapi_strlen(mb->menus[i]->text) * kui_ctx.theme.font_size * 0.6f) + 16;
        }
    }
}

static void statusbar_draw(kui_component_t* comp, uint32_t* fb, int stride, int fw, int fh)
{
    kui_statusbar_t* sb = (kui_statusbar_t*)comp;
    kui_rect_t b = comp->bounds;
    uint32_t bg = kui_col32(sb->bg_color);
    uint32_t tc = kui_col32(sb->text_color);
    kui_draw_filled_rect(fb, stride, fw, fh, b.x, b.y, b.width, b.height, bg);
    if (sb->section_count > 0 && sb->sections && sb->sections[0])
        kui_draw_text(fb, stride, fw, fh, b.x + 8, b.y + 2, sb->sections[0], tc, kui_ctx.theme.font_size_small, 0);
}

static void toolbar_draw(kui_component_t* comp, uint32_t* fb, int stride, int fw, int fh)
{
    kui_toolbar_t* tb = (kui_toolbar_t*)comp;
    kui_rect_t b = comp->bounds;
    uint32_t bg = kui_col32(tb->bg_color);
    uint32_t bc = kui_col32(tb->button_color);
    kui_draw_filled_rect(fb, stride, fw, fh, b.x, b.y, b.width, b.height, bg);
    int x = b.x + tb->spacing;
    int y = b.y + tb->spacing;
    for (int i = 0; i < tb->button_count; i++) {
        kui_toolbar_btn_t* btn = &tb->buttons[i];
        uint32_t c = bc;
        if (btn->pressed) c = kui_col32(tb->pressed_color);
        else if (btn->enabled) c = kui_col32(tb->hover_color);
        int sz = tb->button_size;
        if (tb->vertical) {
            kui_draw_filled_rect(fb, stride, fw, fh, x, y, sz, sz, c);
            y += sz + tb->spacing;
        } else {
            kui_draw_filled_rect(fb, stride, fw, fh, x, y, sz, sz, c);
            x += sz + tb->spacing;
        }
    }
}

static void tabcontrol_draw(kui_component_t* comp, uint32_t* fb, int stride, int fw, int fh)
{
    kui_tabcontrol_t* tc = (kui_tabcontrol_t*)comp;
    kui_rect_t b = comp->bounds;
    uint32_t bg = kui_col32(tc->tab_bg_color);
    uint32_t active = kui_col32(tc->tab_active_color);
    uint32_t txt = kui_col32(tc->tab_text_color);
    uint32_t atxt = kui_col32(tc->tab_active_text_color);
    int tab_h = 28;
    kui_draw_filled_rect(fb, stride, fw, fh, b.x, b.y, b.width, tab_h, bg);
    int tx = b.x;
    for (int i = 0; i < tc->tab_count; i++) {
        if (!tc->tabs[i]) continue;
        int tw = (int)(kapi_strlen(tc->tabs[i]->title) * kui_ctx.theme.font_size * 0.6f) + 16;
        uint32_t c = (i == tc->active_tab_index) ? active : bg;
        uint32_t ct = (i == tc->active_tab_index) ? atxt : txt;
        kui_draw_filled_rect(fb, stride, fw, fh, tx, b.y, tw, tab_h, c);
        kui_draw_text(fb, stride, fw, fh, tx + 8, b.y + 4, tc->tabs[i]->title, ct, kui_ctx.theme.font_size, 0);
        tx += tw;
    }
    kui_draw_filled_rect(fb, stride, fw, fh, b.x, b.y + tab_h, b.width, b.height - tab_h, bg);
    if (tc->active_tab_index >= 0 && tc->active_tab_index < tc->tab_count) {
        kui_tab_item_t* tab = tc->tabs[tc->active_tab_index];
        if (tab && tab->content && tab->content->visible && tab->content->draw)
            tab->content->draw(tab->content, fb, stride, fw, fh);
    }
}

static void treeview_draw(kui_component_t* comp, uint32_t* fb, int stride, int fw, int fh)
{
    kui_treeview_t* tv = (kui_treeview_t*)comp;
    kui_rect_t b = comp->bounds;
    uint32_t bg = kui_col32(kui_ctx.theme.background);
    uint32_t tc = kui_col32(tv->node_text_color);
    uint32_t lc = kui_col32(tv->line_color);
    kui_draw_filled_rect(fb, stride, fw, fh, b.x, b.y, b.width, b.height, bg);
    (void)lc;
    int y = b.y + 4;
    int fs = kui_ctx.theme.font_size;
    for (int i = 0; i < tv->root_node_count && y < b.y + b.height; i++) {
        kui_treeview_node_t* node = &tv->root_nodes[i];
        if (node->selected) {
            uint32_t sc = kui_col32(kui_ctx.theme.selection_bg);
            kui_draw_filled_rect(fb, stride, fw, fh, b.x, y, b.width, fs + 4, sc);
        }
        if (node->text)
            kui_draw_text(fb, stride, fw, fh, b.x + tv->indent_size, y + 2, node->text, tc, fs, 0);
        y += fs + 4;
    }
}

static void splitter_draw(kui_component_t* comp, uint32_t* fb, int stride, int fw, int fh)
{
    kui_splitter_t* sp = (kui_splitter_t*)comp;
    kui_rect_t b = comp->bounds;
    uint32_t sc = kui_col32(sp->splitter_color);
    if (sp->horizontal) {
        int sx = b.x + sp->splitter_position;
        kui_draw_filled_rect(fb, stride, fw, fh, sx, b.y, sp->splitter_size, b.height, sc);
    } else {
        int sy = b.y + sp->splitter_position;
        kui_draw_filled_rect(fb, stride, fw, fh, b.x, sy, b.width, sp->splitter_size, sc);
    }
    if (sp->pane1 && sp->pane1->visible && sp->pane1->draw)
        sp->pane1->draw(sp->pane1, fb, stride, fw, fh);
    if (sp->pane2 && sp->pane2->visible && sp->pane2->draw)
        sp->pane2->draw(sp->pane2, fb, stride, fw, fh);
}

static void tooltip_draw(kui_component_t* comp, uint32_t* fb, int stride, int fw, int fh)
{
    kui_tooltip_t* tt = (kui_tooltip_t*)comp;
    if (!tt->text) return;
    kui_rect_t b = comp->bounds;
    uint32_t bg = kui_col32(kui_ctx.theme.foreground);
    uint32_t tc = kui_col32(kui_ctx.theme.background);
    kui_draw_shadow(fb, stride, fw, fh, b.x, b.y, b.width, b.height, 2, 2, 4, 60);
    kui_draw_rounded_rect(fb, stride, fw, fh, b.x, b.y, b.width, b.height, 4, bg);
    kui_draw_text(fb, stride, fw, fh, b.x + 6, b.y + 4, tt->text, tc, kui_ctx.theme.font_size_small, 0);
}

void kui_button_set_draw(kui_button_t* btn)
{
    if (!btn) return;
    btn->base.draw = button_draw;
    btn->base.on_mouse_move = button_on_mouse_move;
    btn->base.on_left_button = button_on_left_button;
}

void kui_label_set_draw(kui_label_t* lbl)
{
    if (!lbl) return;
    lbl->base.draw = label_draw;
}

void kui_textbox_set_draw(kui_textbox_t* tb)
{
    if (!tb) return;
    tb->base.draw = textbox_draw;
    tb->base.on_left_button = textbox_on_left_button;
    tb->base.on_key = textbox_on_char_input;
}

void kui_progressbar_set_draw(kui_progressbar_t* pb)
{
    if (!pb) return;
    pb->base.draw = progressbar_draw;
}

void kui_slider_set_draw(kui_slider_t* sl)
{
    if (!sl) return;
    sl->base.draw = slider_draw;
    sl->base.on_left_button = slider_on_left_button;
    sl->base.on_mouse_move = slider_on_mouse_move;
}

void kui_checkbox_set_draw(kui_checkbox_t* cb)
{
    if (!cb) return;
    cb->base.draw = checkbox_draw;
    cb->base.on_left_button = checkbox_on_left_button;
}

void kui_listbox_set_draw(kui_listbox_t* lb)
{
    if (!lb) return;
    lb->base.draw = listbox_draw;
    lb->base.on_left_button = listbox_on_left_button;
}

void kui_panel_set_draw(kui_panel_t* p)
{
    if (!p) return;
    p->base.draw = panel_draw;
}

void kui_image_set_draw(kui_image_t* img)
{
    if (!img) return;
    img->base.draw = image_draw;
}

void kui_menubar_set_draw(kui_menubar_t* mb)
{
    if (!mb) return;
    mb->base.draw = menubar_draw;
}

void kui_statusbar_set_draw(kui_statusbar_t* sb)
{
    if (!sb) return;
    sb->base.draw = statusbar_draw;
}

void kui_toolbar_set_draw(kui_toolbar_t* tb)
{
    if (!tb) return;
    tb->base.draw = toolbar_draw;
}

void kui_tabcontrol_set_draw(kui_tabcontrol_t* tc)
{
    if (!tc) return;
    tc->base.draw = tabcontrol_draw;
}

void kui_treeview_set_draw(kui_treeview_t* tv)
{
    if (!tv) return;
    tv->base.draw = treeview_draw;
}

void kui_splitter_set_draw(kui_splitter_t* sp)
{
    if (!sp) return;
    sp->base.draw = splitter_draw;
}

void kui_tooltip_set_draw(kui_tooltip_t* tt)
{
    if (!tt) return;
    tt->base.draw = tooltip_draw;
}

void kui_dispatch_event(kui_window_t* win, kui_msg_type_t msg, uint64_t p1, uint64_t p2)
{
    if (!win) return;
    kui_component_t* comp = win->components_head;
    while (comp) {
        if (!comp->visible || !comp->enabled) { comp = comp->next; continue; }
        switch (msg) {
        case KUI_MSG_MOUSE_MOVE:
            if (comp->on_mouse_move) comp->on_mouse_move(comp, (int)p1, (int)p2);
            break;
        case KUI_MSG_LBUTTON_DOWN:
        case KUI_MSG_LBUTTON_UP:
            if (comp->on_left_button) comp->on_left_button(comp, msg == KUI_MSG_LBUTTON_DOWN, (int)p1, (int)p2);
            break;
        case KUI_MSG_RBUTTON_DOWN:
        case KUI_MSG_RBUTTON_UP:
            if (comp->on_right_button) comp->on_right_button(comp, msg == KUI_MSG_RBUTTON_DOWN, (int)p1, (int)p2);
            break;
        case KUI_MSG_KEY_DOWN:
        case KUI_MSG_KEY_UP:
            if (comp->on_key) comp->on_key(comp, (int)p1, msg == KUI_MSG_KEY_DOWN);
            break;
        case KUI_MSG_CHAR_INPUT:
            if (comp->on_key) comp->on_key(comp, (int)p1, true);
            break;
        case KUI_MSG_SCROLL_UP:
        case KUI_MSG_SCROLL_DOWN:
            if (comp->on_scroll) comp->on_scroll(comp, msg == KUI_MSG_SCROLL_UP ? -1 : 1);
            break;
        default: break;
        }
        comp = comp->next;
    }
}