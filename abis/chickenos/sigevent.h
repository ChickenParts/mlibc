#ifndef _ABIBITS_SIGEVENT_H
#define _ABIBITS_SIGEVENT_H

#include <abi-bits/sigval.h>

#define SIGEV_SIGNAL 0
#define SIGEV_NONE 1
#define SIGEV_THREAD 2

struct sigevent {
	union sigval sigev_value;
	int sigev_signo;
	int sigev_notify;
	union {
		char __pad[64 - 2 * sizeof(int) - sizeof(union sigval)];
		int _tid;
		struct {
			void (*_function)(union sigval);
			void *_attribute;
		} _sigev_thread;
	} __sigev_fields;
};

#define sigev_notify_function __sigev_fields._sigev_thread._function
#define sigev_notify_attributes __sigev_fields._sigev_thread._attribute

#endif /* _ABIBITS_SIGEVENT_H */
