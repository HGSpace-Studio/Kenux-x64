#include "kapi_graphics2d.h"
#include "kapi_window.h"
#include "kapi.h"
#include <string.h>

#define KAPI_GFX_MAX_STOPS 16

static uint32_t next_surface_id = 1;

static void mat_identity(kapi_matrix_t* m)
{
    memset(m, 0, sizeof(kapi_matrix_t));
    m->m[0][0] = 1.0f; m->m[1][1] = 1.0f; m->m[2][2] = 1.0f;
}

static void mat_mul(kapi_matrix_t* r, const kapi_matrix_t* a, const kapi_matrix_t* b)
{
    kapi_matrix_t t;
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) {
            t.m[i][j] = 0;
            for (int k = 0; k < 3; k++) t.m[i][j] += a->m[i][k] * b->m[k][j];
        }
    *r = t;
}

static void mat_translate(kapi_matrix_t* m, float tx, float ty)
{
    kapi_matrix_t t; mat_identity(&t);
    t.m[0][2] = tx; t.m[1][2] = ty;
    mat_mul(m, m, &t);
}

static void mat_scale(kapi_matrix_t* m, float sx, float sy)
{
    kapi_matrix_t t; mat_identity(&t);
    t.m[0][0] = sx; t.m[1][1] = sy;
    mat_mul(m, m, &t);
}

static void mat_rotate(kapi_matrix_t* m, float angle)
{
    kapi_matrix_t t; mat_identity(&t);
    float c = kapi_cosf(angle), s = kapi_sinf(angle);
    t.m[0][0] = c; t.m[0][1] = -s; t.m[1][0] = s; t.m[1][1] = c;
    mat_mul(m, m, &t);
}

static void xform_pt(const kapi_matrix_t* m, float x, float y, float* ox, float* oy)
{
    *ox = m->m[0][0]*x + m->m[0][1]*y + m->m[0][2];
    *oy = m->m[1][0]*x + m->m[1][1]*y + m->m[1][2];
}

static inline uint32_t col_u32(const kapi_color_t* c)
{
    return ((uint32_t)c->a<<24)|((uint32_t)c->r<<16)|((uint32_t)c->g<<8)|(uint32_t)c->b;
}

static inline uint32_t alpha_blend(uint32_t dst, uint32_t src, uint8_t a)
{
    if (a == 0) return dst;
    if (a == 255) return src;
    uint32_t ia = 255 - a;
    uint32_t r = (((src>>16)&0xFF)*a + ((dst>>16)&0xFF)*ia)/255;
    uint32_t g = (((src>>8)&0xFF)*a + ((dst>>8)&0xFF)*ia)/255;
    uint32_t b = ((src&0xFF)*a + (dst&0xFF)*ia)/255;
    return 0xFF000000|(r<<16)|(g<<8)|b;
}

static void fb_pixel(uint8_t* d, uint32_t fmt, int stride, int x, int y, uint32_t c)
{
    if (!d) return;
    if (fmt == KAPI_SURFACE_FORMAT_ARGB8888 || fmt == KAPI_SURFACE_FORMAT_RGB888) {
        ((uint32_t*)(d + y*stride))[x] = c;
    } else if (fmt == KAPI_SURFACE_FORMAT_RGB565) {
        uint8_t r=(c>>16)&0xFF, g=(c>>8)&0xFF, b=c&0xFF;
        ((uint16_t*)(d + y*stride))[x] = ((r&0xF8)<<8)|((g&0xFC)<<3)|(b>>3);
    }
}

static void fb_fill(uint8_t* d, uint32_t fmt, int stride, int w, int h,
                    int x0, int y0, int rw, int rh, uint32_t c)
{
    if (!d) return;
    int x1 = x0+rw; if (x1>w) x1=w; int y1 = y0+rh; if (y1>h) y1=h;
    if (x0<0) x0=0; if (y0<0) y0=0;
    if (fmt == KAPI_SURFACE_FORMAT_ARGB8888 || fmt == KAPI_SURFACE_FORMAT_RGB888) {
        for (int y=y0; y<y1; y++) {
            uint32_t* row = (uint32_t*)(d + y*stride);
            for (int x=x0; x<x1; x++) row[x] = c;
        }
    } else if (fmt == KAPI_SURFACE_FORMAT_RGB565) {
        uint8_t r=(c>>16)&0xFF, g=(c>>8)&0xFF, b=c&0xFF;
        uint16_t c565 = ((r&0xF8)<<8)|((g&0xFC)<<3)|(b>>3);
        for (int y=y0; y<y1; y++) {
            uint16_t* row = (uint16_t*)(d + y*stride);
            for (int x=x0; x<x1; x++) row[x] = c565;
        }
    }
}

static void fb_line(uint8_t* d, uint32_t fmt, int stride, int w, int h,
                    int x0, int y0, int x1, int y1, uint32_t c)
{
    int dx = x1-x0; if (dx<0) dx=-dx;
    int dy = y1-y0; if (dy<0) dy=-dy;
    int sx = x0<x1?1:-1, sy = y0<y1?1:-1, err = dx-dy;
    while (1) {
        if (x0>=0 && x0<w && y0>=0 && y0<h) fb_pixel(d,fmt,stride,x0,y0,c);
        if (x0==x1 && y0==y1) break;
        int e2 = 2*err;
        if (e2>-dy) { err-=dy; x0+=sx; }
        if (e2<dx) { err+=dx; y0+=sy; }
    }
}

static void fb_rect_outline(uint8_t* d, uint32_t fmt, int stride, int w, int h,
                            int x, int y, int rw, int rh, uint32_t c)
{
    fb_line(d,fmt,stride,w,h,x,y,x+rw-1,y,c);
    fb_line(d,fmt,stride,w,h,x,y+rh-1,x+rw-1,y+rh-1,c);
    fb_line(d,fmt,stride,w,h,x,y,x,y+rh-1,c);
    fb_line(d,fmt,stride,w,h,x+rw-1,y,x+rw-1,y+rh-1,c);
}

static void fb_ellipse(uint8_t* d, uint32_t fmt, int stride, int w, int h,
                       int cx, int cy, int rx, int ry, uint32_t c, int filled)
{
    if (rx<=0||ry<=0) return;
    for (int y=-ry; y<=ry; y++)
        for (int x=-rx; x<=rx; x++) {
            long long dx=(long long)x*ry, dy=(long long)y*rx;
            long long r2=(long long)rx*ry;
            if (filled) {
                if (dx*dx+dy*dy <= r2*r2) {
                    int px=cx+x, py=cy+y;
                    if (px>=0&&px<w&&py>=0&&py<h) fb_pixel(d,fmt,stride,px,py,c);
                }
            } else {
                long long dd=dx*dx+dy*dy;
                long long inner=(r2-rx)*(r2-rx), outer=(r2+rx)*(r2+rx);
                if (dd>=inner&&dd<=outer) {
                    int px=cx+x, py=cy+y;
                    if (px>=0&&px<w&&py>=0&&py<h) fb_pixel(d,fmt,stride,px,py,c);
                }
            }
        }
}

static void fb_round_rect(uint8_t* d, uint32_t fmt, int stride, int w, int h,
                          int x, int y, int rw, int rh, int radius, uint32_t c, int filled)
{
    if (radius<=0) {
        if (filled) fb_fill(d,fmt,stride,w,h,x,y,rw,rh,c);
        else fb_rect_outline(d,fmt,stride,w,h,x,y,rw,rh,c);
        return;
    }
    if (radius>rw/2) radius=rw/2;
    if (radius>rh/2) radius=rh/2;
    if (filled) {
        fb_fill(d,fmt,stride,w,h,x+radius,y,rw-2*radius,rh,c);
        fb_fill(d,fmt,stride,w,h,x,y+radius,radius,rh-2*radius,c);
        fb_fill(d,fmt,stride,w,h,x+rw-radius,y+radius,radius,rh-2*radius,c);
        for (int dy=0; dy<=radius; dy++) {
            int dx=(int)kapi_sqrtf((float)(radius*radius-dy*dy));
            for (int i=-dx; i<=dx; i++) {
                int px,py;
                py=y+radius-dy;
                px=x+radius+i; if(px>=0&&px<w&&py>=0&&py<h) fb_pixel(d,fmt,stride,px,py,c);
                px=x+rw-radius+i; if(px>=0&&px<w&&py>=0&&py<h) fb_pixel(d,fmt,stride,px,py,c);
                py=y+rh-radius+dy;
                px=x+radius+i; if(px>=0&&px<w&&py>=0&&py<h) fb_pixel(d,fmt,stride,px,py,c);
                px=x+rw-radius+i; if(px>=0&&px<w&&py>=0&&py<h) fb_pixel(d,fmt,stride,px,py,c);
            }
        }
    } else {
        fb_line(d,fmt,stride,w,h,x+radius,y,x+rw-radius,y,c);
        fb_line(d,fmt,stride,w,h,x+radius,y+rh-1,x+rw-radius,y+rh-1,c);
        fb_line(d,fmt,stride,w,h,x,y+radius,x,y+rh-radius,c);
        fb_line(d,fmt,stride,w,h,x+rw-1,y+radius,x+rw-1,y+rh-radius,c);
        for (int dy=0; dy<=radius; dy++) {
            int dx=(int)kapi_sqrtf((float)(radius*radius-dy*dy));
            int pts[8][2] = {
                {x+radius-dx,y+radius-dy},{x+radius-dy,y+radius-dx},
                {x+rw-1-radius+dx,y+radius-dy},{x+rw-1-radius+dy,y+radius-dx},
                {x+radius-dx,y+rh-1-radius+dy},{x+radius-dy,y+rh-1-radius+dx},
                {x+rw-1-radius+dx,y+rh-1-radius+dy},{x+rw-1-radius+dy,y+rh-1-radius+dx}
            };
            for (int k=0; k<8; k++)
                if(pts[k][0]>=0&&pts[k][0]<w&&pts[k][1]>=0&&pts[k][1]<h)
                    fb_pixel(d,fmt,stride,pts[k][0],pts[k][1],c);
        }
    }
}

static void fb_grad_v(uint8_t* d, uint32_t fmt, int stride, int w, int h,
                      int x, int y, int rw, int rh, uint32_t ct, uint32_t cb)
{
    for (int row=0; row<rh; row++) {
        float t = rh>1 ? (float)row/(rh-1) : 0.0f;
        uint8_t r=(uint8_t)(((ct>>16)&0xFF)*(1-t)+((cb>>16)&0xFF)*t);
        uint8_t g=(uint8_t)(((ct>>8)&0xFF)*(1-t)+((cb>>8)&0xFF)*t);
        uint8_t b=(uint8_t)((ct&0xFF)*(1-t)+(cb&0xFF)*t);
        uint32_t c = 0xFF000000|(r<<16)|(g<<8)|b;
        for (int col=0; col<rw; col++) {
            int px=x+col, py=y+row;
            if(px>=0&&px<w&&py>=0&&py<h) fb_pixel(d,fmt,stride,px,py,c);
        }
    }
}

static void fb_grad_h(uint8_t* d, uint32_t fmt, int stride, int w, int h,
                      int x, int y, int rw, int rh, uint32_t cl, uint32_t cr)
{
    for (int col=0; col<rw; col++) {
        float t = rw>1 ? (float)col/(rw-1) : 0.0f;
        uint8_t r=(uint8_t)(((cl>>16)&0xFF)*(1-t)+((cr>>16)&0xFF)*t);
        uint8_t g=(uint8_t)(((cl>>8)&0xFF)*(1-t)+((cr>>8)&0xFF)*t);
        uint8_t b=(uint8_t)((cl&0xFF)*(1-t)+(cr&0xFF)*t);
        uint32_t c = 0xFF000000|(r<<16)|(g<<8)|b;
        for (int row=0; row<rh; row++) {
            int px=x+col, py=y+row;
            if(px>=0&&px<w&&py>=0&&py<h) fb_pixel(d,fmt,stride,px,py,c);
        }
    }
}

static void fb_alpha_rect(uint8_t* d, uint32_t fmt, int stride, int w, int h,
                          int x, int y, int rw, int rh, uint32_t c, uint8_t a)
{
    for (int row=0; row<rh; row++)
        for (int col=0; col<rw; col++) {
            int px=x+col, py=y+row;
            if(px>=0&&px<w&&py>=0&&py<h && fmt==KAPI_SURFACE_FORMAT_ARGB8888) {
                uint32_t* p = (uint32_t*)(d+py*stride);
                p[px] = alpha_blend(p[px], c, a);
            }
        }
}

static void fb_blit(uint8_t* dst, int ds, int dx, int dy,
                    const uint8_t* src, int ss, int sx, int sy,
                    int bw, int bh, int dw, int dh)
{
    for (int y=0; y<bh; y++) {
        int ddy=dy+y, ssy=sy+y;
        if(ddy<0||ddy>=dh||ssy<0) continue;
        for (int x=0; x<bw; x++) {
            int ddx=dx+x, ssx=sx+x;
            if(ddx<0||ddx>=dw||ssx<0) continue;
            ((uint32_t*)(dst+ddy*ds))[ddx] = ((const uint32_t*)(src+ssy*ss))[ssx];
        }
    }
}

static void fb_blit_alpha(uint8_t* dst, int ds, int dx, int dy,
                          const uint8_t* src, int ss, int sx, int sy,
                          int bw, int bh, int dw, int dh, uint8_t a)
{
    for (int y=0; y<bh; y++) {
        int ddy=dy+y, ssy=sy+y;
        if(ddy<0||ddy>=dh||ssy<0) continue;
        for (int x=0; x<bw; x++) {
            int ddx=dx+x, ssx=sx+x;
            if(ddx<0||ddx>=dw||ssx<0) continue;
            uint32_t* dp = (uint32_t*)(dst+ddy*ds);
            const uint32_t* sp = (const uint32_t*)(src+ssy*ss);
            dp[ddx] = alpha_blend(dp[ddx], sp[ssx], a);
        }
    }
}

static kapi_surface_t* ctx_target(kapi_graphics_context_t* ctx)
{
    return ctx->target_surface ? ctx->target_surface : ctx->surface;
}

int kapi_surface_create(kapi_surface_t** surface, uint32_t width, uint32_t height,
                        uint32_t format, kapi_surface_t* parent)
{
    if (!surface||width==0||height==0) return KAPI_EINVAL;
    kapi_surface_t* s = (kapi_surface_t*)kapi_kmalloc(sizeof(kapi_surface_t));
    if (!s) return KAPI_ENOMEM;
    memset(s,0,sizeof(kapi_surface_t));
    s->id = next_surface_id++;
    s->type = KAPI_SURFACE_TYPE_OFFSCREEN;
    s->format = format ? format : KAPI_SURFACE_FORMAT_ARGB8888;
    s->width = width; s->height = height;
    s->stride = width * 4; s->parent = parent;
    s->bounds = (kapi_rect_t){0,0,(int32_t)width,(int32_t)height};
    s->ref_count = 1;
    size_t sz = (size_t)s->stride * height;
    s->data = (uint8_t*)kapi_kmalloc(sz);
    if (!s->data) { kapi_kfree(s); return KAPI_ENOMEM; }
    memset(s->data,0,sz);
    *surface = s;
    return KAPI_OK;
}

int kapi_surface_destroy(kapi_surface_t* surface)
{
    if (!surface) return KAPI_EINVAL;
    if (surface->data) kapi_kfree(surface->data);
    kapi_kfree(surface);
    return KAPI_OK;
}

kapi_surface_t* kapi_surface_from_window(kapi_window_t* window)
{
    if (!window) return NULL;
    return window->properties.surface;
}

kapi_surface_t* kapi_surface_create_offscreen(uint32_t width, uint32_t height, uint32_t format)
{
    kapi_surface_t* s = NULL;
    if (kapi_surface_create(&s,width,height,format,NULL) != KAPI_OK) return NULL;
    return s;
}

int kapi_surface_get_info(kapi_surface_t* surface, uint32_t* width, uint32_t* height,
                          uint32_t* format, uint8_t** data)
{
    if (!surface) return KAPI_EINVAL;
    if (width) *width=surface->width;
    if (height) *height=surface->height;
    if (format) *format=surface->format;
    if (data) *data=surface->data;
    return KAPI_OK;
}

int kapi_surface_set_data(kapi_surface_t* surface, const uint8_t* data, size_t size)
{
    if (!surface||!data) return KAPI_EINVAL;
    size_t mx=(size_t)surface->stride*surface->height;
    memcpy(surface->data, data, size<mx?size:mx);
    return KAPI_OK;
}

int kapi_surface_update(kapi_surface_t* surface, const kapi_rect_t* rect)
{
    (void)surface;(void)rect; return KAPI_OK;
}

int kapi_surface_copy(kapi_surface_t* dst, kapi_surface_t* src,
                      const kapi_rect_t* src_rect, const kapi_rect_t* dest_rect)
{
    if (!dst||!src) return KAPI_EINVAL;
    int sx=src_rect?src_rect->x:0, sy=src_rect?src_rect->y:0;
    int dx=dest_rect?dest_rect->x:0, dy=dest_rect?dest_rect->y:0;
    int cw=src_rect?src_rect->width:(int)src->width, ch=src_rect?src_rect->height:(int)src->height;
    fb_blit(dst->data,dst->stride,dx,dy,src->data,src->stride,sx,sy,cw,ch,(int)dst->width,(int)dst->height);
    return KAPI_OK;
}

int kapi_surface_blend(kapi_surface_t* dst, kapi_surface_t* src,
                       const kapi_rect_t* src_rect, const kapi_rect_t* dest_rect,
                       uint32_t blend_mode, uint8_t alpha)
{
    if (!dst||!src) return KAPI_EINVAL;
    int sx=src_rect?src_rect->x:0, sy=src_rect?src_rect->y:0;
    int dx=dest_rect?dest_rect->x:0, dy=dest_rect?dest_rect->y:0;
    int cw=src_rect?src_rect->width:(int)src->width, ch=src_rect?src_rect->height:(int)src->height;
    if (blend_mode==KAPI_BLEND_MODE_NORMAL||blend_mode==KAPI_BLEND_MODE_NONE)
        fb_blit_alpha(dst->data,dst->stride,dx,dy,src->data,src->stride,sx,sy,cw,ch,(int)dst->width,(int)dst->height,alpha);
    else
        fb_blit(dst->data,dst->stride,dx,dy,src->data,src->stride,sx,sy,cw,ch,(int)dst->width,(int)dst->height);
    return KAPI_OK;
}

int kapi_graphics_context_create(kapi_graphics_context_t** context, kapi_surface_t* surface)
{
    if (!context) return KAPI_EINVAL;
    kapi_graphics_context_t* ctx = (kapi_graphics_context_t*)kapi_kmalloc(sizeof(kapi_graphics_context_t));
    if (!ctx) return KAPI_ENOMEM;
    memset(ctx,0,sizeof(kapi_graphics_context_t));
    ctx->surface = surface; ctx->target_surface = surface;
    ctx->stroke_color = (kapi_color_t){0,0,0,255};
    ctx->fill_color = (kapi_color_t){255,255,255,255};
    ctx->background_color = (kapi_color_t){0,0,0,0};
    ctx->stroke_width = 1; ctx->line_style = KAPI_LINE_STYLE_SOLID;
    ctx->fill_mode = KAPI_FILL_MODE_SOLID; ctx->blend_mode = KAPI_BLEND_MODE_NORMAL;
    mat_identity(&ctx->transform); mat_identity(&ctx->view_transform); mat_identity(&ctx->projection_transform);
    *context = ctx;
    return KAPI_OK;
}

int kapi_graphics_context_destroy(kapi_graphics_context_t* context)
{
    if (!context) return KAPI_EINVAL;
    kapi_kfree(context); return KAPI_OK;
}

int kapi_graphics_context_set_surface(kapi_graphics_context_t* c, kapi_surface_t* s)
{
    if (!c) return KAPI_EINVAL; c->surface=s; c->target_surface=s; return KAPI_OK;
}

int kapi_graphics_context_get_surface(kapi_graphics_context_t* c, kapi_surface_t** s)
{
    if (!c||!s) return KAPI_EINVAL; *s=c->surface; return KAPI_OK;
}

int kapi_graphics_context_clear(kapi_graphics_context_t* c, const kapi_color_t* color)
{
    if (!c||!c->surface) return KAPI_EINVAL;
    kapi_surface_t* s=c->surface;
    uint32_t cv = color ? col_u32(color) : 0;
    if (s->data) memset(s->data,0,(size_t)s->stride*s->height);
    if (cv) fb_fill(s->data,s->format,s->stride,s->width,s->height,0,0,s->width,s->height,cv);
    return KAPI_OK;
}

int kapi_graphics_context_present(kapi_graphics_context_t* c) { (void)c; return KAPI_OK; }

int kapi_graphics_context_set_stroke_color(kapi_graphics_context_t* c, const kapi_color_t* v)
{
    if(!c||!v) return KAPI_EINVAL; c->stroke_color=*v; return KAPI_OK;
}
int kapi_graphics_context_set_fill_color(kapi_graphics_context_t* c, const kapi_color_t* v)
{
    if(!c||!v) return KAPI_EINVAL; c->fill_color=*v; return KAPI_OK;
}
int kapi_graphics_context_set_background_color(kapi_graphics_context_t* c, const kapi_color_t* v)
{
    if(!c||!v) return KAPI_EINVAL; c->background_color=*v; return KAPI_OK;
}
int kapi_graphics_context_get_stroke_color(kapi_graphics_context_t* c, kapi_color_t* v)
{
    if(!c||!v) return KAPI_EINVAL; *v=c->stroke_color; return KAPI_OK;
}
int kapi_graphics_context_get_fill_color(kapi_graphics_context_t* c, kapi_color_t* v)
{
    if(!c||!v) return KAPI_EINVAL; *v=c->fill_color; return KAPI_OK;
}
int kapi_graphics_context_set_stroke_width(kapi_graphics_context_t* c, uint32_t v)
{
    if(!c) return KAPI_EINVAL; c->stroke_width=v; return KAPI_OK;
}
int kapi_graphics_context_set_line_style(kapi_graphics_context_t* c, uint32_t v)
{
    if(!c) return KAPI_EINVAL; c->line_style=v; return KAPI_OK;
}
int kapi_graphics_context_set_fill_mode(kapi_graphics_context_t* c, uint32_t v)
{
    if(!c) return KAPI_EINVAL; c->fill_mode=v; return KAPI_OK;
}
int kapi_graphics_context_set_blend_mode(kapi_graphics_context_t* c, uint32_t v)
{
    if(!c) return KAPI_EINVAL; c->blend_mode=v; return KAPI_OK;
}
int kapi_graphics_context_get_stroke_width(kapi_graphics_context_t* c, uint32_t* v)
{
    if(!c||!v) return KAPI_EINVAL; *v=c->stroke_width; return KAPI_OK;
}
int kapi_graphics_context_get_line_style(kapi_graphics_context_t* c, uint32_t* v)
{
    if(!c||!v) return KAPI_EINVAL; *v=c->line_style; return KAPI_OK;
}
int kapi_graphics_context_get_fill_mode(kapi_graphics_context_t* c, uint32_t* v)
{
    if(!c||!v) return KAPI_EINVAL; *v=c->fill_mode; return KAPI_OK;
}
int kapi_graphics_context_get_blend_mode(kapi_graphics_context_t* c, uint32_t* v)
{
    if(!c||!v) return KAPI_EINVAL; *v=c->blend_mode; return KAPI_OK;
}

int kapi_graphics_context_set_transform(kapi_graphics_context_t* c, const kapi_matrix_t* m)
{
    if(!c||!m) return KAPI_EINVAL; c->transform=*m; return KAPI_OK;
}
int kapi_graphics_context_get_transform(kapi_graphics_context_t* c, kapi_matrix_t* m)
{
    if(!c||!m) return KAPI_EINVAL; *m=c->transform; return KAPI_OK;
}
int kapi_graphics_context_translate(kapi_graphics_context_t* c, float tx, float ty)
{
    if(!c) return KAPI_EINVAL; mat_translate(&c->transform,tx,ty); return KAPI_OK;
}
int kapi_graphics_context_scale(kapi_graphics_context_t* c, float sx, float sy)
{
    if(!c) return KAPI_EINVAL; mat_scale(&c->transform,sx,sy); return KAPI_OK;
}
int kapi_graphics_context_rotate(kapi_graphics_context_t* c, float a)
{
    if(!c) return KAPI_EINVAL; mat_rotate(&c->transform,a); return KAPI_OK;
}
int kapi_graphics_context_skew(kapi_graphics_context_t* c, float skx, float sky)
{
    if(!c) return KAPI_EINVAL;
    kapi_matrix_t t; mat_identity(&t); t.m[0][1]=skx; t.m[1][0]=sky;
    mat_mul(&c->transform,&c->transform,&t); return KAPI_OK;
}
int kapi_graphics_context_reset_transform(kapi_graphics_context_t* c)
{
    if(!c) return KAPI_EINVAL; mat_identity(&c->transform); return KAPI_OK;
}

int kapi_graphics_context_enable_clip(kapi_graphics_context_t* c, const kapi_rect_t* r)
{
    if(!c||!r) return KAPI_EINVAL; c->clip_enabled=1; c->clip_rect=*r; return KAPI_OK;
}
int kapi_graphics_context_disable_clip(kapi_graphics_context_t* c)
{
    if(!c) return KAPI_EINVAL; c->clip_enabled=0; return KAPI_OK;
}
int kapi_graphics_context_get_clip(kapi_graphics_context_t* c, kapi_rect_t* r)
{
    if(!c||!r) return KAPI_EINVAL; *r=c->clip_rect; return KAPI_OK;
}

int kapi_graphics_context_draw_point(kapi_graphics_context_t* c, float x, float y)
{
    if(!c) return KAPI_EINVAL;
    kapi_surface_t* s=ctx_target(c); if(!s||!s->data) return KAPI_EINVAL;
    float tx,ty; xform_pt(&c->transform,x,y,&tx,&ty);
    fb_pixel(s->data,s->format,s->stride,(int)tx,(int)ty,col_u32(&c->stroke_color));
    return KAPI_OK;
}

int kapi_graphics_context_draw_line(kapi_graphics_context_t* c, float x1, float y1, float x2, float y2)
{
    if(!c) return KAPI_EINVAL;
    kapi_surface_t* s=ctx_target(c); if(!s||!s->data) return KAPI_EINVAL;
    float tx1,ty1,tx2,ty2;
    xform_pt(&c->transform,x1,y1,&tx1,&ty1); xform_pt(&c->transform,x2,y2,&tx2,&ty2);
    fb_line(s->data,s->format,s->stride,s->width,s->height,(int)tx1,(int)ty1,(int)tx2,(int)ty2,col_u32(&c->stroke_color));
    return KAPI_OK;
}

int kapi_graphics_context_draw_rect(kapi_graphics_context_t* c, float x, float y, float w, float h)
{
    if(!c) return KAPI_EINVAL;
    kapi_surface_t* s=ctx_target(c); if(!s||!s->data) return KAPI_EINVAL;
    float tx,ty; xform_pt(&c->transform,x,y,&tx,&ty);
    fb_rect_outline(s->data,s->format,s->stride,s->width,s->height,(int)tx,(int)ty,(int)w,(int)h,col_u32(&c->stroke_color));
    return KAPI_OK;
}

int kapi_graphics_context_draw_round_rect(kapi_graphics_context_t* c, float x, float y, float w, float h, float r)
{
    if(!c) return KAPI_EINVAL;
    kapi_surface_t* s=ctx_target(c); if(!s||!s->data) return KAPI_EINVAL;
    float tx,ty; xform_pt(&c->transform,x,y,&tx,&ty);
    fb_round_rect(s->data,s->format,s->stride,s->width,s->height,(int)tx,(int)ty,(int)w,(int)h,(int)r,col_u32(&c->stroke_color),0);
    return KAPI_OK;
}

int kapi_graphics_context_draw_ellipse(kapi_graphics_context_t* c, float cx, float cy, float rx, float ry)
{
    if(!c) return KAPI_EINVAL;
    kapi_surface_t* s=ctx_target(c); if(!s||!s->data) return KAPI_EINVAL;
    float tx,ty; xform_pt(&c->transform,cx,cy,&tx,&ty);
    fb_ellipse(s->data,s->format,s->stride,s->width,s->height,(int)tx,(int)ty,(int)rx,(int)ry,col_u32(&c->stroke_color),0);
    return KAPI_OK;
}

int kapi_graphics_context_draw_arc(kapi_graphics_context_t* c, float cx, float cy, float r, float sa, float ea)
{
    if(!c) return KAPI_EINVAL;
    kapi_surface_t* s=ctx_target(c); if(!s||!s->data) return KAPI_EINVAL;
    float tx,ty; xform_pt(&c->transform,cx,cy,&tx,&ty);
    int steps=(int)((ea-sa)*r/2); if(steps<8)steps=8; if(steps>360)steps=360;
    float da=(ea-sa)/steps; uint32_t cv=col_u32(&c->stroke_color);
    float px=tx+r*kapi_cosf(sa), py=ty+r*kapi_sinf(sa);
    for(int i=1;i<=steps;i++){
        float a=sa+da*i;
        float nx=tx+r*kapi_cosf(a), ny=ty+r*kapi_sinf(a);
        fb_line(s->data,s->format,s->stride,s->width,s->height,(int)px,(int)py,(int)nx,(int)ny,cv);
        px=nx; py=ny;
    }
    return KAPI_OK;
}

int kapi_graphics_context_fill_rect(kapi_graphics_context_t* c, float x, float y, float w, float h)
{
    if(!c) return KAPI_EINVAL;
    kapi_surface_t* s=ctx_target(c); if(!s||!s->data) return KAPI_EINVAL;
    float tx,ty; xform_pt(&c->transform,x,y,&tx,&ty);
    if(c->current_gradient && c->fill_mode==KAPI_FILL_MODE_GRADIENT && c->current_gradient->stop_count>=2) {
        kapi_gradient_t* g=c->current_gradient;
        uint32_t c1=col_u32(&g->stops[0].color), c2=col_u32(&g->stops[g->stop_count-1].color);
        if(g->type==KAPI_GRADIENT_TYPE_LINEAR) {
            fb_grad_v(s->data,s->format,s->stride,s->width,s->height,(int)tx,(int)ty,(int)w,(int)h,c1,c2);
            return KAPI_OK;
        }
    }
    fb_fill(s->data,s->format,s->stride,s->width,s->height,(int)tx,(int)ty,(int)w,(int)h,col_u32(&c->fill_color));
    return KAPI_OK;
}

int kapi_graphics_context_fill_round_rect(kapi_graphics_context_t* c, float x, float y, float w, float h, float r)
{
    if(!c) return KAPI_EINVAL;
    kapi_surface_t* s=ctx_target(c); if(!s||!s->data) return KAPI_EINVAL;
    float tx,ty; xform_pt(&c->transform,x,y,&tx,&ty);
    fb_round_rect(s->data,s->format,s->stride,s->width,s->height,(int)tx,(int)ty,(int)w,(int)h,(int)r,col_u32(&c->fill_color),1);
    return KAPI_OK;
}

int kapi_graphics_context_fill_ellipse(kapi_graphics_context_t* c, float cx, float cy, float rx, float ry)
{
    if(!c) return KAPI_EINVAL;
    kapi_surface_t* s=ctx_target(c); if(!s||!s->data) return KAPI_EINVAL;
    float tx,ty; xform_pt(&c->transform,cx,cy,&tx,&ty);
    fb_ellipse(s->data,s->format,s->stride,s->width,s->height,(int)tx,(int)ty,(int)rx,(int)ry,col_u32(&c->fill_color),1);
    return KAPI_OK;
}

int kapi_graphics_context_fill_polygon(kapi_graphics_context_t* c, const kapi_point_t* pts, uint32_t cnt)
{
    if(!c||!pts||cnt<3) return KAPI_EINVAL;
    kapi_surface_t* s=ctx_target(c); if(!s||!s->data) return KAPI_EINVAL;
    uint32_t cv=col_u32(&c->fill_color);
    int miny=pts[0].y, maxy=pts[0].y;
    for(uint32_t i=1;i<cnt;i++){if(pts[i].y<miny)miny=pts[i].y; if(pts[i].y>maxy)maxy=pts[i].y;}
    for(int y=miny;y<=maxy;y++){
        int ix[64]; int ni=0;
        for(uint32_t i=0;i<cnt&&ni<64;i++){
            uint32_t j=(i+1)%cnt;
            int y0=pts[i].y,y1=pts[j].y;
            if((y0<=y&&y1>y)||(y1<=y&&y0>y))
                ix[ni++]=pts[i].x+(y-y0)*(pts[j].x-pts[i].x)/(y1-y0);
        }
        for(int k=0;k<ni-1;k+=2){
            int x0=ix[k],x1=ix[k+1]; if(x0>x1){int t=x0;x0=x1;x1=t;}
            for(int x=x0;x<=x1;x++) fb_pixel(s->data,s->format,s->stride,x,y,cv);
        }
    }
    return KAPI_OK;
}

int kapi_path_create(kapi_path_t** path, kapi_surface_t* surface)
{
    if(!path) return KAPI_EINVAL;
    kapi_path_t* p=(kapi_path_t*)kapi_kmalloc(sizeof(kapi_path_t));
    if(!p) return KAPI_ENOMEM; memset(p,0,sizeof(kapi_path_t));
    p->surface=surface; p->points.capacity=64;
    p->points.points=(kapi_point_t*)kapi_kmalloc(sizeof(kapi_point_t)*64);
    p->points.count=0; *path=p; return KAPI_OK;
}

int kapi_path_destroy(kapi_path_t* p)
{
    if(!p) return KAPI_EINVAL;
    if(p->points.points) kapi_kfree(p->points.points);
    if(p->control_points.points) kapi_kfree(p->control_points.points);
    kapi_kfree(p); return KAPI_OK;
}

static void path_add(kapi_path_t* p, float x, float y)
{
    if(p->points.count>=p->points.capacity){
        p->points.capacity*=2;
        p->points.points=(kapi_point_t*)kapi_krealloc(p->points.points,sizeof(kapi_point_t)*p->points.capacity);
    }
    p->points.points[p->points.count++]=(kapi_point_t){(int32_t)x,(int32_t)y};
}

int kapi_path_move_to(kapi_path_t* p, float x, float y)
{
    if(!p) return KAPI_EINVAL; path_add(p,x,y); p->subpath_count++; return KAPI_OK;
}
int kapi_path_line_to(kapi_path_t* p, float x, float y)
{
    if(!p) return KAPI_EINVAL; path_add(p,x,y); return KAPI_OK;
}
int kapi_path_quad_to(kapi_path_t* p, float cx, float cy, float x, float y)
{
    if(!p||p->points.count==0) return KAPI_EINVAL;
    kapi_point_t last=p->points.points[p->points.count-1];
    for(int i=1;i<=16;i++){
        float t=(float)i/16, it=1.0f-t;
        path_add(p, it*it*last.x+2*it*t*cx+t*t*x, it*it*last.y+2*it*t*cy+t*t*y);
    }
    return KAPI_OK;
}
int kapi_path_cubic_to(kapi_path_t* p, float cx1, float cy1, float cx2, float cy2, float x, float y)
{
    if(!p||p->points.count==0) return KAPI_EINVAL;
    kapi_point_t last=p->points.points[p->points.count-1];
    for(int i=1;i<=20;i++){
        float t=(float)i/20, it=1.0f-t;
        path_add(p, it*it*it*last.x+3*it*it*t*cx1+3*it*t*t*cx2+t*t*t*x,
                    it*it*it*last.y+3*it*it*t*cy1+3*it*t*t*cy2+t*t*t*y);
    }
    return KAPI_OK;
}
int kapi_path_close(kapi_path_t* p) { (void)p; return KAPI_OK; }
int kapi_path_draw(kapi_graphics_context_t* c, kapi_path_t* p) { return kapi_path_stroke(c,p); }
int kapi_path_fill(kapi_graphics_context_t* c, kapi_path_t* p)
{
    if(!c||!p) return KAPI_EINVAL;
    return kapi_graphics_context_fill_polygon(c,p->points.points,p->points.count);
}
int kapi_path_stroke(kapi_graphics_context_t* c, kapi_path_t* p)
{
    if(!c||!p||p->points.count<2) return KAPI_EINVAL;
    kapi_surface_t* s=ctx_target(c); if(!s||!s->data) return KAPI_EINVAL;
    uint32_t cv=col_u32(&c->stroke_color);
    for(uint32_t i=1;i<p->points.count;i++)
        fb_line(s->data,s->format,s->stride,s->width,s->height,
                p->points.points[i-1].x,p->points.points[i-1].y,p->points.points[i].x,p->points.points[i].y,cv);
    return KAPI_OK;
}

int kapi_font_create(kapi_font_t** font, const char* name, float size)
{
    if(!font) return KAPI_EINVAL;
    kapi_font_t* f=(kapi_font_t*)kapi_kmalloc(sizeof(kapi_font_t));
    if(!f) return KAPI_ENOMEM; memset(f,0,sizeof(kapi_font_t));
    if(name) strncpy(f->name,name,sizeof(f->name)-1);
    f->size=size>0?size:12.0f; f->style=KAPI_FONT_STYLE_NORMAL; f->weight=400;
    f->line_height=f->size*1.2f; f->ascent=f->size*0.8f; f->descent=f->size*0.2f;
    *font=f; return KAPI_OK;
}
int kapi_font_destroy(kapi_font_t* f) { if(!f) return KAPI_EINVAL; kapi_kfree(f); return KAPI_OK; }
int kapi_font_set_style(kapi_font_t* f, uint32_t s, uint32_t w)
{
    if(!f) return KAPI_EINVAL; f->style=s; f->weight=w; return KAPI_OK;
}
int kapi_font_set_size(kapi_font_t* f, float s)
{
    if(!f) return KAPI_EINVAL; f->size=s; f->line_height=s*1.2f; f->ascent=s*0.8f; f->descent=s*0.2f; return KAPI_OK;
}
kapi_font_t* kapi_font_get_system_font(const char* name)
{
    kapi_font_t* f=NULL; kapi_font_create(&f,name?name:"default",16.0f); return f;
}
kapi_font_t* kapi_font_load_from_file(const char* fn, float sz)
{
    kapi_font_t* f=NULL; kapi_font_create(&f,fn,sz); return f;
}

int kapi_graphics_context_set_font(kapi_graphics_context_t* c, kapi_font_t* f)
{
    if(!c) return KAPI_EINVAL; c->current_font=f; return KAPI_OK;
}
int kapi_graphics_context_get_font(kapi_graphics_context_t* c, kapi_font_t** f)
{
    if(!c||!f) return KAPI_EINVAL; *f=c->current_font; return KAPI_OK;
}

int kapi_graphics_context_draw_text(kapi_graphics_context_t* c, const char* text, float x, float y)
{
    if(!c||!text) return KAPI_EINVAL;
    kapi_surface_t* s=ctx_target(c); if(!s||!s->data) return KAPI_EINVAL;
    float tx,ty; xform_pt(&c->transform,x,y,&tx,&ty);
    float fs=c->current_font?c->current_font->size:12.0f;
    uint32_t cv=col_u32(&c->fill_color);
    int ix=(int)tx, iy=(int)ty;
    size_t len=kapi_strlen(text);
    for(size_t i=0;i<len;i++){
        int cw=(int)(fs*0.6f), ch=(int)fs;
        fb_fill(s->data,s->format,s->stride,s->width,s->height,ix+1,iy+2,cw-2,ch-4,cv);
        ix+=cw;
    }
    return KAPI_OK;
}

int kapi_graphics_context_draw_text_ext(kapi_graphics_context_t* c, const char* text,
                                        float x, float y, float width, uint32_t align)
{
    if(!c||!text) return KAPI_EINVAL;
    float fs=c->current_font?c->current_font->size:12.0f;
    float tw=(float)kapi_strlen(text)*fs*0.6f;
    float dx=x;
    if(align==KAPI_TEXT_ALIGN_CENTER) dx=x+(width-tw)/2;
    else if(align==KAPI_TEXT_ALIGN_RIGHT) dx=x+width-tw;
    return kapi_graphics_context_draw_text(c,text,dx,y);
}

int kapi_graphics_context_measure_text(kapi_graphics_context_t* c, const char* text, float* w, float* h)
{
    if(!c||!text) return KAPI_EINVAL;
    float fs=c->current_font?c->current_font->size:12.0f;
    if(w) *w=(float)kapi_strlen(text)*fs*0.6f;
    if(h) *h=fs;
    return KAPI_OK;
}

int kapi_image_create(kapi_image_t** img, uint32_t w, uint32_t h, uint32_t fmt)
{
    if(!img) return KAPI_EINVAL;
    kapi_image_t* im=(kapi_image_t*)kapi_kmalloc(sizeof(kapi_image_t));
    if(!im) return KAPI_ENOMEM; memset(im,0,sizeof(kapi_image_t));
    im->width=w; im->height=h; im->format=fmt?fmt:KAPI_SURFACE_FORMAT_ARGB8888;
    im->data_size=(size_t)w*h*4; im->data=(uint8_t*)kapi_kmalloc(im->data_size);
    if(!im->data){kapi_kfree(im);return KAPI_ENOMEM;} memset(im->data,0,im->data_size);
    *img=im; return KAPI_OK;
}
int kapi_image_destroy(kapi_image_t* im)
{
    if(!im) return KAPI_EINVAL; if(im->data) kapi_kfree(im->data); kapi_kfree(im); return KAPI_OK;
}
int kapi_image_load_from_file(kapi_image_t** img, const char* fn)
{
    if(!img||!fn) return KAPI_EINVAL;
    kapi_file_t f=kapi_open(fn,KAPI_O_RDONLY,0); if(!f) return KAPI_ENOENT;
    kapi_stat_t st; kapi_fstat(f,&st); size_t sz=st.st_size;
    uint8_t* d=(uint8_t*)kapi_kmalloc(sz); if(!d){kapi_close(f);return KAPI_ENOMEM;}
    kapi_read(f,d,sz); kapi_close(f);
    int ret=kapi_image_create(img,64,64,KAPI_SURFACE_FORMAT_ARGB8888);
    if(ret!=KAPI_OK){kapi_kfree(d);return ret;}
    size_t cp=sz<(*img)->data_size?sz:(*img)->data_size;
    memcpy((*img)->data,d,cp); kapi_kfree(d); return KAPI_OK;
}
int kapi_image_save_to_file(kapi_image_t* im, const char* fn)
{
    if(!im||!fn) return KAPI_EINVAL;
    kapi_file_t f=kapi_open(fn,KAPI_O_WRONLY|KAPI_O_CREAT|KAPI_O_TRUNC,0644);
    if(!f) return KAPI_EACCES; kapi_write(f,im->data,im->data_size); kapi_close(f); return KAPI_OK;
}

int kapi_graphics_context_draw_image(kapi_graphics_context_t* c, kapi_image_t* im, float x, float y, float w, float h)
{
    if(!c||!im) return KAPI_EINVAL;
    kapi_surface_t* s=ctx_target(c); if(!s||!s->data) return KAPI_EINVAL;
    float tx,ty; xform_pt(&c->transform,x,y,&tx,&ty);
    int dw=(int)w>0?(int)w:(int)im->width, dh=(int)h>0?(int)h:(int)im->height;
    fb_blit(s->data,s->stride,(int)tx,(int)ty,im->data,(int)im->width*4,0,0,
            (int)im->width,(int)im->height,(int)s->width,(int)s->height);
    return KAPI_OK;
}
int kapi_graphics_context_draw_image_ext(kapi_graphics_context_t* c, kapi_image_t* im,
                                         const kapi_rect_t* sr, const kapi_rect_t* dr)
{
    if(!c||!im) return KAPI_EINVAL;
    kapi_surface_t* s=ctx_target(c); if(!s||!s->data) return KAPI_EINVAL;
    int sx=sr?sr->x:0, sy=sr?sr->y:0, sw=sr?sr->width:(int)im->width, sh=sr?sr->height:(int)im->height;
    int dx=dr?dr->x:0, dy=dr?dr->y:0;
    float tx,ty; xform_pt(&c->transform,(float)dx,(float)dy,&tx,&ty);
    fb_blit(s->data,s->stride,(int)tx,(int)ty,im->data,(int)im->width*4,sx,sy,sw,sh,(int)s->width,(int)s->height);
    return KAPI_OK;
}

int kapi_gradient_create(kapi_gradient_t** g)
{
    if(!g) return KAPI_EINVAL;
    kapi_gradient_t* gr=(kapi_gradient_t*)kapi_kmalloc(sizeof(kapi_gradient_t));
    if(!gr) return KAPI_ENOMEM; memset(gr,0,sizeof(kapi_gradient_t));
    gr->stops=(kapi_color_stop_t*)kapi_kmalloc(sizeof(kapi_color_stop_t)*KAPI_GFX_MAX_STOPS);
    gr->stop_count=0; mat_identity(&gr->transform); *g=gr; return KAPI_OK;
}
int kapi_gradient_destroy(kapi_gradient_t* g)
{
    if(!g) return KAPI_EINVAL; if(g->stops) kapi_kfree(g->stops); kapi_kfree(g); return KAPI_OK;
}
int kapi_gradient_set_type(kapi_gradient_t* g, uint32_t t)
{
    if(!g) return KAPI_EINVAL; g->type=t; return KAPI_OK;
}
int kapi_gradient_set_linear(kapi_gradient_t* g, float x1, float y1, float x2, float y2)
{
    if(!g) return KAPI_EINVAL; g->type=KAPI_GRADIENT_TYPE_LINEAR;
    g->start_point=(kapi_point_t){(int32_t)x1,(int32_t)y1}; g->end_point=(kapi_point_t){(int32_t)x2,(int32_t)y2};
    return KAPI_OK;
}
int kapi_gradient_set_radial(kapi_gradient_t* g, float cx, float cy, float r)
{
    if(!g) return KAPI_EINVAL; g->type=KAPI_GRADIENT_TYPE_RADIAL;
    g->center=(kapi_vector3d_t){(int32_t)cx,(int32_t)cy,0}; g->radius=(kapi_vector3d_t){(int32_t)r,0,0};
    return KAPI_OK;
}
int kapi_gradient_add_color_stop(kapi_gradient_t* g, const kapi_color_t* c, float pos)
{
    if(!g||!c||g->stop_count>=KAPI_GFX_MAX_STOPS) return KAPI_EINVAL;
    g->stops[g->stop_count].color=*c; g->stops[g->stop_count].position=pos; g->stop_count++;
    return KAPI_OK;
}
int kapi_graphics_context_set_gradient(kapi_graphics_context_t* c, kapi_gradient_t* g)
{
    if(!c) return KAPI_EINVAL; c->current_gradient=g; return KAPI_OK;
}
int kapi_graphics_context_fill_gradient(kapi_graphics_context_t* c, const kapi_rect_t* r)
{
    if(!c||!r||!c->current_gradient) return KAPI_EINVAL;
    kapi_surface_t* s=ctx_target(c); if(!s||!s->data) return KAPI_EINVAL;
    kapi_gradient_t* g=c->current_gradient; if(g->stop_count<2) return KAPI_EINVAL;
    uint32_t c1=col_u32(&g->stops[0].color), c2=col_u32(&g->stops[g->stop_count-1].color);
    float tx,ty; xform_pt(&c->transform,(float)r->x,(float)r->y,&tx,&ty);
    if(g->type==KAPI_GRADIENT_TYPE_LINEAR)
        fb_grad_v(s->data,s->format,s->stride,s->width,s->height,(int)tx,(int)ty,r->width,r->height,c1,c2);
    else
        fb_fill(s->data,s->format,s->stride,s->width,s->height,(int)tx,(int)ty,r->width,r->height,c1);
    return KAPI_OK;
}

int kapi_matrix_identity(kapi_matrix_t* m) { if(!m) return KAPI_EINVAL; mat_identity(m); return KAPI_OK; }
int kapi_matrix_multiply(kapi_matrix_t* r, const kapi_matrix_t* a, const kapi_matrix_t* b)
{
    if(!r||!a||!b) return KAPI_EINVAL; mat_mul(r,a,b); return KAPI_OK;
}
int kapi_matrix_translate(kapi_matrix_t* m, float tx, float ty)
{
    if(!m) return KAPI_EINVAL; mat_translate(m,tx,ty); return KAPI_OK;
}
int kapi_matrix_scale(kapi_matrix_t* m, float sx, float sy)
{
    if(!m) return KAPI_EINVAL; mat_scale(m,sx,sy); return KAPI_OK;
}
int kapi_matrix_rotate(kapi_matrix_t* m, float a)
{
    if(!m) return KAPI_EINVAL; mat_rotate(m,a); return KAPI_OK;
}
int kapi_matrix_invert(kapi_matrix_t* m)
{
    if(!m) return KAPI_EINVAL;
    float det=m->m[0][0]*(m->m[1][1]*m->m[2][2]-m->m[1][2]*m->m[2][1])
             -m->m[0][1]*(m->m[1][0]*m->m[2][2]-m->m[1][2]*m->m[2][0])
             +m->m[0][2]*(m->m[1][0]*m->m[2][1]-m->m[1][1]*m->m[2][0]);
    if(det==0.0f) return KAPI_EINVAL;
    float id=1.0f/det; kapi_matrix_t inv;
    inv.m[0][0]=(m->m[1][1]*m->m[2][2]-m->m[1][2]*m->m[2][1])*id;
    inv.m[0][1]=(m->m[0][2]*m->m[2][1]-m->m[0][1]*m->m[2][2])*id;
    inv.m[0][2]=(m->m[0][1]*m->m[1][2]-m->m[0][2]*m->m[1][1])*id;
    inv.m[1][0]=(m->m[1][2]*m->m[2][0]-m->m[1][0]*m->m[2][2])*id;
    inv.m[1][1]=(m->m[0][0]*m->m[2][2]-m->m[0][2]*m->m[2][0])*id;
    inv.m[1][2]=(m->m[0][2]*m->m[1][0]-m->m[0][0]*m->m[1][2])*id;
    inv.m[2][0]=(m->m[1][0]*m->m[2][1]-m->m[1][1]*m->m[2][0])*id;
    inv.m[2][1]=(m->m[0][1]*m->m[2][0]-m->m[0][0]*m->m[2][1])*id;
    inv.m[2][2]=(m->m[0][0]*m->m[1][1]-m->m[0][1]*m->m[1][0])*id;
    *m=inv; return KAPI_OK;
}

int kapi_graphics_context_draw_debug_info(kapi_graphics_context_t* c) { (void)c; return KAPI_OK; }
int kapi_graphics_context_dump_surface_info(kapi_surface_t* s) { (void)s; return KAPI_OK; }

int kapi_graphics_context_draw_grid(kapi_graphics_context_t* c, float x, float y,
                                    float w, float h, float cw, float ch)
{
    if(!c) return KAPI_EINVAL;
    kapi_surface_t* s=ctx_target(c); if(!s||!s->data) return KAPI_EINVAL;
    uint32_t cv=col_u32(&c->stroke_color);
    for(float cx=x;cx<=x+w;cx+=cw) fb_line(s->data,s->format,s->stride,s->width,s->height,(int)cx,(int)y,(int)cx,(int)(y+h),cv);
    for(float cy=y;cy<=y+h;cy+=ch) fb_line(s->data,s->format,s->stride,s->width,s->height,(int)x,(int)cy,(int)(x+w),(int)cy,cv);
    return KAPI_OK;
}

int kapi_graphics_context_draw_progress_bar(kapi_graphics_context_t* c, float x, float y,
                                            float w, float h, float progress)
{
    if(!c) return KAPI_EINVAL;
    kapi_rect_outline(ctx_target(c)->data,ctx_target(c)->format,ctx_target(c)->stride,
                      ctx_target(c)->width,ctx_target(c)->height,(int)x,(int)y,(int)w,(int)h,col_u32(&c->stroke_color));
    int fw=(int)(w*progress); if(fw>0) fw=fw<(int)w?fw:(int)w;
    fb_fill(ctx_target(c)->data,ctx_target(c)->format,ctx_target(c)->stride,
            ctx_target(c)->width,ctx_target(c)->height,(int)x+1,(int)y+1,fw,(int)h-2,col_u32(&c->fill_color));
    return KAPI_OK;
}

int kapi_graphics_context_draw_button(kapi_graphics_context_t* c, float x, float y,
                                      float w, float h, const char* text, uint32_t state)
{
    if(!c) return KAPI_EINVAL;
    kapi_color_t bg = (state==2) ? (kapi_color_t){100,100,100,255} :
                      (state==1) ? (kapi_color_t){180,180,180,255} : c->fill_color;
    kapi_graphics_context_fill_round_rect(c,x,y,w,h,4.0f);
    if(text) kapi_graphics_context_draw_text(c,text,x+8,y+4);
    return KAPI_OK;
}

int kapi_graphics_context_draw_scrollbar(kapi_graphics_context_t* c, float x, float y,
                                         float w, float h, float mn, float mx, float val)
{
    if(!c) return KAPI_EINVAL;
    kapi_graphics_context_fill_rect(c,x,y,w,h);
    float range=mx-mn; if(range<=0) range=1;
    float t=(val-mn)/range;
    float thumb_h=h*0.3f; float thumb_y=y+t*(h-thumb_h);
    kapi_color_t save=c->fill_color; c->fill_color=(kapi_color_t){160,160,160,255};
    kapi_graphics_context_fill_round_rect(c,x+2,thumb_y,w-4,thumb_h,3.0f);
    c->fill_color=save;
    return KAPI_OK;
}

int kapi_graphics_context_transform_point3d(kapi_graphics_context_t* c, const kapi_vector3d_t* p, kapi_point_t* sp)
{
    if(!c||!p||!sp) return KAPI_EINVAL;
    sp->x=(int32_t)(c->transform.m[0][0]*p->x+c->transform.m[0][1]*p->y+c->transform.m[0][2]);
    sp->y=(int32_t)(c->transform.m[1][0]*p->x+c->transform.m[1][1]*p->y+c->transform.m[1][2]);
    return KAPI_OK;
}
int kapi_graphics_context_untransform_point3d(kapi_graphics_context_t* c, const kapi_point_t* sp, kapi_vector3d_t* p)
{
    if(!c||!sp||!p) return KAPI_EINVAL;
    kapi_matrix_t inv=c->transform;
    if(kapi_matrix_invert(&inv)!=KAPI_OK) return KAPI_EINVAL;
    p->x=(int32_t)(inv.m[0][0]*sp->x+inv.m[0][1]*sp->y+inv.m[0][2]);
    p->y=(int32_t)(inv.m[1][0]*sp->x+inv.m[1][1]*sp->y+inv.m[1][2]);
    p->z=0; return KAPI_OK;
}

int kapi_graphics2d_init(void) { return KAPI_OK; }