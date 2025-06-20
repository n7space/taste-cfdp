#include "cfdp_core.h"
#include "receiver_timer.h"

void receiver_timer_restart(struct receiver_timer *timer)
{
	timer->timer_stop();
	timer->timer_restart(timer->timeout, receiver_timer_expired);
}

void receiver_timer_stop(struct receiver_timer *timer) { timer->timer_stop(); }

void receiver_timer_expired(struct receiver_timer *timer)
{
	cfdp_core_issue_request(timer->core, timer->transaction_id,
				E27_INACTIVITY_TIMEOUT);
}