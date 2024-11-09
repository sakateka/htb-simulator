#define SIMULATOR_IMPLEMENTATION
#include "simulator.h"

typedef enum {
    DG_DROP,
    DG_PASS,
} E_DG_Verdict;

typedef struct Bucket {
    s64 edt;
    u64 packet_cost;
    u64 cap;
} Bucket;

static __always_inline s64 dg_tb_consume_or_refill_impl(u64 elapsed, s64* edt, s64 capacity, s64 packet_cost, bool force_take) {
    s64 usage = *edt - (s64)elapsed;
    if (usage < 0) {
        // If the current rate is too low, we need to actualize tb->edt
        // in order to meet the burst constraints.
        __sync_bool_compare_and_swap(edt, *edt, elapsed + (u64)packet_cost);
        return 0;
    }
    if (!force_take && usage + packet_cost > capacity) {
        // If the current rate is too high just drop the packet without moving tb->edt
        return usage + packet_cost;
    }
    // If there is a good chance that the packet will be passed, we need to
    // advance tb->edt and recheck whether the current rate is within the limit.
    return __sync_fetch_and_add(edt, packet_cost) - (s64)elapsed;
}

static __always_inline E_DG_Verdict dg_tb_consume_or_refill(Bucket* tb, u64 elapsed) {
    s64 usage = dg_tb_consume_or_refill_impl(elapsed, &tb->edt, tb->cap, tb->packet_cost, false);
    if (usage > tb->cap) {
        return DG_DROP;
    }
    return DG_PASS;
}

/// SIMULATOR interface implementation
void dg_debug_print(void* bucket, char* buf, u64 buf_size) {
    Bucket* tb = (Bucket*)bucket;
    snprintf(
        buf, buf_size,
        "{usage=%ldrps pc=%lu rate=%lurps cap=%lurps}",
        (tb->edt - (s64)time_now_ns()) / (s64)tb->packet_cost,
        tb->packet_cost,
        ONE_SEC_NS / tb->packet_cost,
        tb->cap / tb->packet_cost
    );
}

bool dg_consume(void* bucket, u64 now_ns) {
    Bucket* tb = bucket;
    E_DG_Verdict verdict = dg_tb_consume_or_refill(tb, now_ns);
    return verdict == DG_PASS;
}

void dg_set_rate(void* bucket, u64 rate) {
    Bucket* b = bucket;
    u64 burst = b->cap / MAX(b->packet_cost, 1);
    b->packet_cost = ONE_SEC_NS / rate;
    b->cap = b->packet_cost * burst;
}

void dg_set_burst(void* bucket, u64 burst) {
    Bucket* b = bucket;
    b->cap = b->packet_cost * burst;
}

void* dg_init_bucket() {
    Bucket* b = calloc(1, sizeof(Bucket));
    b->edt = time_now_ns();
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
