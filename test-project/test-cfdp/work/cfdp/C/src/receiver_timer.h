#ifndef TIMER_H
#define TIMER_H

#include "transaction_id.h"

struct cfdp_core;

struct receiver_timer {
	struct cfdp_core *core;
	struct transaction_id transaction_id;
	// timeout user interpreted
	uint32_t timeout;

	void *timer_data;

	bool (*timer_restart)(void *timer_data, const uint32_t timeout,
			      void expired(struct receiver_timer *));
	bool (*timer_stop)(void *timer_data);
};

void receiver_timer_restart(struct receiver_timer *timer);
void receiver_timer_stop(struct receiver_timer *timer);
void receiver_timer_expired(struct receiver_timer *timer);

#endif