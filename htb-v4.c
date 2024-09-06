#define SIMULATOR_IMPLEMENTATION
#include "simulator.h"
// XXX does not work properly

typedef enum {
    DG_DROP,
    DG_PASS,
    DG_BORROW,
} E_DG_Verdict;

typedef struct Bucket {
    s64 edt;
    u64 packet_cost;
    u64 cap;
    u64 bb_ok;
} Bucket;

Bucket* global = NULL;
s64 global_tokens = 0;

static __always_inline E_DG_Verdict dg_tb_consume_or_refill(Bucket* tb, u64 elapsed) {
    s64 usage = tb->edt - (s64)elapsed;
    if (usage < 0) {
        // If the current rate is too low, we need to actualize tb->edt
        // in order to meet the burst constraints.
        s64 old_edt = tb->edt;
        if (__sync_bool_compare_and_swap(&tb->edt, tb->edt, elapsed + tb->packet_cost)) {
            usage = old_edt - (s64)elapsed;
            if (tb != global && usage < 0 && global_tokens < 1) {
                __sync_fetch_and_add(&global_tokens, -usage / (s64)tb->packet_cost);
            }
        }
        return DG_PASS;
    } else if (usage > (tb->cap - tb->packet_cost)) {
        // if there is are tokens pass the packet
        if (tb != global && __sync_fetch_and_add(&global_tokens, -1) > 0) {
            return DG_BORROW;
        }
        // If the current rate is too high just drop the packet without moving tb->edt
        return DG_DROP;
    }
    // If there is a good chance that the packet will be passed, we need to
    // advance tb->edt and recheck whether the current rate is within the limit.
    usage = __sync_fetch_and_add(&tb->edt, tb->packet_cost) - (s64)elapsed;
    if (usage > tb->cap) {
        // if there is are tokens pass the packet
        if (tb != global && __sync_fetch_and_add(&global_tokens, -1) > 0) {
            return DG_BORROW;
        }
        return DG_DROP;
    }
    return DG_PASS;
}

static __always_inline E_DG_Verdict dg_htb_consume_or_refill(Bucket* tb, u64 elapsed) {
    E_DG_Verdict my_v = dg_tb_consume_or_refill(tb, elapsed);
    E_DG_Verdict global_v = dg_tb_consume_or_refill(global, elapsed);
    if (my_v == DG_BORROW) {
        return global_v;
    }
    return my_v;
}

/// SIMULATOR interface implementation
void dg_debug_print(void* bucket, char* buf, u64 buf_size) {
    Bucket* tb = (Bucket*)bucket;
    snprintf(
        buf, buf_size,
        "{usage=%ldrps pc=%lu rate=%lurps cap=%lurps gt=%ld}",
        (tb->edt - (s64)time_now_ns()) / (s64)tb->packet_cost,
        tb->packet_cost,
        ONE_SEC_NS / tb->packet_cost,
        tb->cap / tb->packet_cost,
        global_tokens
    );
}

bool dg_consume(void* bucket, u64 now_ns) {
    Bucket* tb = bucket;
    E_DG_Verdict verdict = dg_htb_consume_or_refill(tb, now_ns);
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
    if (global == NULL) {
        global = b;
    }
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
