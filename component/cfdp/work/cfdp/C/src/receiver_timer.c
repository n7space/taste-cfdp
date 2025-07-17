#include "cfdp_core.h"
#include "receiver_timer.h"

void receiver_timer_restart(struct receiver_timer *timer)
{
	if (timer->timer_stop != NULL) {
		if (!timer->timer_stop(timer->timer_data)) {
			cfdp_core_issue_error(timer->core,
							      TIMER_ERROR, 0);
		}
	}

	if (timer->timer_restart != NULL) {
		if (!timer->timer_restart(timer->timer_data,
					  timer->timeout,
					  receiver_timer_expired)) {
			cfdp_core_issue_error(timer->core,
							      TIMER_ERROR, 0);
		}
	}
}

void receiver_timer_stop(struct receiver_timer *timer)
{
	if (timer->timer_stop != NULL) {
		if (!timer->timer_stop(timer->timer_data)) {
			cfdp_core_issue_error(timer->core,
							      TIMER_ERROR, 0);
		}
	}
}

void receiver_timer_expired(struct receiver_timer *timer)
{
	cfdp_core_issue_request(timer->core, timer->transaction_id,
				E27_INACTIVITY_TIMEOUT);
}