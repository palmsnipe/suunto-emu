#include "nema_backend_internal.h"
#include "nema_state_internal.h"
#include "nema_tsc6a_internal.h"
#include <stdlib.h>
#include <string.h>

#define CODEC_VERSION 2u
#define HEADER_SIZE (44u + NEMA_STATE_REGISTER_COUNT * 4u)
#define CORE_SIZE (HEADER_SIZE + 2u * NEMA_BACKEND_PANEL_BYTES + NEMA_TSC6A_PIXELS * 4u)
#define SPAN_OFFSET (CORE_SIZE + 12u)
#define BASELINE_OFFSET (SPAN_OFFSET + NEMA_TSC6A_SPAN_BYTES)
#define IMAGE_SIZE (BASELINE_OFFSET + NEMA_TSC6A_PIXELS * 4u)

static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8u) |
        ((uint32_t)p[2] << 16u) | ((uint32_t)p[3] << 24u);
}
static uint64_t get64(const uint8_t *p)
{ return get32(p) | ((uint64_t)get32(p + 4u) << 32u); }
static void put32(uint8_t *p, uint32_t v)
{
    p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8u);
    p[2]=(uint8_t)(v>>16u);p[3]=(uint8_t)(v>>24u);
}
static void put64(uint8_t *p, uint64_t v)
{ put32(p,(uint32_t)v);put32(p+4u,(uint32_t)(v>>32u)); }
static semu_status fail(semu_error *e, semu_status code, const char *why)
{ semu_error_set(e,code,"renderer snapshot: %s",why);return code; }

static semu_status save(void *context, uint8_t **data, size_t *size, semu_error *e)
{
    semu_nema_backend *b=context;
    uint8_t *p; uint32_t i;
    if (!b || !data || !size) return fail(e,SEMU_ERR_ARGUMENT,"invalid arguments");
    if (b->phase != NEMA_BACKEND_IDLE) return fail(e,SEMU_ERR_CONFLICT,"active transaction");
    if ((b->shadow_fresh != 0 && b->shadow_fresh != 1) ||
        (b->baseline_valid != 0 && b->baseline_valid != 1) ||
        (b->shadow_fresh && !b->baseline_valid) ||
        (b->baseline_valid && !tsc6a_bounded_sram(b->guest_span_base,NEMA_TSC6A_SPAN_BYTES)))
        return fail(e,SEMU_ERR_STATE,"invalid compressed-frame state");
    p=malloc(IMAGE_SIZE);
    if (!p) return fail(e,SEMU_ERR_NOMEM,"cannot allocate image");
    put32(p,CODEC_VERSION);put32(p+4u,NEMA_BACKEND_PANEL_WIDTH);
    put32(p+8u,NEMA_BACKEND_PANEL_HEIGHT);put32(p+12u,(uint32_t)b->published_valid);
    put64(p+16u,semu_surface_frame(b->surface)->generation);
    put64(p+24u,b->state->presence);put32(p+32u,b->state->list_id);
    put64(p+36u,(uint64_t)b->state->snapshot_count);
    for(i=0u;i<NEMA_STATE_REGISTER_COUNT;++i) put32(p+44u+4u*i,b->state->values[i]);
    memcpy(p+HEADER_SIZE,semu_surface_frame(b->surface)->pixels,NEMA_BACKEND_PANEL_BYTES);
    /* Canonical unused published pixels; reset need not clear an old image. */
    if (b->published_valid)
        memcpy(p+HEADER_SIZE+NEMA_BACKEND_PANEL_BYTES,b->published_pixels,NEMA_BACKEND_PANEL_BYTES);
    else memset(p+HEADER_SIZE+NEMA_BACKEND_PANEL_BYTES,0,NEMA_BACKEND_PANEL_BYTES);
    for(i=0u;i<NEMA_TSC6A_PIXELS;++i)
        put32(p+HEADER_SIZE+2u*NEMA_BACKEND_PANEL_BYTES+4u*i,b->tsc6a->pixels[i]);
    /* The baseline contains history for undecodable blocks and cannot always
     * be re-derived from current RAM. It is part of committed renderer state;
     * staging and rollback buffers remain transient. */
    put32(p+CORE_SIZE,(uint32_t)b->shadow_fresh);
    put32(p+CORE_SIZE+4u,(uint32_t)b->baseline_valid);
    put32(p+CORE_SIZE+8u,b->baseline_valid ? b->guest_span_base : 0u);
    if (b->baseline_valid) {
        memcpy(p+SPAN_OFFSET,b->guest_span,NEMA_TSC6A_SPAN_BYTES);
        for(i=0u;i<NEMA_TSC6A_PIXELS;++i)
            put32(p+BASELINE_OFFSET+4u*i,b->baseline->pixels[i]);
    } else memset(p+SPAN_OFFSET,0,IMAGE_SIZE-SPAN_OFFSET);
    *data=p;*size=IMAGE_SIZE;semu_error_clear(e);return SEMU_OK;
}

static semu_status load(void *context, const uint8_t *p, size_t size, semu_error *e)
{
    semu_nema_backend *b=context;
    nema_state state = {0};
    uint32_t i, published, fresh, valid, base; uint64_t generation, count;
    if (!b || !p) return fail(e,SEMU_ERR_ARGUMENT,"invalid arguments");
    if (b->phase != NEMA_BACKEND_IDLE) return fail(e,SEMU_ERR_CONFLICT,"active transaction");
    if (size < 4u) return fail(e,SEMU_ERR_FORMAT,"invalid length");
    if (get32(p)==1u) return fail(e,SEMU_ERR_UNSUPPORTED,
        "codec version 1 omits compressed-frame state; recreate snapshot");
    if (get32(p)!=CODEC_VERSION) return fail(e,SEMU_ERR_UNSUPPORTED,"unsupported version");
    if (size != IMAGE_SIZE) return fail(e,SEMU_ERR_FORMAT,"invalid length");
    fresh=get32(p+CORE_SIZE);valid=get32(p+CORE_SIZE+4u);base=get32(p+CORE_SIZE+8u);
    if (fresh>1u || valid>1u || (fresh && !valid) ||
        (valid && !tsc6a_bounded_sram(base,NEMA_TSC6A_SPAN_BYTES)) || (!valid && base))
        return fail(e,SEMU_ERR_FORMAT,"invalid compressed-frame state");
    if (!valid) for(i=SPAN_OFFSET;i<IMAGE_SIZE;++i)
        if (p[i]!=0u) return fail(e,SEMU_ERR_FORMAT,"invalid cache is not empty");
    published=get32(p+12u);generation=get64(p+16u);count=get64(p+36u);
    if (get32(p+4u)!=NEMA_BACKEND_PANEL_WIDTH || get32(p+8u)!=NEMA_BACKEND_PANEL_HEIGHT ||
        published>1u || (published && generation==0u) ||
        (get64(p+24u)>>NEMA_STATE_REGISTER_COUNT)!=0u || count>SIZE_MAX)
        return fail(e,SEMU_ERR_FORMAT,"invalid state");
    if (!published) for(i=0u;i<NEMA_BACKEND_PANEL_BYTES;++i)
        if(p[HEADER_SIZE+NEMA_BACKEND_PANEL_BYTES+i]!=0u)
            return fail(e,SEMU_ERR_FORMAT,"unpublished image is not empty");
    state.presence=get64(p+24u);state.list_id=get32(p+32u);state.snapshot_count=(size_t)count;
    for(i=0u;i<NEMA_STATE_REGISTER_COUNT;++i) state.values[i]=get32(p+44u+4u*i);
    /* All validation precedes this allocation-free commit. Pixel bit patterns
     * themselves are unrestricted RGB565 and ARGB shadow values. */
    *b->state=state;
    memcpy(semu_surface_pixels(b->surface,NULL),p+HEADER_SIZE,NEMA_BACKEND_PANEL_BYTES);
    semu_surface_restore_generation(b->surface,generation);
    memcpy(b->published_pixels,p+HEADER_SIZE+NEMA_BACKEND_PANEL_BYTES,NEMA_BACKEND_PANEL_BYTES);
    b->published=*semu_surface_frame(b->surface);b->published.pixels=b->published_pixels;
    b->published_valid=(int)published;
    for(i=0u;i<NEMA_TSC6A_PIXELS;++i)
        b->tsc6a->pixels[i]=get32(p+HEADER_SIZE+2u*NEMA_BACKEND_PANEL_BYTES+4u*i);
    memcpy(b->guest_span,p+SPAN_OFFSET,NEMA_TSC6A_SPAN_BYTES);
    for(i=0u;i<NEMA_TSC6A_PIXELS;++i)
        b->baseline->pixels[i]=get32(p+BASELINE_OFFSET+4u*i);
    b->guest_span_base=base;
    b->baseline_valid=(int)valid;
    b->shadow_fresh=(int)fresh;
    b->pending_shadow_fresh=(int)fresh;
    semu_error_clear(e);return SEMU_OK;
}
static const semu_frame *published_frame(void *context)
{
    semu_nema_backend *b=context;
    return b && b->published_valid ? &b->published : NULL;
}
const semu_display_snapshot_ops semu_nema_backend_snapshot_ops = {
    UINT32_C(0x4e454d41),save,load,published_frame
};
