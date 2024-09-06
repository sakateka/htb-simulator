#define SIMULATOR_IMPLEMENTATION
#include "simulator.h"

typedef enum {
    DG_DROP,
    DG_PASS,
} E_DG_Verdict;

typedef struct {
    u32 guard;
    s32 packet_cost;
    u64 ts;
    u64 cap;
    s64 value;
    s64 stage;
} Bucket;

static __always_inline E_DG_Verdict dg_tb_consume_or_refill(Bucket* tb, u64 now) {
    if (tb->value >= tb->packet_cost) {
         if (__sync_add_and_fetch(&tb->value, -tb->packet_cost) >= 0) {
             return DG_PASS;
         }
    }
    // critical section START
    if (__sync_lock_test_and_set(&tb->guard, 1) == 0) { // acquire the barrier
        if (now > tb->ts) {
            s64 add = MIN(now - tb->ts, tb->cap);
            tb->ts = now;

            add += tb->stage;

            if (tb->value < 0) {
                add += tb->packet_cost;
            } else if (tb->value > 0) {
                add += tb->value;
                tb->value = 0;
            }

            if (add >= tb->packet_cost) {
                tb->stage = 0;

                add = MIN(add, tb->cap) - tb->packet_cost;
                __sync_lock_test_and_set(&tb->value, add);
                __sync_lock_release(&tb->guard); // release
                // critical section END
                return DG_PASS;
            } else {
                tb->stage = add;
            }
        }
        __sync_lock_release(&tb->guard); // release
        // critical section END
    }
    return DG_DROP;
}

/// SIMULATOR interface implementation
void dg_debug_print(void* bucket, char* buf, u64 buf_size) {
    Bucket* tb = (Bucket*)bucket;
    snprintf(
        buf, buf_size,
        "{ts=%lu packet_cost=%u cap=%lu value=%ld stage=%ld}",
        tb->ts,
        tb->packet_cost,
        tb->cap,
        tb->value,
        tb->stage
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
    b->ts = time_now_ns();
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
