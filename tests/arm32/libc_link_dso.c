/* SPDX-License-Identifier: MIT */
static __thread int shared_tls=5;
int arm32_shared_call(int (*callback)(int),int value) {
 return callback(value)+shared_tls++;
}
