/* SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "BLI_utildefines.h"

#include "MEM_guardedalloc.h"
#include "guardedalloc_test_base.h"

#define CHECK_ALIGNMENT(ptr, align) EXPECT_EQ(size_t(ptr) % align, 0)

namespace {

void DoBasicAlignmentChecks(const int alignment)
{
  int *foo, *bar;

  foo = (int *)MEM_mallocN_aligned(sizeof(int) * 10, alignment, "test");
  CHECK_ALIGNMENT(foo, alignment);

  bar = (int *)MEM_dupallocN(foo);
  CHECK_ALIGNMENT(bar, alignment);
  MEM_freeN(bar);

  foo = (int *)MEM_reallocN(foo, sizeof(int) * 5);
  CHECK_ALIGNMENT(foo, alignment);

  foo = (int *)MEM_recallocN(foo, sizeof(int) * 5);
  CHECK_ALIGNMENT(foo, alignment);

  MEM_freeN(foo);
}

}  // namespace


#define STACK_TRACE
#ifdef STACK_TRACE
#include "windows.h"
#  include "dbghelp.h"
#  pragma comment(lib, "DbgHelp.lib ")
#  define MAX_INFO 200000
#  define MAX_INFO_MSG 256

//#include <pthread.h>
// static pthread_mutex_t _thread_lock = ((pthread_mutex_t)(size_t)-1);

typedef struct MEM_info {
  uintptr_t ptr;
  bool is_free;
  size_t len;
  char msg[MAX_INFO_MSG];
} MEM_info;
static MEM_info *minfo;
static int minfo_alloc = 0;
static int minfo_id = -1;
static int push_cnt_id = 0;
static int pop_cnt_id = 0;

void MEM_StackInfo(void *ptr, const char *str, uint64_t len)
{
  // printStack();
#  if 1
  if (push_cnt_id == 0) {
    {
      minfo = (MEM_info *)malloc(MAX_INFO * sizeof(MEM_info));
      for (int j = 0; j < MAX_INFO; j++) {
        minfo[j].is_free = true;
        minfo[j].len = 0;
        minfo[j].ptr = 1;
      };
    };
  }

  push_cnt_id++;

  minfo_id = -1;
  for (int i = 0; i < MAX_INFO; i++) {
    if (minfo[i].is_free) {
      minfo_id = i;
      // push_cnt_id++;
      break;
    }
  }

  if (minfo_id == -1) {
    exit(-1);
  }

  minfo[minfo_id].ptr = (uintptr_t)ptr;
  minfo[minfo_id].len = len;
  minfo[minfo_id].is_free = false;
  unsigned int i;
#    define STACK_NUMS 30
  void *stack[STACK_NUMS];
  unsigned short frames;

  HANDLE process;

  process = GetCurrentProcess();

  SymInitialize(process, NULL, TRUE);
  memset(&stack, 0, sizeof(uintptr_t) * STACK_NUMS);

  frames = CaptureStackBackTrace(0, STACK_NUMS, stack, NULL);

  char *dst = &minfo[minfo_id].msg[0];
  memset(minfo[minfo_id].msg, 0, MAX_INFO_MSG);
  int total = 0;
  SYMBOL_INFO *symbol = (SYMBOL_INFO *)calloc(sizeof(SYMBOL_INFO) + 256 * sizeof(char), 1);
  symbol->MaxNameLen = 255;
  symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
  static int CALLEE_CNT = 0;
  CALLEE_CNT++;

  for (i = 0; i < frames; i++) {
    if (!stack[i])
      break;
    // mem_lock_thread();
    SymFromAddr(process, (DWORD64)(stack[i]), 0, symbol);
    // mem_unlock_thread();
    if (total + symbol->NameLen > MAX_INFO_MSG) {
      break;
    }
    memcpy(dst, symbol->Name, symbol->NameLen);
    dst[symbol->NameLen] = '\n';
    dst += (symbol->NameLen + 1);
    total += (symbol->NameLen + 1);
    if (strcmp(symbol->Name, "main") == 0) {
      break;
    }
    /*  printf("%i: %s - 0x%0X\n", frames - i - 1, symbol->Name, symbol->Address); */
  }
  free(symbol);

#  endif
}
void MEM_PopInfo(void *ptr)
{

  uintptr_t p = (uintptr_t)ptr;
  for (int i = 0; i < MAX_INFO; i++) {
    if (minfo[i].ptr == p) {
      minfo[i].is_free = true;
      minfo[i].ptr = 0;
      minfo[i].len = 0;
      pop_cnt_id++;
      return;
    }
  }
};

void MEM_PrintInfo()
{

  printf("Memory  push  %d   pop   %d   \n", push_cnt_id, pop_cnt_id);
  for (int i = 0; i < MAX_INFO; i++) {
    if (!minfo[i].is_free) {
      printf("Memory leak  found size  %zu  \n  %s    \n", minfo[i].len, minfo[i].msg);
    }
  }
  free(minfo);
  minfo_alloc = 0;
  minfo_id = -1;
  push_cnt_id = 0;
  pop_cnt_id = 0;
};
#else
void MEM_PrintInfo()
{
}
#endif

TEST_F(LockFreeAllocatorTest, MEM_mallocN_aligned)
{
  MEM_use_memleak_detection(true);
  DoBasicAlignmentChecks(1);
  DoBasicAlignmentChecks(2);
  DoBasicAlignmentChecks(4);
  DoBasicAlignmentChecks(8);
  DoBasicAlignmentChecks(16);
  DoBasicAlignmentChecks(32);
  DoBasicAlignmentChecks(256);
  DoBasicAlignmentChecks(512);
  MEM_PrintInfo();
}

TEST_F(GuardedAllocatorTest, MEM_mallocN_aligned)
{
  DoBasicAlignmentChecks(1);
  DoBasicAlignmentChecks(2);
  DoBasicAlignmentChecks(4);
  DoBasicAlignmentChecks(8);
  DoBasicAlignmentChecks(16);
  DoBasicAlignmentChecks(32);
  DoBasicAlignmentChecks(256);
  DoBasicAlignmentChecks(512);
}
