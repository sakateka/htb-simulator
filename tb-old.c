#define SIMULATOR_IMPLEMENTATION
#include "simulator.h"

// Tokens per one packet
#define DG_TB_PACKET_COST 100
// Nano seconds to santi (1/100 s) seconds divider
#define DG_TO_SANTI_DIV (u64)10e6
#define DG_TB_PACK_TIME_AND_LIMIT(time, limit) ((time << 32) | DG_TB_PACKET_COST * limit)

struct dg_bucket {
    s32 tokens;
    u32 last_time;
};

struct dg_tb_value {
    u64 time_and_tokens;
    u32 rate;
    u32 burst;
    u32 max_time_slice;
};

static __always_inline s32 dg_tb_consume_or_refill(u64* tb, u32 now, u32 rate, u32 burst, u32 max_time_slice) {
    struct dg_bucket* b = (struct dg_bucket*)tb;
    s32 tokens = b->tokens;
    if (tokens >= DG_TB_PACKET_COST) {
        tokens = __sync_add_and_fetch(&b->tokens, -DG_TB_PACKET_COST);
        if (tokens >= DG_TB_PACKET_COST) {
            return tokens;
        }
    }
    u64 packed = *tb;
    u64 old_packed = packed;
    b = (struct dg_bucket*)&packed;
    if (now > b->last_time) {
        s32 elapsed = now - b->last_time;
        s32 add = MIN(elapsed, max_time_slice) * rate;
        b->last_time = now;
        b->tokens = MIN(MAX(b->tokens, 0) + add, burst);
    } else if (b->last_time - now > 1<<31) {
        // time slice turned over, this happens once every ~1.3 years of uptime
        b->last_time = now;
        b->tokens = DG_TB_PACKET_COST * rate;
    }
    if (__sync_bool_compare_and_swap(tb, old_packed, packed)) {
        return b->tokens;
    }
    return 0;
}

/// SIMULATOR interface implementation
void dg_debug_print(void* bucket, char* buf, u64 buf_size) {
    struct dg_tb_value* tb = bucket;
    struct dg_bucket* b = (struct dg_bucket*)&tb->time_and_tokens;
    snprintf(
        buf, buf_size,
        "{time=%d, tokens=%d, rate=%u, burst=%u, max_time_slice=%u}",
        b->last_time, b->tokens, tb->rate, tb->burst, tb->max_time_slice
    );
}

bool dg_consume(void* bucket, u64 now_ns) {
    struct dg_tb_value* tb = bucket;
    s32 tokens = dg_tb_consume_or_refill(
        &tb->time_and_tokens,
        (u32)(now_ns/DG_TO_SANTI_DIV),
        tb->rate,
        tb->burst,
        tb->max_time_slice
    );
    return tokens >= DG_TB_PACKET_COST;
}

void dg_set_rate(void* bucket, u64 rate) {
    struct dg_tb_value* b = bucket;
    b->rate = rate;
    b->max_time_slice = MAX(DG_TB_PACKET_COST * rate, b->burst) / MAX(rate, 1);
}

void dg_set_burst(void* bucket, u64 burst) {
    struct dg_tb_value* b = bucket;
    b->burst = DG_TB_PACKET_COST * burst;
    b->max_time_slice = MAX(DG_TB_PACKET_COST * b->rate, b->burst) / MAX(b->rate, 1);
}

void* dg_init_bucket() {
    struct dg_tb_value* b = calloc(1, sizeof(struct dg_tb_value));
    b->time_and_tokens = DG_TB_PACK_TIME_AND_LIMIT(1lu, 1);
    dg_set_rate(b, 1);
    dg_set_burst(b, 1);
    return b;
}

int main() {
    u64 count = 4;
    spawn_simulator(
        count,
        dg_init_bucket,
        dg_set_rate,
        dg_set_burst,
        dg_consume,
        dg_debug_print
    );
}
