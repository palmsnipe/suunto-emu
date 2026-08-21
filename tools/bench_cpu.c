/*
 * CPU throughput benchmark for suunto-emu.
 *
 * Builds synthetic Thumb workloads and measures instructions/second
 * through the core interpreter.  Output is a markdown table suitable
 * for saving to docs/benchmark-results.md.
 *
 * Usage:
 *   build/bench_cpu             run all workloads, print to stdout
 *   build/bench_cpu --write     also write results to docs/benchmark-results.md
 *
 * Build: make bench
 */

#include "semu/bus.h"
#include "semu/cpu.h"
#include "semu/scheduler.h"
#include "semu/types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define BENCH_INSTRUCTIONS 20000000u
#define IMAGE_SIZE 0x1000u

typedef struct bench_result {
    const char *name;
    uint64_t instructions;
    double seconds;
    double mips;
    uint32_t final_r0;
    uint32_t final_pc;
} bench_result;

static double now_seconds(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static void put16(uint8_t *dst, uint16_t value)
{
    dst[0] = (uint8_t)(value & 0xffu);
    dst[1] = (uint8_t)(value >> 8u);
}

static void put32(uint8_t *dst, uint32_t value)
{
    dst[0] = (uint8_t)value;
    dst[1] = (uint8_t)(value >> 8u);
    dst[2] = (uint8_t)(value >> 16u);
    dst[3] = (uint8_t)(value >> 24u);
}

/*
 * All workloads share the same vector-table layout:
 *   [0x0000] initial SP = 0x1000
 *   [0x0004] initial PC = 0x0011  (Thumb bit set)
 * Program code starts at 0x0010.
 */

/*
 * Workload A: tight ALU loop (2 instructions/iteration).
 *   loop: ADDS r0, r0, #1
 *         B    loop
 */
static void build_alu_tight(uint8_t *image)
{
    memset(image, 0, IMAGE_SIZE);
    put32(image + 0x0000, 0x1000u);
    put32(image + 0x0004, 0x0011u);
    put16(image + 0x0010, 0x2000u);  /* MOVS r0, #0 */
    put16(image + 0x0012, 0x3001u);  /* ADDS r0, r0, #1 */
    put16(image + 0x0014, 0xE7FDu);  /* B 0x0012 */
}

/*
 * Workload B: mixed ALU (7 instructions/iteration).
 *   MOVS r0,#0
 *   loop: ADDS r0,#1 / SUBS r0,#1 / ADDS r1,#1 / SUBS r1,#1
 *         ADDS r2,#1 / SUBS r2,#1 / B loop
 */
static void build_alu_mixed(uint8_t *image)
{
    memset(image, 0, IMAGE_SIZE);
    put32(image + 0x0000, 0x1000u);
    put32(image + 0x0004, 0x0011u);
    uint8_t *p = image + 0x0010;
    put16(p + 0,  0x2000u);  /* MOVS r0, #0 */
    put16(p + 2,  0x3001u);  /* ADDS r0, r0, #1 */
    put16(p + 4,  0x3801u);  /* SUBS r0, r0, #1 */
    put16(p + 6,  0x3091u);  /* ADDS r1, r1, #1 */
    put16(p + 8,  0x3891u);  /* SUBS r1, r1, #1 */
    put16(p + 10, 0x3111u);  /* ADDS r2, r2, #1 */
    put16(p + 12, 0x3911u);  /* SUBS r2, r2, #1 */
    put16(p + 14, 0xE7F8u);  /* B 0x0012 */
}

/*
 * Workload C: branch-heavy (3 instructions/iteration, taken BNE + B).
 *   MOVS r0, #0
 *   loop: ADDS r0, r0, #1
 *         BNE +0          ; skip MOVS when r0 != 0
 *         MOVS r0, #0     ; (skipped when BNE taken)
 *         B loop
 */
static void build_branch_mixed(uint8_t *image)
{
    memset(image, 0, IMAGE_SIZE);
    put32(image + 0x0000, 0x1000u);
    put32(image + 0x0004, 0x0011u);
    uint8_t *p = image + 0x0010;
    put16(p + 0,  0x2000u);  /* MOVS r0, #0 */
    put16(p + 2,  0x3001u);  /* ADDS r0, r0, #1 */
    put16(p + 4,  0xD100u);  /* BNE +0 (branch to next instr when Z=0) */
    put16(p + 6,  0x2000u);  /* MOVS r0, #0 (skipped) */
    put16(p + 8,  0xE7FBu);  /* B 0x0012 */
}

/*
 * Workload D: memory load/store loop (3 instructions/iteration).
 *   MOVS r0, #0
 *   MOVS r1, #0x20    ; scratch in RAM at 0x20 (beyond code)
 *   loop:
 *     STR  r0, [r1]
 *     LDR  r0, [r1]
 *     ADDS r0, r0, #1
 *     B    loop
 */
static void build_memory_loop(uint8_t *image)
{
    memset(image, 0, IMAGE_SIZE);
    put32(image + 0x0000, 0x1000u);
    put32(image + 0x0004, 0x0011u);
    uint8_t *p = image + 0x0010;
    put16(p + 0,  0x2000u);  /* MOVS r0, #0 */
    put16(p + 2,  0x2010u);  /* MOVS r1, #0x20 */
    put16(p + 4,  0x6008u);  /* STR  r0, [r1] */
    put16(p + 6,  0x6808u);  /* LDR  r0, [r1] */
    put16(p + 8,  0x3001u);  /* ADDS r0, r0, #1 */
    put16(p + 10, 0xE7FBu);  /* B 0x0014 */
}

/*
 * Workload E: register shuffling + data movement (stress register pipeline).
 *   MOVS r0, #1
 *   MOVS r1, #2
 *   loop:
 *     ADDS r2, r0, r1   ; 0x4081
 *     MOV  r0, r1       ; 0x4011
 *     MOV  r1, r2       ; 0x4112
 *     ADDS r2, r2, #1   ; 0x30A1
 *     B    loop         ; 0xE7FA
 */
static void build_reg_shuffle(uint8_t *image)
{
    memset(image, 0, IMAGE_SIZE);
    put32(image + 0x0000, 0x1000u);
    put32(image + 0x0004, 0x0011u);
    uint8_t *p = image + 0x0010;
    put16(p + 0,  0x2001u);  /* MOVS r0, #1 */
    put16(p + 2,  0x2102u);  /* MOVS r1, #2 */
    put16(p + 4,  0x4081u);  /* ADDS r2, r0, r1 */
    put16(p + 6,  0x4011u);  /* MOV  r0, r1 */
    put16(p + 8,  0x4112u);  /* MOV  r1, r2 */
    put16(p + 10, 0x30A1u);  /* ADDS r2, r2, #1 */
    put16(p + 12, 0xE7FAu);  /* B 0x0014 */
}

static int run_workload(const char *name, const uint8_t *program,
                        uint64_t num_instructions, bench_result *result)
{
    semu_bus *bus;
    semu_scheduler *scheduler;
    semu_cpu *cpu;
    semu_error error;
    double t0, t1;

    semu_error_clear(&error);
    bus = semu_bus_create(&error);
    if (bus == NULL) {
        fprintf(stderr, "bench: bus create failed: %s\n", error.text);
        return 0;
    }
    if (semu_bus_map_ram(bus, "bench-ram", 0u, 0x10000u, &error) != SEMU_OK) {
        fprintf(stderr, "bench: map ram failed: %s\n", error.text);
        semu_bus_destroy(bus);
        return 0;
    }
    if (semu_bus_load(bus, 0u, program, IMAGE_SIZE, &error) != SEMU_OK) {
        fprintf(stderr, "bench: load failed: %s\n", error.text);
        semu_bus_destroy(bus);
        return 0;
    }

    scheduler = semu_scheduler_create(&error);
    if (scheduler == NULL) {
        fprintf(stderr, "bench: scheduler create failed\n");
        semu_bus_destroy(bus);
        return 0;
    }

    cpu = semu_cpu_create(bus, scheduler, &error);
    if (cpu == NULL) {
        fprintf(stderr, "bench: cpu create failed: %s\n", error.text);
        semu_scheduler_destroy(scheduler);
        semu_bus_destroy(bus);
        return 0;
    }

    semu_error_clear(&error);
    semu_cpu_reset(cpu, 0u, &error);

    /* Warm up: run 10000 instructions to fill any caches */
    for (uint64_t i = 0; i < 10000u; i++) {
        semu_error_clear(&error);
        semu_status st = semu_cpu_step(cpu, &error);
        if (st != SEMU_OK) {
            fprintf(stderr, "bench: %s: warmup step %llu failed: %s\n",
                    name, (unsigned long long)(i + 1u), error.text);
            semu_cpu_destroy(cpu);
            semu_scheduler_destroy(scheduler);
            semu_bus_destroy(bus);
            return 0;
        }
    }

    /*
     * Run the timed phase multiple times from the same reset state and keep
     * the fastest (lowest wall-clock time).  Wall-clock is host-dependent,
     * so the best-of-N is the most reproducible figure; the Final PC/R0 are
     * identical across trials because each starts from the same reset state.
     */
    double best_seconds = 0.0;
    for (int trial = 0; trial < 5; trial++) {
        semu_error_clear(&error);
        semu_cpu_reset(cpu, 0u, &error);
        t0 = now_seconds();
        for (uint64_t i = 0; i < num_instructions; i++) {
            semu_error_clear(&error);
            semu_status st = semu_cpu_step(cpu, &error);
            if (st != SEMU_OK) {
                fprintf(stderr, "bench: %s: trial %d step %llu failed: %s\n",
                        name, trial + 1,
                        (unsigned long long)(i + 1u), error.text);
                semu_cpu_destroy(cpu);
                semu_scheduler_destroy(scheduler);
                semu_bus_destroy(bus);
                return 0;
            }
        }
        t1 = now_seconds();
        if (trial == 0 || (t1 - t0) < best_seconds) {
            best_seconds = t1 - t0;
        }
    }

    const semu_cpu_state *state = semu_cpu_get_state(cpu);
    result->name = name;
    result->instructions = state->instructions;
    result->seconds = best_seconds;
    result->mips = (best_seconds > 0.0)
                       ? ((double)state->instructions / best_seconds) / 1e6
                       : 0.0;
    result->final_r0 = state->r[0];
    result->final_pc = state->r[15];

    semu_cpu_destroy(cpu);
    semu_scheduler_destroy(scheduler);
    semu_bus_destroy(bus);
    return 1;
}

int main(int argc, char **argv)
{
    int write_file = (argc >= 2 && strcmp(argv[1], "--write") == 0);
    uint8_t *image = (uint8_t *)calloc(1u, IMAGE_SIZE);
    bench_result results[5];
    int count = 0;

    if (image == NULL) {
        fprintf(stderr, "bench: out of memory\n");
        return 1;
    }

    build_alu_tight(image);
    if (!run_workload("alu-tight", image, BENCH_INSTRUCTIONS,
                      &results[count])) {
        free(image);
        return 1;
    }
    count++;

    build_alu_mixed(image);
    if (!run_workload("alu-mixed", image, BENCH_INSTRUCTIONS,
                      &results[count])) {
        free(image);
        return 1;
    }
    count++;

    build_branch_mixed(image);
    if (!run_workload("branch-mixed", image, BENCH_INSTRUCTIONS,
                      &results[count])) {
        free(image);
        return 1;
    }
    count++;

    build_memory_loop(image);
    if (!run_workload("mem-loop", image, BENCH_INSTRUCTIONS,
                      &results[count])) {
        free(image);
        return 1;
    }
    count++;

    build_reg_shuffle(image);
    if (!run_workload("reg-shuffle", image, BENCH_INSTRUCTIONS,
                      &results[count])) {
        free(image);
        return 1;
    }
    count++;

    printf("# CPU Benchmark Results\n\n");
    printf("| Workload | Instructions | Time (s) | MIPS | Final PC | R0 |\n");
    printf("|----------|-------------:|---------:|-----:|---------:|---:|\n");
    for (int i = 0; i < count; i++) {
        const bench_result *r = &results[i];
        printf("| %s | %llu | %.3f | %.2f | 0x%08x | 0x%08x |\n",
               r->name, (unsigned long long)r->instructions, r->seconds,
               r->mips, r->final_pc, r->final_r0);
    }
    printf("\n");

    if (write_file) {
        FILE *fp = fopen("docs/benchmark-results.md", "w");
        if (fp == NULL) {
            fprintf(stderr, "bench: cannot open docs/benchmark-results.md\n");
            free(image);
            return 1;
        }
        fprintf(fp, "# CPU Benchmark Results\n\n");
        fprintf(fp,
                "Generated by `tools/bench_cpu.c`.\n\n"
                "Workloads execute synthetic Thumb programs through the\n"
                "interpreter's `semu_cpu_step` and measure wall-clock time\n"
                "for a fixed instruction budget. Lower time is better.\n\n");
        fprintf(fp,
                "| Workload | Instructions | Time (s) | MIPS | Final PC | R0 |\n");
        fprintf(fp,
                "|----------|-------------:|---------:|-----:|---------:|---:|\n");
        for (int i = 0; i < count; i++) {
            const bench_result *r = &results[i];
            fprintf(fp, "| %s | %llu | %.3f | %.2f | 0x%08x | 0x%08x |\n",
                    r->name, (unsigned long long)r->instructions,
                    r->seconds, r->mips, r->final_pc, r->final_r0);
        }
        fclose(fp);
        printf("Wrote docs/benchmark-results.md\n");
    }

    free(image);
    return 0;
}
