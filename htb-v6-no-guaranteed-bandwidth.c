#define SIMULATOR_IMPLEMENTATION
#include "simulator.h"

// WARNING:
// This modification allows us to remove one bucket, but..."
// There is no guaranteed bandwidth anymore!

typedef enum {
    DG_DROP,
    DG_PASS,
} E_DG_Verdict;

typedef struct Bucket {
    s64 edt;
    s64 packet_cost;
    s64 cap;
    s64 burst_cap;
} Bucket;

Bucket* global = NULL;

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
        return usage + packet_cost; // syncronize with upper level check
    }
    // If there is a good chance that the packet will be passed, we need to
    // advance tb->edt and recheck whether the current rate is within the limit.
    return __sync_fetch_and_add(edt, packet_cost) - (s64)elapsed;
}

static __always_inline E_DG_Verdict dg_tb_consume_or_refill(Bucket* tb, u64 elapsed, bool force_take) {
    s64 usage = dg_tb_consume_or_refill_impl(elapsed, &tb->edt, tb->cap, tb->packet_cost, force_take);
    if (usage > tb->cap) {
        return DG_DROP;
    }
    return DG_PASS;
}

static __always_inline E_DG_Verdict dg_htb_consume_or_refill(Bucket* tb, u64 elapsed) {
    s64 usage = dg_tb_consume_or_refill_impl(elapsed, &tb->edt, tb->burst_cap, tb->packet_cost, false);
    if (usage > tb->burst_cap) {
        return DG_DROP;
    }
    if (usage <= tb->cap) {
        dg_tb_consume_or_refill(global, elapsed, true);
        return DG_PASS;
    }
    E_DG_Verdict global_v = dg_tb_consume_or_refill(global, elapsed, false);
    if (global_v == DG_DROP) {
        // Return the burst token if the global limit doesn't allow bursting.
        __sync_fetch_and_add(&tb->edt, -tb->packet_cost);
    }
    return global_v;
}


/// SIMULATOR interface implementation
void dg_debug_print(void* bucket, char* buf, u64 buf_size) {
    Bucket* tb = (Bucket*)bucket;
    s64 now_ns = time_now_ns();
    snprintf(
        buf, buf_size,
        "{usage=%ldrps pc=%lu rate=%lurps cap=%lurps burst_cap=%lurps}",
        (tb->edt - (s64)now_ns) / (s64)tb->packet_cost,
        tb->packet_cost,
        ONE_SEC_NS / tb->packet_cost,
        tb->cap / tb->packet_cost,
        tb->burst_cap / tb->packet_cost
    );
}

bool dg_consume(void* bucket, u64 now_ns) {
    Bucket* tb = bucket;
    E_DG_Verdict verdict = dg_htb_consume_or_refill(tb, now_ns);
    return verdict == DG_PASS;
}

// heuristic metric - max of 0.1% or 10rps
s64 heuristic_capacity_for_htb_main_tb(u64 rate) {
    return MAX(10, (s64)rate / 1000);
}

void dg_set_rate(void* bucket, u64 rate) {
    Bucket* b = bucket;
    s64 burst = b->burst_cap / MAX(b->packet_cost, 1);
    b->packet_cost = ONE_SEC_NS / rate;
    b->cap = b->packet_cost * heuristic_capacity_for_htb_main_tb(rate);
    b->burst_cap = b->packet_cost * burst;
}

void dg_set_burst(void* bucket, u64 burst) {
    Bucket* b = bucket;
    b->cap = b->packet_cost * heuristic_capacity_for_htb_main_tb(ONE_SEC_NS / b->packet_cost);
    b->burst_cap = b->packet_cost * burst;
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
