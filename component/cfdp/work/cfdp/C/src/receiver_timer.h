#ifndef TIMER_H
#define TIMER_H

#include "transaction_id.h"

struct cfdp_core;

struct receiver_timer {
	struct cfdp_core *core;
	struct transaction_id transaction_id;
	int timeout;

	void (*timer_restart)(const int timeout,
			      void expired(struct receiver_timer *));
	void (*timer_stop)();
};

void receiver_timer_restart(struct receiver_timer *timer);
void receiver_timer_stop(struct receiver_timer *timer);
void receiver_timer_expired(struct receiver_timer *timer);

#endif