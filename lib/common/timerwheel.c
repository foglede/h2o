/*
 * Copyright (c) 2017 Fastly, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include <sys/time.h>
#include "h2o/memory.h"
#include "h2o/timerwheel.h"

static inline int clz(uint64_t n)
{
    H2O_BUILD_ASSERT(sizeof(unsigned long long) == 8);
    return __builtin_clzll(n);
}

/* debug macros and functions */
#ifdef WANT_DEBUG
#define WHEEL_DEBUG(fmt, args...)                                                                                                  \
    do {                                                                                                                           \
        fprintf(stdout, "[%s:%d %s]:" fmt, __FILE__, __LINE__, __FUNCTION__, ##args);                                              \
    } while (0)

#else
#define WHEEL_DEBUG(...)
#endif

static void h2o_timer_show(h2o_timerwheel_timer_t *timer)
{
    WHEEL_DEBUG("timer with expire_at %" PRIu64 "\n", timer->expire_at);
#ifdef TW_DEBUG_VERBOSE
    WHEEL_DEBUG("_link.next: %p\n", timer->_link.next);
    WHEEL_DEBUG("_link.prev: %p\n", timer->_link.prev);
    WHEEL_DEBUG("callback: %p\n", timer->cb);
#endif
}

static void h2o_timerwheel_slot_show(h2o_timerwheel_slot_t *slot)
{
    h2o_linklist_t *node;
    if (h2o_linklist_is_empty(slot))
        return;

    for (node = slot->next; node != slot; node = node->next) {
        h2o_timerwheel_timer_t *entry = H2O_STRUCT_FROM_MEMBER(h2o_timerwheel_timer_t, _link, node);
        h2o_timer_show(entry);
    }
}

void h2o_timerwheel_show(h2o_timerwheel_t *w)
{
    int i, slot;

    for (i = 0; i < H2O_TIMERWHEEL_MAX_WHEELS; i++) {
        for (slot = 0; slot < H2O_TIMERWHEEL_SLOTS_PER_WHEEL; slot++) {
            h2o_timerwheel_slot_t *s = &(w->wheel[i][slot]);
            h2o_timerwheel_slot_show(s);
        }
    }
}

/* timer APIs */
h2o_timerwheel_timer_t *h2o_timerwheel_create_timer(h2o_timerwheel_cb cb)
{
    h2o_timerwheel_timer_t *t = h2o_mem_alloc(sizeof(h2o_timerwheel_timer_t));
    memset((void *)t, 0, sizeof(struct st_h2o_timerwheel_timer_t));
    t->cb = cb;

    return t;
}

void h2o_timerwheel_init_timer(h2o_timerwheel_timer_t *t, h2o_timerwheel_cb cb)
{
    memset((void *)t, 0, sizeof(struct st_h2o_timerwheel_timer_t));
    t->cb = cb;
}

/* calculate wheel number base on the absolute expiration time */
static inline int timer_wheel(uint64_t abs_wtime, uint64_t abs_expire)
{
    uint64_t delta = (abs_wtime ^ abs_expire) & H2O_TIMERWHEEL_MAX_TIMER;
    return (H2O_TIMERWHEEL_SLOTS_MASK - clz(delta))/H2O_TIMERWHEEL_BITS_PER_WHEEL;
}

/* calculate slot number based on the absolute expiration time */
static inline int timer_slot(int wheel, uint64_t expire)
{
    return H2O_TIMERWHEEL_SLOTS_MASK & (expire >> wheel * H2O_TIMERWHEEL_BITS_PER_WHEEL);
}

static h2o_timerwheel_slot_t *compute_slot(h2o_timerwheel_t *w, h2o_timerwheel_timer_t *timer)
{
    h2o_timerwheel_slot_t *slot;
    uint64_t diff = timer->expire_at - w->last_run;
    if (diff < H2O_TIMERWHEEL_SLOTS_PER_WHEEL) {
        slot = &w->wheel[0][0] + (timer->expire_at & H2O_TIMERWHEEL_SLOTS_MASK);
    } else if (diff < 1 << (2*H2O_TIMERWHEEL_BITS_PER_WHEEL)) {
        slot = &w->wheel[1][0] + ((timer->expire_at >> H2O_TIMERWHEEL_BITS_PER_WHEEL) & H2O_TIMERWHEEL_SLOTS_MASK);
    } else if (diff < 1 << (3*H2O_TIMERWHEEL_BITS_PER_WHEEL)) {
        slot = &w->wheel[2][0] + ((timer->expire_at >> (2*H2O_TIMERWHEEL_BITS_PER_WHEEL)) & H2O_TIMERWHEEL_SLOTS_MASK);
    } else {
        slot = &w->wheel[3][0] + ((timer->expire_at >> (3*H2O_TIMERWHEEL_BITS_PER_WHEEL)) & H2O_TIMERWHEEL_SLOTS_MASK);
    }
    return slot;
}

int h2o_timerwheel_add_timer(h2o_timerwheel_t *w, h2o_timerwheel_timer_t *timer, uint64_t abs_expire)
{
    h2o_timerwheel_slot_t *slot;
    if (abs_expire - timer->expire_at > 0xffffffff) return -1;

    timer->expire_at = abs_expire;

    if (abs_expire <= w->last_run) {
        slot = &w->expired;
        WHEEL_DEBUG("timer (expired_at %" PRIu64 ") is added to expired queue\n", abs_expire);
    } else {
        int wid = timer_wheel(w->last_run, abs_expire);
        int sid = timer_slot(wid, abs_expire);
        slot = &(w->wheel[wid][sid]);

        WHEEL_DEBUG("timer(expire_at %" PRIu64 ") is added to wheel %d, slot %d\n",
            abs_expire, wid, sid);
    }

    h2o_linklist_insert(slot, &timer->_link);
    return 0;
}

void h2o_timerwheel_del_timer(h2o_timerwheel_timer_t *timer)
{
    if (h2o_linklist_is_linked(&timer->_link)) {
        h2o_linklist_unlink(&timer->_link);
    }
}

inline int h2o_timer_is_linked(h2o_timerwheel_timer_t *entry)
{
    return h2o_linklist_is_linked(&entry->_link);
}

/* timer wheel APIs */

/**
 * initializes a timerwheel
 */
void h2o_timerwheel_init(h2o_timerwheel_t *w)
{
    int i, j;
    memset(w, 0, sizeof(h2o_timerwheel_t));

    for (i = 0; i < H2O_TIMERWHEEL_MAX_WHEELS; i++) {
        for (j = 0; j < H2O_TIMERWHEEL_SLOTS_PER_WHEEL; j++) {
            h2o_linklist_init_anchor(&w->wheel[i][j]);
        }
    }
    h2o_linklist_init_anchor(&w->expired);
}

/**
 * cascading happens when the lower wheel wraps around and ticks the next
 * higher wheel
 */
static void cascade(h2o_timerwheel_t *w, int wheel, int slot)
{
    /* cannot cascade timers on wheel 0 */
    assert(wheel > 0);

    WHEEL_DEBUG("cascade timers on wheel %d slot %d\n", wheel, slot);
    h2o_timerwheel_slot_t *s = &w->wheel[wheel][slot];
    while (!h2o_linklist_is_empty(s)) {
        h2o_timerwheel_timer_t *entry = H2O_STRUCT_FROM_MEMBER(h2o_timerwheel_timer_t, _link, s->next);
        h2o_linklist_unlink(&entry->_link);
        h2o_timerwheel_add_timer(w, entry, entry->expire_at);
    }
}

size_t h2o_timerwheel_run(h2o_timerwheel_t *w, uint64_t now)
{
    int i, j, now_slot, prev_slot, end_slot;
    uint64_t abs_wtime = w->last_run;
    size_t count = 0;
    h2o_linklist_t todo;
    h2o_linklist_init_anchor(&todo);

    /* update the timestamp for the timerwheel */
    w->last_run = now;

    /* how the wheel is run: based on abs_wtime and now, we should be able
     * to figure out the wheel id on which most update happens. Most likely
     * the operating wheel is wheel 0 (wid == 0), since we optimize the case
     * where h2o_timerwheel_run() is called very frequently, i.e the gap
     * between abs_wtime and now is normally small. */
    int wid = timer_wheel(abs_wtime, now);
    WHEEL_DEBUG(" wtime %" PRIu64 ", now %" PRIu64 " wid %d\n", abs_wtime, now, wid);

    if (wid > 0) {
        /* now collect all the expired timers on wheels [0, wid-1] */
        for (j = 0; j <= wid; j++) {
            prev_slot = timer_slot(j, abs_wtime);
            now_slot = timer_slot(j, now);
            end_slot = j==wid? now_slot-1:H2O_TIMERWHEEL_SLOTS_MASK;
            /* all slots between prev_slot and end_slot are expired */
            for (i = prev_slot; i <= end_slot; i++) {
                h2o_linklist_insert_list(&todo, &w->wheel[j][i]);
            }
        }

        /* cascade the timers on wheel[wid][now_slot] */
        cascade(w, wid, now_slot);
    } else {
        prev_slot = timer_slot(0, abs_wtime);
        now_slot = timer_slot(0, now);
        for (i = prev_slot; i <= now_slot; i++) {
            h2o_linklist_insert_list(&todo, &w->wheel[0][i]);
        }
    }

    /* also add all the timers on w->expired slot to the todo list */
    h2o_linklist_insert_list(&todo, &w->expired);

    /* expiration processing */
    while (!h2o_linklist_is_empty(&todo)) {
        h2o_timerwheel_timer_t *timer = H2O_STRUCT_FROM_MEMBER(h2o_timerwheel_timer_t, _link, todo.next);
        /* remove this timer from todo list */
        h2o_linklist_unlink(&timer->_link);
        timer->cb(timer);
        count++;
    }

    return count;
}