#ifndef __WBTV_CLOCK_HEADER
#define __WBTV_CLOCK_HEADER
struct WBTV_Time_t
{
    long long seconds;
    unsigned int fraction;
};

#ifdef WBTV_ADV_MODE
struct WBTV_Time_t WBTVClock_get_time();
extern unsigned long WBTVClock_error;
extern unsigned int WBTVClock_error_per_second;
#define WBTV_CLOCK_UNSYNCHRONIZED 4294967295
#define WBTV_CLOCK_HIGH_ERROR 4294967294
#define WBTVClock_invalidate() WBTVClock_error_per_second = 4294967294

#endif
#endif