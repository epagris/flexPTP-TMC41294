/* (C) András Wiesner, 2020 */

#ifndef TIMEUTILS_H
#define TIMEUTILS_H

#include <stdint.h>
#include <stdbool.h>

// timestamp (unsigned)
struct TimestampU
{
    uint64_t sec;
    uint32_t nanosec;
};

// timestamp (signed)
struct TimestampI
{
    int64_t sec;
    int32_t nanosec;
};

#define NANO_PREFIX (1000000000)
#define NANO_PREFIX_F (1000000000.0f)

// TIME OPERATIONS
struct TimestampI *tsUToI(struct TimestampI *ti, struct TimestampU *tu); // convert unsigned timestamp to signed
struct TimestampI *addTime(struct TimestampI *r, struct TimestampI *a, struct TimestampI *b); // sum timestamps (r = a + b)
struct TimestampI *subTime(struct TimestampI *r, struct TimestampI *a, struct TimestampI *b); // substract timestamps (r = a - b)
struct TimestampI *divTime(struct TimestampI *r, struct TimestampI *a, int divisor); // divide time value by an integer
uint64_t nsU(struct TimestampU *t); // convert unsigned time into nanoseconds
int64_t nsI(struct TimestampI *t); // convert signed time into nanoseconds
void normTime(struct TimestampI * t); // normalize time
int64_t tsToTick(struct TimestampI * ts, uint32_t tps); // convert time to hardware ticks, tps: ticks per second
struct Timestamp *nsToTsI(struct TimestampI *r, int64_t ns); // convert nanoseconds to time
bool nonZeroI(struct TimestampI *a); // does the timestamp differ from zero?

#endif /* TIMEUTILS_H */

