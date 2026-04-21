#include "time_manager.h"

#include <time.h>

void time_manager_init(TimeEventCb cb) { (void)cb; }
TimeState time_manager_get_state(void) { return TIME_STATE_SYNCED; }
void time_manager_resync(void) {}
void time_manager_poll(void) {}

bool time_manager_get_time(struct tm* timeinfo) {
    time_t now = time(NULL);
    localtime_r(&now, timeinfo);
    return true;
}
