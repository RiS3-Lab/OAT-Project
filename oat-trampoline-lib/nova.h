#ifndef __NOVA_H_
#define __NOVA_H_
#include <stdint.h>
#include <stdbool.h>

void __record_defevt(uint64_t addr, uint64_t val);
void __check_useevt(uint64_t addr, uint64_t val);
void __collect_loop_hints(int fid, int level, int count);
void __collect_cond_branch_hints(bool cond);
void __collect_icall_hints(uint64_t fid, uint64_t count, uint64_t func);
void __collect_ibranch_hints(uint64_t fid, uint64_t count, uint64_t target);

void __cfv_icall(uint64_t target);
void __cfv_ijmp(uint64_t target);
void __cfv_ret(uint64_t target);

#endif
