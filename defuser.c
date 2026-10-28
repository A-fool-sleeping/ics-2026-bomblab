#include "defuser.h"

#include <inttypes.h>
#include <stdio.h>

#define MAX_FIELDS 8

static int write_answer(char *out, size_t cap, const uint32_t *values, size_t count) {
    size_t used = 0;
    for (size_t i = 0; i < count; ++i) {
        int size = snprintf(out + used, cap - used, "%s%" PRIu32, i ? " " : "", values[i]);
        if (size < 0 || (size_t)size >= cap - used) return 1;
        used += (size_t)size;
    }
    return count == 0;
}

/* phase 1: 单值，challenge 拆三段后三步混合 */
int solve_phase_1(uint64_t challenge, char *out, size_t cap) {
    uint32_t values[MAX_FIELDS] = {0};
    size_t count = 0;
    uint32_t lo = (uint32_t)challenge;
    uint32_t hi = (uint32_t)(challenge >> 32);
    uint32_t mid = (uint32_t)(challenge >> 16);
    uint32_t t2 = (hi + 2u * lo) ^ UINT32_C(0x3b51);
    uint32_t t0 = ((mid ^ UINT32_C(0x9aeb)) + 4u * lo);
    values[count++] = (3u * t0) ^ (t2 + UINT32_C(0x38f6));
    return write_answer(out, cap, values, count);
}

/* phase 2: 6 项递推，a[i+1] = ((challenge>>(3+2i))&63 + a[i]) ^ 12*(i+1) */
int solve_phase_2(uint64_t challenge, char *out, size_t cap) {
    uint32_t values[MAX_FIELDS] = {0};
    size_t count = 0;
    uint32_t t = (uint32_t)(challenge >> 28);
    uint32_t v = (uint32_t)(uint8_t)(t + t) + UINT32_C(20);
    values[count++] = v;
    for (unsigned i = 0; i < 5; ++i) {
        uint32_t add = (uint32_t)(challenge >> (3 + 2 * i)) & UINT32_C(0x3f);
        v = (add + v) ^ (UINT32_C(12) * (i + 1));
        values[count++] = v;
    }
    return write_answer(out, cap, values, count);
}

/* phase 3: switch，选择子 = challenge & 7，输入第二项等于对应 case 的结果 */
static uint32_t p3_apply(uint32_t sel, uint32_t x) {
    switch (sel) {
    case 0: return (x + x * 4) + UINT32_C(0x5560);
    case 1: return (x ^ UINT32_C(0xfe35)) + (x >> 2);
    case 2: return (x + x) ^ UINT32_C(0x906d);
    case 3: return (x + x * 4) + UINT32_C(0x4d6b);
    case 4: return (x + x * 4) + UINT32_C(0x28b2c);
    case 5: return (x + x * 4) + UINT32_C(0x27303);
    case 6: return (x + x) ^ UINT32_C(0xab88);
    default: return (x ^ UINT32_C(0xf2a0)) + (x >> 2);
    }
}

int solve_phase_3(uint64_t challenge, char *out, size_t cap) {
    uint32_t values[MAX_FIELDS] = {0};
    size_t count = 0;
    uint32_t sel = (uint32_t)challenge & UINT32_C(7);
    uint32_t x = (uint32_t)((challenge >> 32) ^ (challenge >> 3));
    values[count++] = sel;
    values[count++] = p3_apply(sel, x);
    return write_answer(out, cap, values, count);
}

/* phase 4: 在 [0,127] 上二分递归，返回值按 5x^mid+c 折叠 */
static uint32_t p4_walk(uint32_t x, uint32_t lo, uint32_t hi, uint32_t c) {
    uint32_t mid = lo + ((hi - lo) >> 1);
    if (mid == x) return (x + UINT32_C(0x98)) ^ c;
    if (x < mid) return (p4_walk(x, lo, mid - 1, c) * 5u) ^ (mid + c) ^ UINT32_C(2);
    return (p4_walk(x, mid + 1, hi, c) * 5u) ^ (mid + c) ^ UINT32_C(0x1c);
}

int solve_phase_4(uint64_t challenge, char *out, size_t cap) {
    uint32_t values[MAX_FIELDS] = {0};
    size_t count = 0;
    uint32_t x = (uint32_t)challenge & UINT32_C(0x7f);
    uint32_t c = (uint32_t)((challenge >> 32) ^ (challenge >> 7));
    values[count++] = p4_walk(x, 0, UINT32_C(0x7f), c);
    return write_answer(out, cap, values, count);
}

/* phase 5: 链表遍历，起点由 challenge&7 决定，步数 = (challenge>>3)&3 */
struct p5_node { uint32_t a, b; int next; };

static const struct p5_node P5_NODES[12] = {
    {14601, 2388,  4}, { 7523, 2584, 11}, {63512,  560,  8}, {51381, 2872, -1},
    {26692,  787,  7}, {60023, 2166,  3}, { 2823, 1543,  1}, {15695,  100,  2},
    {34812, 1097,  5}, {33841,  281,  0}, {17134, 1342,  9}, {43617, 1869, 10},
};

static const unsigned P5_HEADS[8] = {6, 1, 11, 10, 9, 0, 4, 7};

int solve_phase_5(uint64_t challenge, char *out, size_t cap) {
    uint32_t values[MAX_FIELDS] = {0};
    size_t count = 0;
    int i = (int)P5_HEADS[challenge & 7];
    uint32_t base = (uint32_t)((challenge >> 6) ^ (challenge >> 32)) ^ UINT32_C(0xc075);
    uint32_t v = (base * 3u + P5_NODES[i].a) ^ P5_NODES[i].b;
    uint32_t last = P5_NODES[i].b;
    unsigned steps = (unsigned)((challenge >> 3) & UINT32_C(3));
    for (unsigned k = 0; k < steps; ++k) {
        i = P5_NODES[i].next;
        v = (v * 3u + P5_NODES[i].a) ^ P5_NODES[i].b;
        last = P5_NODES[i].b;
    }
    values[count++] = v;
    values[count++] = last;
    return write_answer(out, cap, values, count);
}

/* phase 6: 两次间接调用，索引分别是 challenge&7 和 (challenge>>3)&7 */
static uint32_t p6_op(unsigned which, uint32_t x) {
    switch (which) {
    case 0: return ((x << 2) | (x >> 30)) ^ UINT32_C(0x4ceb);
    case 1: return (x ^ UINT32_C(0xbb26)) + (x << 7);
    case 2: return ((x >> 5) ^ x) + UINT32_C(0x657c);
    case 3: return (x >> 2) | (x << 30);
    case 4: return (((x << 7) + UINT32_C(0x1ea8)) ^ (x * 7u));
    default: return (x + UINT32_C(0x6ae)) ^ (x >> 5);
    }
}

int solve_phase_6(uint64_t challenge, char *out, size_t cap) {
    static const unsigned P6_OPS[8] = {1, 3, 2, 3, 5, 0, 4, 3};
    uint32_t values[MAX_FIELDS] = {0};
    size_t count = 0;
    uint32_t x = (uint32_t)((challenge >> 6) ^ (challenge >> 32));
    uint32_t t = p6_op(P6_OPS[challenge & 7], x) ^ UINT32_C(0x1166);
    values[count++] = p6_op(P6_OPS[(challenge >> 3) & 7], t) + UINT32_C(0x4f7);
    return write_answer(out, cap, values, count);
}

int solve_secret(uint64_t challenge, uint64_t context, char *out, size_t cap) {
    uint32_t lo = (uint32_t)challenge, hi = (uint32_t)(challenge >> 32);
    uint32_t tlo = (uint32_t)context, thi = (uint32_t)(context >> 32);
    uint32_t cmix = hi ^ lo;
    uint32_t tmix = thi ^ tlo;
    uint32_t pick = tlo & UINT32_C(3);
    uint32_t x = cmix ^ tmix;
    uint32_t mix = ((x >> 3) ^ UINT32_C(0xe036)) + (x ^ UINT32_C(0xf5ef));
    uint32_t v0 = pick;
    uint32_t v1 = mix ^ ((UINT32_C(0xf5ef) * pick) ^ (uint32_t)(context >> 16));
    uint32_t v2 = v1 ^ (UINT32_C(0xe036) + tmix + pick + (cmix >> 3));
    char prefix[33];
    for (int i = 0; i < 32; ++i) prefix[i] = '.';
    prefix[32] = 0;
    int n = snprintf(out, cap, "%sOPEN%" PRIu32 " %" PRIu32 " %" PRIu32,
                     prefix, v0, v1, v2);
    return n < 0 || (size_t)n >= cap;
}
