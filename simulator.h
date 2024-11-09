#pragma once
#include <stdio.h>
#include <errno.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <ctype.h>
#ifndef __USE_GNU
    #define _GNU_SOURCE
    #define __USE_GNU
#endif
#include <pthread.h>

#include <stdint.h>
typedef uint64_t u64;
typedef uint32_t u32;
typedef int64_t s64;
typedef double f64;
typedef float f32;
typedef int32_t s32;

#define ONE_SEC_NS (u64)1e9
#ifndef MIN
    #define MIN(X, Y) ((X) < (Y) ? (X) : (Y))
#endif
#ifndef MAX
    #define MAX(X, Y) ((X) > (Y) ? (X) : (Y))
#endif

// Public interface
typedef void* (*Initializer)();
typedef void (*Setter)(void* bucket, u64 value);
typedef bool (*Consumer)(void* bucket, u64 now_ns);
typedef void (*Formatter)(void* bucket, char* buf, u64 buf_size);

void spawn_simulator(u64 count, Initializer init, Setter rate, Setter burst, Consumer consume, Formatter print);
u64 time_now_ns();

///////////////////////////////////////////////////////////////////////////////

//#define SIMULATOR_IMPLEMENTATION
#ifdef SIMULATOR_IMPLEMENTATION

#define GRANULARITY 8
#define MAX_THREADS 100

#define TH_CONSUMER (char*)"consumer"
#define TH_TUNER    (char*)"tuner"
#define TH_METRICS  (char*)"metrics"
#define TH_MAIN     (char*)"main"
#define handle_error_en(en, msg) \
       do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0)

#define COLOR_RED     "\x1b[1;31m"
#define COLOR_GREEN   "\x1b[1;32m"
#define COLOR_GRAY    "\x1b[38;5;8m"
#define COLOR_YELLOW  "\x1b[1;33m"
#define COLOR_BLUE    "\x1b[1;34m"
#define COLOR_MAGENTA "\x1b[1;35m"
#define COLOR_CYAN    "\x1b[1;36m"
#define COLOR_RESET   "\x1b[0m"

__thread char* LOGGER_NAME = NULL;

void* query_routine(void* arg);
void set_thread_name(pthread_t id, char* name, int item_idx, u64 thread_idx);
char* trim_whitespace(char *str);
void thread_logger_name(char* color, char* name);
void sim_log(const char *fmt, ...);

typedef struct {
    u64 idx;
    u32 debug_state;
    u32 debug_rps;
    u64 threads;
    u64 pass;
    u64 drop;
    f64 consume;
    void* bucket;
} Simulator_Item;

typedef struct {
    bool main_thread;
    bool run;
    u64 count;
    Simulator_Item* items;

    // ops
    Initializer init_bucket;
    Setter set_rate;
    Setter set_burst;
    Consumer do_consume;
    Formatter do_format;
} Simulator_Payload;

typedef struct {
    char key[8];
    s64 val;
} Tuner_Param;

typedef struct Thread_List {
    Simulator_Payload* payload;
    pthread_t* th_handler;
    u64 count;
} Thread_List;

u64 time_now_ns() {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return now.tv_sec * ONE_SEC_NS + now.tv_nsec;
}

void* tuner_routine(void* arg) {
    thread_logger_name(COLOR_GREEN, TH_TUNER);
    Simulator_Payload* payload = (Simulator_Payload*)arg;
    char *line = NULL;
    size_t len = 0;
    ssize_t nread;

    Thread_List* thread_lists = (Thread_List*)calloc(payload->count, sizeof(Thread_List));
    for (int i = 0; i < payload->count; i++) {

        thread_lists[i].payload = (Simulator_Payload*)calloc(MAX_THREADS, sizeof(Simulator_Payload));
        thread_lists[i].th_handler = (pthread_t*)calloc(MAX_THREADS, sizeof(pthread_t));

    }
    while ((nread = getline(&line, &len, stdin)) != -1) {
        char* command = trim_whitespace(line);
        if (strlen(command) == 0 || command[0] == '#') continue; // skip empty and comments line
        command = strtok(command, "#"); // strip comments
        char mode[4] = {0};
        int ret;
        Tuner_Param params[4] = {
            {.key = {0}, .val = -1},
            {.key = {0}, .val = -1},
            {.key = {0}, .val = -1},
            {.key = {0}, .val = -1},
        };
        f32 num_arg = -1;

        if ((ret = sscanf(command, " %3s %f %8[a-z]=%ld %8[a-z]=%ld %8[a-z]=%ld %8[a-z]=%ld ",
                          mode, &num_arg,
                          params[0].key, &params[0].val,
                          params[1].key, &params[1].val,
                          params[2].key, &params[2].val,
                          params[3].key, &params[3].val
                          )) <= 0) {
            if (ret < 0 && ret != EOF) {
                perror("sscanf");
            } else {
                goto PARSE_FAILED;
            }
            continue;
        }
        if (strstr(mode, "set") != NULL) {
            int item_idx = (int)num_arg;
            if (item_idx < 0 || item_idx >= payload->count) {
                sim_log("bucket index out of range [0..%zd)", payload->count);
                goto PARSE_FAILED;
            }
            for (int i = 0; i < sizeof(params)/sizeof(Tuner_Param); i++) {
                Tuner_Param* param = &params[i];
                if (param->val < 0) {
                    continue;
                }
                Simulator_Item* item = &payload->items[item_idx];

                if (strstr(param->key, "rate")) {
                    sim_log("OK set tb[%d].rate = %ld", item_idx, param->val);
                    payload->set_rate(item->bucket, param->val);
                } else if (strstr(param->key, "burst")) {
                    sim_log("OK set tb[%d].burst = %ld", item_idx, param->val);
                    payload->set_burst(item->bucket, param->val);
                } else if (strstr(param->key, "consume")) {
                    sim_log("OK set tb[%d].consume = %ld", item_idx, param->val);
                    item->consume = (f64)param->val;
                } else if (strstr(param->key, "thread")) {
                    if (param->val <= 0 || param->val > MAX_THREADS) {
                        sim_log("INGORE tb[%d].thread = %ld (min=1 max=100)", item_idx, param->val);
                        continue;
                    }
                    Thread_List* th_list = &thread_lists[item_idx];
                    s64 add_thread = param->val - 1;
                    sim_log("OK set tb[%d].thread = %ld (diff=%ld)", item_idx, param->val, add_thread-(s64)th_list->count);
                    if (th_list->count == add_thread) {
                        continue;
                    }
                    int spawned = 0;
                    for (;(s64)th_list->count < add_thread;) {
                        Simulator_Payload* new_payload = &th_list->payload[th_list->count];
                        *new_payload = *payload;
                        new_payload->main_thread = false;
                        new_payload->run = true;
                        new_payload->items = &new_payload->items[item_idx];
                        int rc = pthread_create(&th_list->th_handler[th_list->count], NULL, query_routine, new_payload);
                        if (rc != 0) handle_error_en(rc, "pthread_create");

                        set_thread_name(th_list->th_handler[th_list->count], TH_CONSUMER, item_idx, th_list->count+1);

                        spawned++;
                        th_list->count++;
                        // sim_log("OK new consumer thread spawned (idx=%lu)", th_list->count);
                    }
                    if (spawned > 0) {
                        sim_log(COLOR_GRAY "OK spawned %d new consumer threads", spawned);
                    }

                    int terminated = 0;
                    for (;(s64)th_list->count > add_thread;) {
                        int idx = th_list->count - 1;
                        th_list->payload[idx].run = false;
                        //sim_log("OK cancel consumer thread (idx=%d)", idx);
                        pthread_cancel(th_list->th_handler[idx]);
                        //sim_log("OK join consumer thread (idx=%d)", idx);
                        pthread_join(th_list->th_handler[idx], NULL);
                        terminated++;
                        th_list->count--;

                    }
                    if (terminated > 0) {
                        sim_log(COLOR_GRAY "OK %d consumer threads terminated", terminated);
                    }
                    item->threads = th_list->count;

                } else if (strlen(param->key) > 0) {
                    sim_log(COLOR_RED "IGNORE unknown param '%s'=%ld for bucket %d", param->key, param->val, item_idx);
                }
            }

        } else if (strstr(mode, "dbg")) {
            int item_idx = (int)num_arg;
            if (item_idx < 0 || item_idx >= payload->count) {
                sim_log(COLOR_GRAY "OK debug all buckets");
                for (int i = 0; i < payload->count; i++) {
                    payload->items[i].debug_state = 1;
                    payload->items[i].debug_rps = 1;
                }
            } else {
                payload->items[item_idx].debug_state = 1;
                payload->items[item_idx].debug_rps = 1;
            }
        } else if (strstr(mode, "slp")) {
            u64 sleep_time_us = (u64)(num_arg * 1000.0) * 1000;
            if (sleep_time_us <= 0) goto PARSE_FAILED;
            sim_log(COLOR_GRAY "OK going to sleep %0.3f seconds", num_arg);
            usleep(sleep_time_us);
        } else if (strstr(mode, "mrk")) {
            sim_log(COLOR_CYAN "OK set marker %0.1f", num_arg);
            fprintf(stdout, "marker=0;marker=%0.1f;marker=0\n", num_arg);
        } else {
            goto PARSE_FAILED;
        }
        continue;
PARSE_FAILED:
        sim_log(
            COLOR_YELLOW "Failed to parse '%s'" COLOR_RESET "\nUSAGE\n"
            COLOR_GREEN " set" COLOR_RESET " tb-idx [rate=%%d|burst=%%d|consume=%%d|thread=%%d ...] - set simulation properties\n"
            COLOR_GREEN " dbg" COLOR_RESET " [tb-idx] - debug print bucket 'tb-idx' or all\n"
            COLOR_GREEN " slp" COLOR_RESET " %%f - sleep N seconds (ex: 0.2) before applying next simulator command\n"
            COLOR_GREEN " mrk" COLOR_RESET " %%f - marker sends metric=0;... N;... 0 value to the 'marker' key",
            line
        );
    }
    sim_log("Tuner loop terminated");
    exit(EXIT_SUCCESS);
    return NULL;
}

void* query_routine(void* arg) {
    thread_logger_name(COLOR_YELLOW, TH_CONSUMER);
    Simulator_Payload* payload = (Simulator_Payload*)arg;

    Simulator_Item* tb = payload->items;

    f64 amount = 0.0;
    u64 prev_ns = time_now_ns();
    for (;payload->run;) {
        if (payload->main_thread && tb->debug_state) {
            tb->debug_state = 0;

            char buf[255] = {0};
            payload->do_format(tb->bucket, buf, sizeof(buf));
            sim_log(
                "DBG: tb[%lu]%s - consume=%.2lfrps pass=%lu drop=%lu threads=%lu",
                tb->idx, buf, tb->consume, tb->pass, tb->drop, 1 + tb->threads
            );
        }

        if (usleep(1)) {
            break;
        };
        u64 now_ns = time_now_ns();
        s64 tdiff = now_ns - prev_ns;
        if (tdiff <= 0) {
            continue;
        }
        prev_ns = now_ns;
        amount += tb->consume * (tdiff / (f64)ONE_SEC_NS);
        if (amount > 0) {
            for (size_t i = 0; i < floor(amount); i++) {
                if (!payload->run) break;
                if (payload->do_consume(tb->bucket, now_ns)) {
                    __sync_add_and_fetch(&tb->pass, 1);
                } else {
                    __sync_add_and_fetch(&tb->drop, 1);
                }
            }
            amount -= floor(amount);
        }
    }
    return NULL;
}

void* output_routine(void* arg) {
    thread_logger_name(COLOR_MAGENTA, TH_METRICS);
    Simulator_Payload* htb = (Simulator_Payload*)arg;
    u64* pass = (u64*)calloc(GRANULARITY * htb->count, sizeof(u64));
    u64* drops = (u64*)calloc(GRANULARITY * htb->count, sizeof(u64));
    size_t k = 0;

    struct timespec fraction = {.tv_sec = 0, .tv_nsec = ONE_SEC_NS / GRANULARITY};
    struct timespec remaining = {0};
    struct timespec to_sleep = fraction;
    for (;;) {
        if (nanosleep(&to_sleep, &remaining)) {
            break;
        };
        if (remaining.tv_nsec > 0) {
            to_sleep = remaining;
            continue;
        }
        to_sleep = fraction;


        bool log_total = 0;
        u64 total_pass = 0;
        u64 total_drop = 0;
        for (int i = 0; i < htb->count; i++) {
            u64 j = htb->count*k+i;
            u64 curr_pass = htb->items[i].pass - pass[j];
            pass[j] = htb->items[i].pass;
            u64 curr_drop = htb->items[i].drop - drops[j];
            drops[j] = htb->items[i].drop;
            total_pass += curr_pass;
            total_drop += curr_drop;

            fprintf(stdout, "bucket-%d-pass=%lu;\n", i, curr_pass);
            fprintf(stdout, "bucket-%d-drop=%lu;\n", i, curr_drop);
            if (htb->items[i].debug_rps) {
                log_total = 1;
                htb->items[i].debug_rps = 0;
                sim_log("DBG: bucket-%d-pass=%lurps bucket-%d-drop=%lurps",
                        i, curr_pass, i, curr_drop);
            }
        }
        fprintf(stdout, "total-pass=%lu;\n", total_pass);
        fprintf(stdout, "total-drop=%lu;\n", total_drop);
        fflush(stdout);
        k = (k+1) & (GRANULARITY - 1);
        if (log_total) {
            log_total = false;
            sim_log("DBG: total-pass=%lurps total-drop=%lurps", total_pass, total_drop);
        }
    }
    sim_log("output thread terminated");

    return NULL;
}

pthread_t spawn_tuner(Simulator_Payload* payload) {
    pthread_t tuner;
    int rc =pthread_create(&tuner, NULL, tuner_routine, payload);
    if (rc != 0) handle_error_en(rc, "pthread_create");
    set_thread_name(tuner, TH_TUNER, 0, 0);
    return tuner;
}

pthread_t spawn_metrics(Simulator_Payload* payload) {
    pthread_t output;
    int rc = pthread_create(&output, NULL, output_routine, payload);
    if (rc != 0) handle_error_en(rc, "pthread_create");
    set_thread_name(output, TH_METRICS, 0, 0);
    return output;
}

pthread_t* spawn_consumers(Simulator_Payload* payload) {
    pthread_t* consumers = (pthread_t*)calloc(payload->count, sizeof(pthread_t));
    for (int i = 0; i < payload->count; i++) {
        Simulator_Payload* consumer_payload = (Simulator_Payload*)calloc(1, sizeof(Simulator_Payload));
        *consumer_payload = *payload;
        consumer_payload->items = &payload->items[i];
        consumer_payload->main_thread = true;
        consumer_payload->run = true;

        int rc = pthread_create(&consumers[i], NULL, query_routine, consumer_payload);
        if (rc != 0) handle_error_en(rc, "pthread_create");
        set_thread_name(consumers[i], TH_CONSUMER, i, 0);
    }
    return consumers;
}

void set_thread_name(pthread_t id, char* name, int item_idx, u64 thread_idx) {
    char buf[255] = {0};
    snprintf(buf, 255, "%s-b%d-t%lu", name, item_idx, thread_idx);
    int rc = pthread_setname_np(id, buf);
    if (rc != 0) handle_error_en(rc, "pthread_setname_np");
}

void init_simulator(Simulator_Payload* payload) {
    Simulator_Item* items = (Simulator_Item*)calloc(payload->count, sizeof(Simulator_Item));
    for (int i = 0; i < payload->count; i++) {
        items[i].idx = i;
        items[i].bucket = payload->init_bucket();
    }
    payload->items = items;
}

void spawn_simulator(u64 count, Initializer init, Setter rate, Setter burst, Consumer consume, Formatter print) {
    thread_logger_name(COLOR_BLUE, TH_MAIN);

    Simulator_Payload payload = {
        .count = count,
        .init_bucket = init,
        .set_rate = rate,
        .set_burst = burst,
        .do_consume = consume,
        .do_format = print,
    };

    sim_log("INFO: initialize buckets");
    init_simulator(&payload);

    sim_log("INFO: spawn consumers");
    spawn_consumers(&payload);
    sim_log("INFO: spawn metrics reporter");
    spawn_metrics(&payload);
    sim_log("INFO: spawn tuner");
    pthread_t tuner = spawn_tuner(&payload);
    sim_log("INFO: going to join tuner thread");
    pthread_join(tuner, NULL);
}

// https://stackoverflow.com/questions/122616/how-do-i-trim-leading-trailing-whitespace-in-a-standard-way
char* trim_whitespace(char *str) {
  // Trim leading space
  while(isspace((unsigned char)*str)) str++;

  if(*str == 0)  // All spaces?
    return str;

  // Trim trailing space
  char* end = str + strlen(str) - 1;
  while(end > str && isspace((unsigned char)*end)) end--;

  // Write new null terminator character
  end[1] = '\0';

  return str;
}

void thread_logger_name(char* color, char* name) {
    int n = asprintf(&LOGGER_NAME, "%s%-8s%s", color, name, COLOR_RESET);
    (void)n;
}

u64 fmt_log_time(char* buf, u64 buf_size) {
    time_t t = time(NULL);
    struct tm *tmp = localtime(&t);
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    u64 n = strftime(buf, buf_size, "%FT%T.000", tmp);
    if (n == 0) {
        return 0;
    }
    f64 frac = (f64)now.tv_nsec/1e6;
    snprintf(buf + n - 3, 4, "%03.0f", frac);
    return n;
}

void sim_log(const char *fmt, ...) {
    char ts[32] = {0};
    fmt_log_time(ts, 32);
    char buf[512] = {0};
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, 512, fmt, args);
    va_end(args);
    fprintf(stderr, "%s: [%s] %s%s\n", ts, LOGGER_NAME, buf, COLOR_RESET);
}
#endif
