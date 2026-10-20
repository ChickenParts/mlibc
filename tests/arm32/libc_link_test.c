/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <pthread.h>
#if ARM32_SHARED_TEST
#include <dlfcn.h>
#endif
static __thread int local_tls=42;
static int initialized;
__attribute__((constructor)) static void setup(void) { initialized=1; }
/* Force the real pthread creation glue into the static link as well. */
int (*volatile thread_api)(pthread_t *, const pthread_attr_t *, void *(*)(void *), void *)=pthread_create;
static int callback(int x) { return x+1; }
int main(void) {
 if(!initialized || local_tls++!=42 || !thread_api) return 1;
#if ARM32_SHARED_TEST
 void *library=dlopen("libarm32-test.so",RTLD_NOW|RTLD_LOCAL);
 if(!library) { puts(dlerror()); return 2; }
 int (*call)(int (*)(int),int)=dlsym(library,"arm32_shared_call");
 if(!call || call(callback,10)!=16 || call(callback,10)!=17) return 3;
 if(dlclose(library)) return 4;
#else
 if(callback(10)!=11) return 5;
#endif
 puts("ARM32_LIBC_OK");
 return 0;
}
