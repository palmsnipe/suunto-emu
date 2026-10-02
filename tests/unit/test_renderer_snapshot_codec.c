#include "../../src/display/nema_backend_internal.h"
#include "../../src/display/nema_tsc6a_internal.h"
#include "test.h"
#include <stdlib.h>
#include <string.h>

/* Wire offsets intentionally independent of the implementation's constants. */
#define CORE_BYTES 1152180u
#define CACHE_BYTES (12u + 172800u + 480u * 480u * 4u)
#define IMAGE_BYTES (CORE_BYTES + CACHE_BYTES)
#define SPAN_OFFSET (CORE_BYTES + 12u)
#define BASELINE_OFFSET (SPAN_OFFSET + 172800u)
static void wire32(uint8_t *p, uint32_t v)
{
    p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8u);
    p[2]=(uint8_t)(v>>16u);p[3]=(uint8_t)(v>>24u);
}
static void unchanged(semu_test_context *context, semu_nema_backend *b,
    const uint8_t *before, size_t n)
{
    semu_error e; uint8_t *after=NULL; size_t size=0u;
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_nema_backend_snapshot_ops.save(b,&after,&size,&e));
    SEMU_TEST_ASSERT(context, size==n && memcmp(before,after,n)==0);
    free(after);
}
static void test_legacy_renderer_requires_new_snapshot(semu_test_context *context)
{
    semu_error e; semu_nema_backend *b=semu_nema_backend_create(&e);
    uint8_t *legacy=calloc(CORE_BYTES,1u), *before=NULL; size_t n=0u;
    SEMU_TEST_ASSERT(context, b && legacy);
    wire32(legacy,1u);wire32(legacy+4u,240u);wire32(legacy+8u,240u);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_nema_backend_snapshot_ops.save(b,&before,&n,&e));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, semu_nema_backend_snapshot_ops.load(b,legacy,CORE_BYTES,&e));
    SEMU_TEST_ASSERT(context, strstr(e.text,"recreate snapshot")!=NULL);
    unchanged(context,b,before,n);
    free(before);free(legacy);semu_nema_backend_destroy(b);
}
static void test_cache_encoding_refuses_before_mutation(semu_test_context *context)
{
    semu_error e; semu_nema_backend *b=semu_nema_backend_create(&e);
    uint8_t *empty=NULL,*before=NULL,*bad; size_t size=0u,n=0u,i;
    const struct { size_t offset; uint32_t value; } corrupt[] = {
        {CORE_BYTES,2u}, {CORE_BYTES+4u,2u}, {CORE_BYTES,1u},
        {CORE_BYTES+8u,1u}, {SPAN_OFFSET,1u},
        {BASELINE_OFFSET,1u}, {IMAGE_BYTES-4u,1u}
    };
    SEMU_TEST_ASSERT(context,b);
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_nema_backend_snapshot_ops.save(b,&empty,&size,&e));
    SEMU_TEST_EQ_U64(context, IMAGE_BYTES,size);
    SEMU_TEST_EQ_U64(context, 2u,empty[0]);
    for (i=CORE_BYTES;i<size;++i) SEMU_TEST_EQ_U64(context,0u,empty[i]);
    b->baseline_valid=1;b->shadow_fresh=1;b->guest_span_base=0x10001000u;
    b->guest_span[0]=42u;b->baseline->pixels[0]=0xff123456u;
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_nema_backend_snapshot_ops.save(b,&before,&n,&e));
    bad=malloc(size+1u);SEMU_TEST_ASSERT(context,bad);
    for (i=0u;i<SEMU_ARRAY_LEN(corrupt);++i) {
        memcpy(bad,empty,size);wire32(bad+corrupt[i].offset,corrupt[i].value);
        SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,semu_nema_backend_snapshot_ops.load(b,bad,size,&e));
        unchanged(context,b,before,n);
    }
    memcpy(bad,before,n);wire32(bad+CORE_BYTES+8u,0x1017fff0u);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,semu_nema_backend_snapshot_ops.load(b,bad,n,&e));
    unchanged(context,b,before,n);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,semu_nema_backend_snapshot_ops.load(b,before,n-1u,&e));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,semu_nema_backend_snapshot_ops.load(b,bad,n+1u,&e));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,semu_nema_backend_snapshot_ops.load(b,bad,3u,&e));
    memcpy(bad,before,n);wire32(bad,3u);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,semu_nema_backend_snapshot_ops.load(b,bad,n,&e));
    unchanged(context,b,before,n);
    SEMU_TEST_EQ_U64(context, SEMU_OK,semu_nema_backend_reset(b));
    /* Reset may leave old cache storage allocated: none of it enters the wire. */
    unchanged(context,b,empty,size);
    SEMU_TEST_EQ_U64(context, SEMU_OK,semu_nema_backend_snapshot_ops.load(b,before,n,&e));
    unchanged(context,b,before,n);
    SEMU_TEST_EQ_U64(context, 1u,b->shadow_fresh);
    SEMU_TEST_EQ_U64(context, 1u,b->baseline_valid);
    SEMU_TEST_EQ_U64(context, 42u,b->guest_span[0]);
    SEMU_TEST_EQ_U64(context, 0xff123456u,b->baseline->pixels[0]);
    free(bad);free(empty);free(before);semu_nema_backend_destroy(b);
}
int main(void)
{
    const semu_test_case cases[]={
        SEMU_TEST_CASE(test_legacy_renderer_requires_new_snapshot),
        SEMU_TEST_CASE(test_cache_encoding_refuses_before_mutation)
    };
    return semu_test_run(cases,SEMU_ARRAY_LEN(cases));
}
