/* (C) András Wiesner, 2020 */

#include "timeutils.h"

// TimestampU -> TimestampI
struct TimestampI *tsUToI(struct TimestampI *ti, struct TimestampU *tu)
{
    ti->sec = (int64_t)tu->sec;
    ti->nanosec = (int32_t)tu->nanosec;
    return ti;
}

// r = a + b;
struct TimestampI *addTime(struct TimestampI *r, struct TimestampI *a, struct TimestampI *b)
{
    int64_t ns = nsI(a) + nsI(b);
    nsToTsI(r, ns);
    return r;
}

// r = a - b;
struct TimestampI *subTime(struct TimestampI *r, struct TimestampI *a, struct TimestampI *b)
{
    int64_t ns = nsI(a) - nsI(b);
    nsToTsI(r, ns);
    return r;
}

struct TimestampI *divTime(struct TimestampI *r, struct TimestampI *a, int divisor) {
    int64_t ns = a->sec * NANO_PREFIX + a->nanosec;
    ns = ns / divisor; // így a pontosság +-0.5ns
    r->sec = ns / NANO_PREFIX;
    r->nanosec = ns - r->sec * NANO_PREFIX;
    return r;
}

uint64_t nsU(struct TimestampU *t)
{
    return t->sec * NANO_PREFIX + t->nanosec;
}

int64_t nsI(struct TimestampI *t)
{
    return t->sec * NANO_PREFIX + t->nanosec;
}

void normTime(struct TimestampI * t) {
    uint32_t s = t->nanosec / NANO_PREFIX;
    t->sec += s;
    t->nanosec -= s * NANO_PREFIX;
}

int64_t tsToTick(struct TimestampI * ts, uint32_t tps) { 
    int64_t ns = ts->sec * NANO_PREFIX + ts->nanosec;
    int64_t ticks = (ns * tps) / NANO_PREFIX;
    return ticks;
}

struct Timestamp *nsToTsI(struct TimestampI *r, int64_t ns) {
    r->sec = ns / NANO_PREFIX;
    r->nanosec = ns % NANO_PREFIX;
    return r;
}

bool nonZeroI(struct TimestampI *a) {
    return a->sec != 0 || a->nanosec != 0;
}
