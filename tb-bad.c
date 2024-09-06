#define SIMULATOR_IMPLEMENTATION
#include "simulator.h"

typedef enum {
    DG_DROP,
    DG_PASS,
} E_DG_Verdict;

typedef struct {
    s64 tokens;
    s64 cap;
    s64 last_timestamp;
    s64 packet_cost;

} Bucket;

E_DG_Verdict dg_tb_consume_or_refill(Bucket* tb, s64 now_ns) {
    // доступ без атомарного чтения не проблема, потому-что в конечном итоге
    // всё синхронизируется через последующие атомарные операции.
    if (tb->tokens < tb->packet_cost) {
        s64 last_ts = tb->last_timestamp;
        s64 old_tokens = tb->tokens;

        if (last_ts >= now_ns) {
            return DG_DROP;
        }

        s64 elapsed = now_ns - last_ts;
        s64 tokens = old_tokens + elapsed;
        if (tokens >= tb->packet_cost) {
            tokens -= tb->packet_cost;
        }
        if (__sync_bool_compare_and_swap(&tb->last_timestamp, last_ts, now_ns)) {
            if (__sync_bool_compare_and_swap(&tb->tokens, old_tokens, tokens)) {
                if (old_tokens + elapsed >= tb->packet_cost) {
                    return DG_PASS;
                } else {
                    return DG_DROP;
                }
            }
        }
    }
    if (tb->tokens >= tb->packet_cost && __sync_add_and_fetch(&tb->tokens, -tb->packet_cost) > tb->packet_cost) {
        return DG_PASS;
    }
    return DG_DROP;
}

/// SIMULATOR interface implementation
void dg_debug_print(void* bucket, char* buf, u64 buf_size) {
    Bucket* tb = (Bucket*)bucket;
    snprintf(
        buf, buf_size,
        "{ts=%ld cap=%ld packet_cost=%ld}",
        tb->last_timestamp,
        tb->tokens / tb->packet_cost,
        tb->packet_cost
    );
}

bool dg_consume(void* bucket, u64 now_ns) {
    Bucket* tb = bucket;
    E_DG_Verdict verdict = dg_tb_consume_or_refill(tb, now_ns);
    return verdict == DG_PASS;
}

void dg_set_rate(void* bucket, u64 rate) {
    Bucket* b = bucket;
    s64 burst = b->cap / MAX(b->packet_cost, 1);
    b->packet_cost = ONE_SEC_NS / (s64)rate;
    b->cap = b->packet_cost * burst;
}

void dg_set_burst(void* bucket, u64 burst) {
    Bucket* b = bucket;
    b->cap = b->packet_cost * (s64)burst;
}

void* dg_init_bucket() {
    Bucket* b = calloc(1, sizeof(Bucket));
    b->last_timestamp = time_now_ns();
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
