#include "semu/machine.h"
#include "semu/hash.h"
#include "../../src/display/nema_backend_internal.h"
#include "test.h"
#include "../../src/display/nema_tsc6a_internal.h"
#include "../../src/core/scheduler_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { unsigned count; uint64_t generation; uint32_t crc; } capture;
static void frame(void *context, const semu_frame *f)
{
    capture *c = context;
    ++c->count; c->generation = f->generation;
    c->crc = semu_crc32(0u, f->pixels, f->size);
}
static int contract(char *path, size_t capacity, semu_profile *p,
    semu_firmware_manifest *m, semu_error *e)
{
    uint8_t program[64] = {0,1,0,0x10,0x21,0,0,0,
        [0x20]=0,0xbf,[0x22]=0xfe,0xe7};
    FILE *f; uint64_t size;
    if (!semu_test_temp_path(path, capacity, "renderer-snapshot.bin")) return 0;
    f=fopen(path,"wb"); if (!f) return 0;
    if (fwrite(program,1,sizeof(program),f)!=sizeof(program)) { fclose(f);return 0; }
    if (fclose(f)) return 0;
    memset(p,0,sizeof(*p)); memset(m,0,sizeof(*m));
    p->format=m->format=1u;
    strcpy(p->id,"sapporo-2.22.60"); strcpy(p->board,"sapporo");
    strcpy(p->product,"Synthetic Sapporo"); strcpy(p->version,"renderer-test");
    p->display_width=p->display_height=240u; p->required_count=1u;
    strcpy(m->product,p->product);strcpy(m->version,p->version);m->component_count=1u;
    strcpy(m->components[0].id,"synthetic-reset");
    strcpy(m->components[0].role,"application");
    strcpy(m->components[0].path,path);
    if (semu_sha256_file(path,m->components[0].sha256,&size,e)!=SEMU_OK) return 0;
    m->components[0].size=size;p->required[0]=m->components[0];p->required[0].path[0]=0;
    return 1;
}
static void test_machine_restores_published_renderer(semu_test_context *context)
{
    semu_profile p; semu_firmware_manifest m; semu_machine_options o={0};
    semu_error e; char path[128]; semu_machine *first,*second;
    semu_nema_backend *a,*b; semu_bus *bus; semu_snapshot *snap;
    capture ca={0},cb={0}; uint32_t expected;
    semu_display_snapshot_ops codec=semu_nema_backend_snapshot_ops;
    SEMU_TEST_ASSERT(context,contract(path,sizeof(path),&p,&m,&e));
    a=semu_nema_backend_create(&e);b=semu_nema_backend_create(&e);
    bus=semu_bus_create(&e);snap=semu_snapshot_create(&e);
    SEMU_TEST_ASSERT(context,a&&b&&bus&&snap);
    SEMU_TEST_EQ_U64(context,SEMU_OK,semu_bus_map_ram(bus,"commands",0x10000000u,16u,&e));
    SEMU_TEST_EQ_U64(context,SEMU_OK,semu_bus_write(bus,0x10000000u,4u,NEMA_REG_DRAW_COLOR,&e));
    SEMU_TEST_EQ_U64(context,SEMU_OK,semu_bus_write(bus,0x10000004u,4u,0x1234u,&e));
    semu_surface_clear(a->surface,0x1234u);
    SEMU_TEST_EQ_U64(context,SEMU_TRANSACTION_OK,semu_nema_backend_submit(a,bus,0x10000000u,2u,0u,frame,&ca,&e));
    expected=ca.crc;
    o.display_snapshot=&codec;
    o.profile=&p;o.firmware=&m;o.display_backend=&semu_nema_backend_ops;
    o.display_backend_context=a;o.frame_callback=frame;o.frame_context=&ca;
    first=semu_machine_create(&o,&e);
    o.display_backend_context=b;o.frame_context=&cb;second=semu_machine_create(&o,&e);
    SEMU_TEST_ASSERT(context,first&&second);
    codec.backend_id=0u; /* Existing machines own a copy of the table. */
    SEMU_TEST_ASSERT(context,semu_machine_create(&o,&e)==NULL);
    SEMU_TEST_EQ_U64(context,SEMU_ERR_ARGUMENT,e.code);
    SEMU_TEST_EQ_U64(context,SEMU_OK,semu_machine_snapshot_save(first,snap,&e));
    SEMU_TEST_EQ_U64(context,SEMU_OK,semu_machine_snapshot_load(second,snap,&e));
    SEMU_TEST_EQ_U64(context,semu_machine_virtual_time(first),semu_machine_virtual_time(second));
    SEMU_TEST_EQ_U64(context,semu_machine_instructions(first),semu_machine_instructions(second));
    SEMU_TEST_EQ_U64(context,1u,cb.count);
    SEMU_TEST_EQ_U64(context,ca.generation,cb.generation);
    SEMU_TEST_EQ_U64(context,expected,cb.crc);
    SEMU_TEST_EQ_U64(context,expected,semu_crc32(0,semu_nema_backend_frame(b)->pixels,NEMA_BACKEND_PANEL_BYTES));

    {
        uint8_t scheduler[60] = {0}, *before=malloc(8u*1024u*1024u), *after=malloc(8u*1024u*1024u);
        uint8_t *bad; const uint8_t *section; size_t section_size,n,m;
        semu_snapshot *saved=semu_snapshot_create(&e);
        semu_run_limits limits={2u,1000000u}; unsigned count=cb.count;
        SEMU_TEST_ASSERT(context,before&&after&&saved);
        SEMU_TEST_EQ_U64(context,SEMU_STOP_BUDGET,semu_machine_run(second,&limits,&e));
        semu_surface_clear(b->surface,0xabcdu);
        b->baseline_valid=1;b->shadow_fresh=1;b->guest_span_base=0x10001000u;
        b->guest_span[0]=0x5au;b->baseline->pixels[0]=0xff123456u;
        b->tsc6a->pixels[0]=0xff654321u;
        SEMU_TEST_EQ_U64(context,SEMU_OK,semu_machine_snapshot_save(second,saved,&e));
        n=semu_snapshot_serialize(saved,before,8u*1024u*1024u);
        SEMU_TEST_ASSERT(context,n>0);
        SEMU_TEST_EQ_U64(context,SEMU_OK,semu_snapshot_read_section(snap,
            SEMU_SNAPSHOT_SECTION_DISPLAY,&section,&section_size));
        bad=malloc(section_size);SEMU_TEST_ASSERT(context,bad);
        memcpy(bad,section,section_size);bad[0]^=1u;
        SEMU_TEST_EQ_U64(context,SEMU_OK,semu_snapshot_write_section(snap,
            SEMU_SNAPSHOT_SECTION_DISPLAY,bad,section_size,&e));
        SEMU_TEST_EQ_U64(context,SEMU_ERR_CONFLICT,semu_machine_snapshot_load(second,snap,&e));
        SEMU_TEST_EQ_U64(context,SEMU_OK,semu_machine_snapshot_save(second,saved,&e));
        m=semu_snapshot_serialize(saved,after,8u*1024u*1024u);
        SEMU_TEST_ASSERT(context,n==m&&memcmp(before,after,n)==0);
        SEMU_TEST_EQ_U64(context,count,cb.count);
        SEMU_TEST_EQ_U64(context,SEMU_OK,semu_machine_snapshot_save(first,snap,&e));
        /* Valid scheduler encoding, but an orphan SysTick event fails link
         * validation AFTER the renderer has loaded. All components roll back. */
        scheduler[8]=1u;scheduler[16]=2u;scheduler[24]=1u;
        scheduler[28]=1u;scheduler[44]=1u;scheduler[52]=SEMU_SCHED_EVENT_SYSTICK;
        SEMU_TEST_EQ_U64(context,SEMU_OK,semu_snapshot_write_section(snap,
            SEMU_SNAPSHOT_SECTION_SCHEDULER,scheduler,sizeof(scheduler),&e));
        SEMU_TEST_ASSERT(context,semu_machine_snapshot_load(second,snap,&e)!=SEMU_OK);
        SEMU_TEST_EQ_U64(context,SEMU_OK,semu_machine_snapshot_save(second,saved,&e));
        m=semu_snapshot_serialize(saved,after,8u*1024u*1024u);
        SEMU_TEST_ASSERT(context,n==m&&memcmp(before,after,n)==0);
        SEMU_TEST_EQ_U64(context,count,cb.count);
        /* A backend may run without persistence, but cannot silently save. */
        o.display_snapshot=NULL;
        {
            semu_machine *missing=semu_machine_create(&o,&e);
            SEMU_TEST_ASSERT(context,missing);
            SEMU_TEST_EQ_U64(context,SEMU_ERR_UNSUPPORTED,semu_machine_snapshot_save(missing,saved,&e));
            m=semu_snapshot_serialize(saved,after,8u*1024u*1024u);
            SEMU_TEST_ASSERT(context,n==m&&memcmp(before,after,n)==0);
            semu_machine_destroy(missing);
        }
        free(before);free(after);free(bad);semu_snapshot_destroy(saved);
    }
    semu_machine_destroy(first);semu_machine_destroy(second);
    semu_snapshot_destroy(snap);semu_bus_destroy(bus);
    semu_nema_backend_destroy(a);semu_nema_backend_destroy(b);(void)remove(path);
}

#define BASE 0x10000000u
static void write_words(semu_bus *bus, uint32_t offset,
    const uint32_t *words, size_t count)
{
    semu_error e; size_t i;
    for (i=0; i<count; ++i)
        (void)semu_bus_write(bus,BASE+offset+(uint32_t)i*4u,4u,words[i],&e);
}
static const uint32_t quad[] = {
    NEMA_REG_TEX0_BASE,BASE+2048u,NEMA_REG_TEX0_FSTRIDE,0x040001e0u,
    NEMA_REG_TEX0_RESXY,0x00f000f0u,NEMA_REG_CLIPMIN,0u,NEMA_REG_CLIPMAX,0x10001u,
    NEMA_REG_POINT0_X,0u,NEMA_REG_POINT0_Y,0u,
    NEMA_REG_POINT1_X,0x10000u,NEMA_REG_POINT1_Y,0u,
    NEMA_REG_POINT2_X,0x10000u,NEMA_REG_POINT2_Y,0x10000u,
    NEMA_REG_POINT3_X,0u,NEMA_REG_POINT3_Y,0x10000u,
    NEMA_REG_DRAW_COLOR,0x001fu,NEMA_REG_DRAW_CMD,NEMA_DRAW_QUAD
};
static void same_image(semu_test_context *context, semu_nema_backend *a,
    semu_nema_backend *b)
{
    uint8_t *x=NULL,*y=NULL; size_t nx=0,ny=0; semu_error e;
    SEMU_TEST_EQ_U64(context,SEMU_OK,semu_nema_backend_snapshot_ops.save(a,&x,&nx,&e));
    SEMU_TEST_EQ_U64(context,SEMU_OK,semu_nema_backend_snapshot_ops.save(b,&y,&ny,&e));
    SEMU_TEST_EQ_U64(context,nx,ny);
    SEMU_TEST_ASSERT(context,memcmp(x,y,nx)==0);
    free(x);free(y);
}
static void test_renderer_inline_and_inherited_continuation(semu_test_context *context)
{
    semu_error e; semu_bus *bus=semu_bus_create(&e);
    semu_nema_backend *a=semu_nema_backend_create(&e),*b=semu_nema_backend_create(&e);
    capture ca={0},cb={0}; uint8_t *data=NULL; size_t size=0;
    const uint32_t red[]={NEMA_REG_DRAW_COLOR,0xf800u,NEMA_REG_DRAW_CMD,NEMA_DRAW_QUAD};
    const uint32_t inherited[]={NEMA_REG_DRAW_CMD,NEMA_DRAW_QUAD};
    semu_display_list lists[]={{BASE,SEMU_ARRAY_LEN(quad),0u},
        {BASE+256u,SEMU_ARRAY_LEN(red),SEMU_DISPLAY_LIST_INLINE}};
    SEMU_TEST_ASSERT(context,bus&&a&&b);
    SEMU_TEST_EQ_U64(context,SEMU_OK,semu_bus_map_ram(bus,"commands",BASE,4096u,&e));
    write_words(bus,0,quad,SEMU_ARRAY_LEN(quad));write_words(bus,256u,red,SEMU_ARRAY_LEN(red));
    SEMU_TEST_EQ_U64(context,SEMU_TRANSACTION_OK,semu_nema_backend_ops.prepare(a,bus,
        lists,2u,0u,frame,&ca,&e));
    semu_nema_backend_ops.commit(a);
    SEMU_TEST_EQ_U64(context,1u,ca.count);
    SEMU_TEST_EQ_U64(context,0x1fu,a->published_pixels[0]);
    SEMU_TEST_EQ_U64(context,0xf8u,semu_nema_backend_frame(a)->pixels[1]);
    SEMU_TEST_EQ_U64(context,SEMU_OK,semu_nema_backend_snapshot_ops.save(a,&data,&size,&e));
    SEMU_TEST_EQ_U64(context,SEMU_OK,semu_nema_backend_snapshot_ops.load(b,data,size,&e));
    same_image(context,a,b);
    write_words(bus,512u,inherited,SEMU_ARRAY_LEN(inherited));
    SEMU_TEST_EQ_U64(context,SEMU_TRANSACTION_OK,semu_nema_backend_submit(a,bus,BASE+512u,2u,0,frame,&ca,&e));
    SEMU_TEST_EQ_U64(context,SEMU_TRANSACTION_OK,semu_nema_backend_submit(b,bus,BASE+512u,2u,0,frame,&cb,&e));
    SEMU_TEST_EQ_U64(context,ca.crc,cb.crc);
    SEMU_TEST_EQ_U64(context,2u,cb.generation);
    same_image(context,a,b);
    free(data);semu_bus_destroy(bus);semu_nema_backend_destroy(a);semu_nema_backend_destroy(b);
}
static void shadow_continuation(semu_test_context *context, int mid_frame)
{
    semu_error e; semu_bus *bus=semu_bus_create(&e);
    semu_nema_backend *a=semu_nema_backend_create(&e),*b=semu_nema_backend_create(&e);
    uint8_t *data=NULL; size_t size=0; capture ca={0},cb={0};
    const uint32_t shadow[]={NEMA_REG_TEX0_FSTRIDE,TSC6A_TARGET_FSTRIDE,
        NEMA_REG_TEX0_RESXY,TSC6A_RESOLUTION,NEMA_REG_MATMULT,TSC6A_OBSERVED_MATMULT,
        NEMA_REG_CODEPTR,TSC6A_OBSERVED_CODE,NEMA_REG_DRAW_COLOR,0xff55ff00u,
        NEMA_REG_DRAW_CMD,NEMA_DRAW_TRI_AA};
    const uint32_t resolve[]={NEMA_REG_TEX0_FSTRIDE,TSC6A_RGB_FSTRIDE,
        NEMA_REG_TEX0_RESXY,TSC6A_RGB_RESOLUTION,NEMA_REG_TEX1_BASE,BASE+2048u,
        NEMA_REG_TEX1_FSTRIDE,TSC6A_TARGET_FSTRIDE_SAMPLED,NEMA_REG_TEX1_RESXY,TSC6A_RESOLUTION,
        NEMA_REG_MATMULT,0u,NEMA_REG_CODEPTR,TSC6A_RESOLVE_CODE,
        NEMA_REG_IMEM_ADDR,TSC6A_IMEM_ADDRESS,NEMA_REG_IMEM_DATAH,TSC6A_IMEM_DATAH,
        NEMA_REG_IMEM_DATAL,TSC6A_IMEM_DATAL,NEMA_REG_TEX_COLOR,0xff55ff00u,
        NEMA_REG_MM00,0x3f800000u,NEMA_REG_MM01,0u,NEMA_REG_MM02,0u,
        NEMA_REG_MM10,0u,NEMA_REG_MM11,0x3f800000u,NEMA_REG_MM12,0u,
        NEMA_REG_DRAW_CMD,NEMA_DRAW_TSC6A_RESOLVE};
    SEMU_TEST_ASSERT(context,bus&&a&&b);
    SEMU_TEST_EQ_U64(context,SEMU_OK,semu_bus_map_ram(bus,"commands",BASE,0x40000u,&e));
    write_words(bus,0,quad,SEMU_ARRAY_LEN(quad));
    write_words(bus,256u,shadow,SEMU_ARRAY_LEN(shadow));
    write_words(bus,512u,resolve,SEMU_ARRAY_LEN(resolve));
    SEMU_TEST_EQ_U64(context,SEMU_TRANSACTION_OK,semu_nema_backend_submit(a,bus,BASE,SEMU_ARRAY_LEN(quad),0,frame,&ca,&e));
    SEMU_TEST_EQ_U64(context,SEMU_TRANSACTION_OK,semu_nema_backend_submit(a,bus,BASE+256u,SEMU_ARRAY_LEN(shadow),0,frame,&ca,&e));
    SEMU_TEST_ASSERT(context,a->tsc6a->pixels[0]!=0);
    if (!mid_frame) {
        SEMU_TEST_EQ_U64(context,SEMU_TRANSACTION_OK,semu_nema_backend_submit(a,bus,BASE+512u,SEMU_ARRAY_LEN(resolve),0,frame,&ca,&e));
    }
    SEMU_TEST_EQ_U64(context,SEMU_OK,semu_nema_backend_snapshot_ops.save(a,&data,&size,&e));
    SEMU_TEST_EQ_U64(context,SEMU_OK,semu_nema_backend_snapshot_ops.load(b,data,size,&e));
    same_image(context,a,b);
    if (!mid_frame) {
        SEMU_TEST_EQ_U64(context,SEMU_TRANSACTION_OK,semu_nema_backend_submit(a,bus,BASE+256u,SEMU_ARRAY_LEN(shadow),0,frame,&ca,&e));
        SEMU_TEST_EQ_U64(context,SEMU_TRANSACTION_OK,semu_nema_backend_submit(b,bus,BASE+256u,SEMU_ARRAY_LEN(shadow),0,frame,&cb,&e));
    }
    SEMU_TEST_EQ_U64(context,SEMU_TRANSACTION_OK,semu_nema_backend_submit(a,bus,BASE+512u,SEMU_ARRAY_LEN(resolve),0,frame,&ca,&e));
    SEMU_TEST_EQ_U64(context,SEMU_TRANSACTION_OK,semu_nema_backend_submit(b,bus,BASE+512u,SEMU_ARRAY_LEN(resolve),0,frame,&cb,&e));
    SEMU_TEST_EQ_U64(context,ca.crc,cb.crc);same_image(context,a,b);
    free(data);semu_bus_destroy(bus);semu_nema_backend_destroy(a);semu_nema_backend_destroy(b);
}
static void test_renderer_completed_shadow_continuation(semu_test_context *context)
{ shadow_continuation(context, 0); }
static void test_renderer_mid_frame_continuation(semu_test_context *context)
{ shadow_continuation(context, 1); }
static void test_renderer_refusals_are_atomic(semu_test_context *context)
{
    semu_error e; semu_bus *bus=semu_bus_create(&e);
    semu_nema_backend *a=semu_nema_backend_create(&e),*b=semu_nema_backend_create(&e);
    uint8_t *data=NULL,*bad,*out; size_t size=0,outsize; capture c={0}; size_t i;
    const size_t offsets[]={0u,4u,8u,12u,31u,180u+NEMA_BACKEND_PANEL_BYTES};
    semu_display_list list={BASE,SEMU_ARRAY_LEN(quad),0u};
    SEMU_TEST_ASSERT(context,bus&&a&&b);
    SEMU_TEST_EQ_U64(context,SEMU_OK,semu_nema_backend_snapshot_ops.save(a,&data,&size,&e));
    bad=malloc(size+1u);SEMU_TEST_ASSERT(context,bad);
    for(i=0;i<SEMU_ARRAY_LEN(offsets);++i) {
        memcpy(bad,data,size);bad[offsets[i]]=0xffu;
        SEMU_TEST_ASSERT(context,semu_nema_backend_snapshot_ops.load(b,bad,size,&e)!=SEMU_OK);
        same_image(context,a,b);
    }
    SEMU_TEST_EQ_U64(context,SEMU_ERR_FORMAT,semu_nema_backend_snapshot_ops.load(b,data,size-1u,&e));
    SEMU_TEST_EQ_U64(context,SEMU_ERR_FORMAT,semu_nema_backend_snapshot_ops.load(b,bad,size+1u,&e));
    same_image(context,a,b);
    SEMU_TEST_EQ_U64(context,SEMU_OK,semu_bus_map_ram(bus,"commands",BASE,4096u,&e));
    write_words(bus,0,quad,SEMU_ARRAY_LEN(quad));
    SEMU_TEST_EQ_U64(context,SEMU_TRANSACTION_OK,semu_nema_backend_ops.prepare(b,bus,&list,1u,0u,frame,&c,&e));
    out=data;outsize=123u;
    SEMU_TEST_EQ_U64(context,SEMU_ERR_CONFLICT,semu_nema_backend_snapshot_ops.save(b,&out,&outsize,&e));
    SEMU_TEST_ASSERT(context,out==data);SEMU_TEST_EQ_U64(context,123u,outsize);
    SEMU_TEST_EQ_U64(context,SEMU_ERR_CONFLICT,semu_nema_backend_snapshot_ops.load(b,data,size,&e));
    semu_nema_backend_ops.commit(b);SEMU_TEST_EQ_U64(context,1u,c.count);
    SEMU_TEST_EQ_U64(context,0x1fu,semu_nema_backend_frame(b)->pixels[0]);
    SEMU_TEST_EQ_U64(context,SEMU_OK,semu_nema_backend_reset(b));
    SEMU_TEST_ASSERT(context,semu_nema_backend_snapshot_ops.published_frame(b)==NULL);
    free(data);data=NULL;
    SEMU_TEST_EQ_U64(context,SEMU_OK,semu_nema_backend_snapshot_ops.save(b,&data,&size,&e));
    SEMU_TEST_EQ_U64(context,SEMU_OK,semu_nema_backend_snapshot_ops.load(a,data,size,&e));
    SEMU_TEST_ASSERT(context,semu_nema_backend_snapshot_ops.published_frame(a)==NULL);
    same_image(context,a,b);
    free(data);free(bad);semu_bus_destroy(bus);semu_nema_backend_destroy(a);semu_nema_backend_destroy(b);
}
int main(void)
{
    const semu_test_case cases[]={SEMU_TEST_CASE(test_machine_restores_published_renderer),
        SEMU_TEST_CASE(test_renderer_inline_and_inherited_continuation),
        SEMU_TEST_CASE(test_renderer_completed_shadow_continuation),
        SEMU_TEST_CASE(test_renderer_mid_frame_continuation),
        SEMU_TEST_CASE(test_renderer_refusals_are_atomic)};
    return semu_test_run(cases,SEMU_ARRAY_LEN(cases));
}
