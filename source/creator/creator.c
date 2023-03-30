/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup creator
 */

#include <stdlib.h>
#include <string.h>

#ifdef WIN32
#  include "utfconv.h"
#  include <windows.h>
#endif

#if defined(WITH_TBB_MALLOC) && defined(_MSC_VER) && defined(NDEBUG)
#  pragma comment(lib, "tbbmalloc_proxy.lib")
#  pragma comment(linker, "/include:__TBB_malloc_proxy")
#endif

#include "MEM_guardedalloc.h"

#include "CLG_log.h"

#include "DNA_genfile.h"

#include "BLI_string.h"
#include "BLI_system.h"
#include "BLI_task.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

/* Mostly initialization functions. */
#include "BKE_appdir.h"
#include "BKE_blender.h"
#include "BKE_brush.h"
#include "BKE_cachefile.h"
#include "BKE_callbacks.h"
#include "BKE_context.h"
#include "BKE_cpp_types.h"
#include "BKE_global.h"
#include "BKE_gpencil_modifier.h"
#include "BKE_idtype.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_modifier.h"
#include "BKE_node.h"
#include "BKE_particle.h"
#include "BKE_shader_fx.h"
#include "BKE_sound.h"
#include "BKE_vfont.h"
#include "BKE_volume.h"

#ifndef WITH_PYTHON_MODULE
#  include "BLI_args.h"
#endif

#include "DEG_depsgraph.h"

#include "IMB_imbuf.h" /* For #IMB_init. */

#include "RE_engine.h"
#include "RE_texture.h"

#include "ED_datafiles.h"

#include "WM_api.h"
#include "WM_toolsystem.h"

#include "RNA_define.h"

#ifdef WITH_FREESTYLE
#  include "FRS_freestyle.h"
#endif

#include <signal.h>

#ifdef __FreeBSD__
#  include <floatingpoint.h>
#endif

#ifdef WITH_BINRELOC
#  include "binreloc.h"
#endif

#ifdef WITH_LIBMV
#  include "libmv-capi.h"
#endif

#ifdef WITH_CYCLES_LOGGING
#  include "CCL_api.h"
#endif

#ifdef WITH_SDL_DYNLOAD
#  include "sdlew.h"
#endif

#ifdef WITH_USD
#  include "usd.h"
#endif

#include "creator_intern.h" /* Own include. */

/* -------------------------------------------------------------------- */
/** \name Local Defines
 * \{ */

/* When building as a Python module, don't use special argument handling
 * so the module loading logic can control the `argv` & `argc`. */
#if defined(WIN32) && !defined(WITH_PYTHON_MODULE)
#  define USE_WIN32_UNICODE_ARGS
#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name Local Application State
 * \{ */

/* written to by 'creator_args.c' */
struct ApplicationState app_state = {
    .signal =
        {
            .use_crash_handler = true,
            .use_abort_handler = true,
        },
    .exit_code_on_error =
        {
            .python = 0,
        },
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Application Level Callbacks
 *
 * Initialize callbacks for the modules that need them.
 * \{ */

static void callback_mem_error(const char *errorStr)
{
  fputs(errorStr, stderr);
  fflush(stderr);
}

static void main_callback_setup(void)
{
  /* Error output from the guarded allocation routines. */
  MEM_set_error_callback(callback_mem_error);
}

/* free data on early exit (if Python calls 'sys.exit()' while parsing args for eg). */
struct CreatorAtExitData {
#ifndef WITH_PYTHON_MODULE
  bArgs *ba;
#endif

#ifdef USE_WIN32_UNICODE_ARGS
  const char **argv;
  int argv_num;
#endif

#if defined(WITH_PYTHON_MODULE) && !defined(USE_WIN32_UNICODE_ARGS)
  void *_empty; /* Prevent empty struct error with MSVC. */
#endif
};

static void callback_main_atexit(void *user_data)
{
  struct CreatorAtExitData *app_init_data = user_data;

#ifndef WITH_PYTHON_MODULE
  if (app_init_data->ba) {
    BLI_args_destroy(app_init_data->ba);
    app_init_data->ba = NULL;
  }
#else
  UNUSED_VARS(app_init_data); /* May be unused. */
#endif

#ifdef USE_WIN32_UNICODE_ARGS
  if (app_init_data->argv) {
    while (app_init_data->argv_num) {
      free((void *)app_init_data->argv[--app_init_data->argv_num]);
    }
    free((void *)app_init_data->argv);
    app_init_data->argv = NULL;
  }
#else
  UNUSED_VARS(app_init_data); /* May be unused. */
#endif
}

static void callback_clg_fatal(void *fp)
{
  BLI_system_backtrace(fp);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Blender as a Stand-Alone Python Module (bpy)
 *
 * While not officially supported, this can be useful for Python developers.
 * See: https://wiki.blender.org/wiki/Building_Blender/Other/BlenderAsPyModule
 * \{ */

#ifdef WITH_PYTHON_MODULE

/* Called in `bpy_interface.c` when building as a Python module. */
int main_python_enter(int argc, const char **argv);
void main_python_exit(void);

/* Rename the 'main' function, allowing Python initialization to call it. */
#  define main main_python_enter
static void *evil_C = NULL;

#  ifdef __APPLE__
/* Environment is not available in macOS shared libraries. */
#    include <crt_externs.h>
char **environ = NULL;
#  endif /* __APPLE__ */

#endif /* WITH_PYTHON_MODULE */

/** \} */

/* -------------------------------------------------------------------- */
/** \name GMP Allocator Workaround
 * \{ */

#if (defined(WITH_TBB_MALLOC) && defined(_MSC_VER) && defined(NDEBUG) && defined(WITH_GMP)) || \
    defined(DOXYGEN)
#  include "gmp.h"
#  include "tbb/scalable_allocator.h"

void *gmp_alloc(size_t size)
{
  return scalable_malloc(size);
}
void *gmp_realloc(void *ptr, size_t old_size, size_t new_size)
{
  return scalable_realloc(ptr, new_size);
}

void gmp_free(void *ptr, size_t size)
{
  scalable_free(ptr);
}
/**
 * Use TBB's scalable_allocator on Windows.
 * `TBBmalloc` correctly captures all allocations already,
 * however, GMP is built with MINGW since it doesn't build with MSVC,
 * which TBB has issues hooking into automatically.
 */
void gmp_blender_init_allocator()
{
  mp_set_memory_functions(gmp_alloc, gmp_realloc, gmp_free);
}
#endif

/** \} */



#define STACK_TRACE
#ifdef STACK_TRACE

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
static pthread_mutex_t thread_lock = PTHREAD_MUTEX_INITIALIZER;

static void mem_lock_thread(void)
{
  pthread_mutex_lock(&thread_lock);
}

static void mem_unlock_thread(void)
{
  pthread_mutex_unlock(&thread_lock);
}
static DWORD thread_main;
void MEM_StackInfo(void *ptr, const char *str, uint64_t len)
{
  // printStack();
#  if 1
  mem_lock_thread();
  if (push_cnt_id == 0) {
    {
      thread_main = GetCurrentThreadId();
      minfo = (MEM_info *)malloc(MAX_INFO * sizeof(MEM_info));
      for (int j = 0; j < MAX_INFO; j++) {
        minfo[j].is_free = true;
        minfo[j].len = 0;
        minfo[j].ptr = 1;
      };
      minfo_alloc = MAX_INFO;
    };
  }
  DWORD thread_id = GetCurrentThreadId();
  if (thread_main != thread_id) {

  }


  minfo_id = -1;
  for (int i = 0; i < minfo_alloc; i++) {
    if (minfo[i].is_free) {
      minfo_id = i;
      push_cnt_id++;
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

  mem_unlock_thread();

}
void MEM_PopInfo(void *ptr)
{
  
  mem_lock_thread();

  uintptr_t p = (uintptr_t)ptr;
  for (int i = 0; i < minfo_alloc; i++) {
    if (minfo[i].ptr == p) {
      minfo[i].is_free = true;
      minfo[i].ptr = 0;
      minfo[i].len = 0;
      pop_cnt_id++;
      break;
    }
  }
  mem_unlock_thread();

};

void MEM_PrintInfo()
{


  int cnt = 0;
  for (int i = 0; i < minfo_alloc; i++) {
    if (!minfo[i].is_free) {
      cnt++;
      //printf("Memory leak  found ADR %llx   size  %zu  \n  %s    \n",minfo[i].ptr, minfo[i].len, minfo[i].msg);
    }
  }
  //printf("Memory  push  %d   pop   %d  leak %d  \n", push_cnt_id, pop_cnt_id,cnt);
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





/* -------------------------------------------------------------------- */
/** \name Main Function
 * \{ */

/**
 * Blender's main function responsibilities are:
 * - setup subsystems.
 * - handle arguments.
 * - run #WM_main() event loop,
 *   or exit immediately when running in background-mode.
 */
int main(int argc,
#ifdef USE_WIN32_UNICODE_ARGS
         const char **UNUSED(argv_c)
#else
         const char **argv
#endif
)
{
  bContext *C;

#ifndef WITH_PYTHON_MODULE
  bArgs *ba;
#endif

#ifdef USE_WIN32_UNICODE_ARGS
  char **argv;
  int argv_num;
#endif

  /* --- end declarations --- */

  /* Ensure we free data on early-exit. */
  struct CreatorAtExitData app_init_data = {NULL};
  BKE_blender_atexit_register(callback_main_atexit, &app_init_data);

  /* Un-buffered `stdout` makes `stdout` and `stderr` better synchronized, and helps
   * when stepping through code in a debugger (prints are immediately
   * visible). However disabling buffering causes lock contention on windows
   * see T76767 for details, since this is a debugging aid, we do not enable
   * the un-buffered behavior for release builds. */
#ifndef NDEBUG
  setvbuf(stdout, NULL, _IONBF, 0);
#endif

#ifdef WIN32
  /* We delay loading of OPENMP so we can set the policy here. */
#  if defined(_MSC_VER)
  _putenv_s("OMP_WAIT_POLICY", "PASSIVE");
#  endif

#  ifdef USE_WIN32_UNICODE_ARGS
  /* Win32 Unicode Arguments. */
  {
    /* NOTE: Can't use `guardedalloc` allocation here, as it's not yet initialized
     * (it depends on the arguments passed in, which is what we're getting here!) */
    wchar_t **argv_16 = CommandLineToArgvW(GetCommandLineW(), &argc);
    argv = malloc(argc * sizeof(char *));
    for (argv_num = 0; argv_num < argc; argv_num++) {
      argv[argv_num] = alloc_utf_8_from_16(argv_16[argv_num], 0);
    }
    LocalFree(argv_16);

    /* free on early-exit */
    app_init_data.argv = argv;
    app_init_data.argv_num = argv_num;
  }
#  endif /* USE_WIN32_UNICODE_ARGS */
#endif   /* WIN32 */

  /* NOTE: Special exception for guarded allocator type switch:
   *       we need to perform switch from lock-free to fully
   *       guarded allocator before any allocation happened.
   */
  {
    int i;
    for (i = 0; i < argc; i++) {
      if (STR_ELEM(argv[i], "-d", "--debug", "--debug-memory", "--debug-all")) {
        printf("Switching to fully guarded memory allocator.\n");
        MEM_use_guarded_allocator();
        break;
      }
      if (STREQ(argv[i], "--")) {
        break;
      }
    }
    MEM_init_memleak_detection();
  }

#ifdef BUILD_DATE
  {
    time_t temp_time = build_commit_timestamp;
    struct tm *tm = gmtime(&temp_time);
    if (LIKELY(tm)) {
      strftime(build_commit_date, sizeof(build_commit_date), "%Y-%m-%d", tm);
      strftime(build_commit_time, sizeof(build_commit_time), "%H:%M", tm);
    }
    else {
      const char *unknown = "date-unknown";
      BLI_strncpy(build_commit_date, unknown, sizeof(build_commit_date));
      BLI_strncpy(build_commit_time, unknown, sizeof(build_commit_time));
    }
  }
#endif

#ifdef WITH_SDL_DYNLOAD
  sdlewInit();
#endif

  /* Initialize logging. */
  CLG_init();
  CLG_fatal_fn_set(callback_clg_fatal);

  C = CTX_create();

#ifdef WITH_PYTHON_MODULE
#  ifdef __APPLE__
  environ = *_NSGetEnviron();
#  endif

#  undef main
  evil_C = C;
#endif

#ifdef WITH_BINRELOC
  br_init(NULL);
#endif

#ifdef WITH_LIBMV
  libmv_initLogging(argv[0]);
#elif defined(WITH_CYCLES_LOGGING)
  CCL_init_logging(argv[0]);
#endif

#if defined(WITH_TBB_MALLOC) && defined(_MSC_VER) && defined(NDEBUG) && defined(WITH_GMP)
  gmp_blender_init_allocator();
#endif

  main_callback_setup();

#if defined(__APPLE__) && !defined(WITH_PYTHON_MODULE) && !defined(WITH_HEADLESS)
  /* Patch to ignore argument finder gives us (PID?) */
  if (argc == 2 && STRPREFIX(argv[1], "-psn_")) {
    extern int GHOST_HACK_getFirstFile(char buf[]);
    static char firstfilebuf[512];

    argc = 1;

    if (GHOST_HACK_getFirstFile(firstfilebuf)) {
      argc = 2;
      argv[1] = firstfilebuf;
    }
  }
#endif

#ifdef __FreeBSD__
  fpsetmask(0);
#endif

  /* Initialize path to executable. */
  BKE_appdir_program_path_init(argv[0]);

  BLI_threadapi_init();

  DNA_sdna_current_init();

  BKE_blender_globals_init(); /* blender.c */

  BKE_cpp_types_init();
  BKE_idtype_init();
  BKE_cachefiles_init();
  BKE_modifier_init();
  BKE_gpencil_modifier_init();
  BKE_shaderfx_init();
  BKE_volumes_init();
  DEG_register_node_types();

  BKE_brush_system_init();
  RE_texture_rng_init();

  BKE_callback_global_init();

  /* First test for background-mode (#Global.background) */
#ifndef WITH_PYTHON_MODULE
  ba = BLI_args_create(argc, (const char **)argv); /* skip binary path */

  /* Ensure we free on early exit. */
  app_init_data.ba = ba;

  main_args_setup(C, ba);

  /* Begin argument parsing, ignore leaks so arguments that call #exit
   * (such as '--version' & '--help') don't report leaks. */
  MEM_use_memleak_detection(false);

  /* Parse environment handling arguments. */
  BLI_args_parse(ba, ARG_PASS_ENVIRONMENT, NULL, NULL);

#else
  /* Using preferences or user startup makes no sense for #WITH_PYTHON_MODULE. */
  G.factory_startup = true;
#endif

  /* After parsing #ARG_PASS_ENVIRONMENT such as `--env-*`,
   * since they impact `BKE_appdir` behavior. */
  BKE_appdir_init();

  /* After parsing number of threads argument. */
  BLI_task_scheduler_init();

  /* Initialize sub-systems that use `BKE_appdir.h`. */
  IMB_init();

#ifdef WITH_USD
  USD_ensure_plugin_path_registered();
#endif

#ifndef WITH_PYTHON_MODULE
  /* First test for background-mode (#Global.background) */
  BLI_args_parse(ba, ARG_PASS_SETTINGS, NULL, NULL);

  main_signal_setup();
#endif

#ifdef WITH_FFMPEG
  /* Keep after #ARG_PASS_SETTINGS since debug flags are checked. */
  IMB_ffmpeg_init();
#endif
  /* End argument parsing, allow memory leaks to be printed. */
  MEM_use_memleak_detection(true);

  /* After #ARG_PASS_SETTINGS arguments, this is so #WM_main_playanim skips #RNA_init. */
  RNA_init();



  RE_engines_init();
  BKE_node_system_init();
  BKE_particle_init_rng();
  /* End second initialization. */

#if defined(WITH_PYTHON_MODULE) || defined(WITH_HEADLESS)
  /* Python module mode ALWAYS runs in background-mode (for now). */
  G.background = true;
#else
  if (G.background) {
    main_signal_setup_background();
  }
#endif

  /* Background render uses this font too. */
  BKE_vfont_builtin_register(datatoc_bfont_pfb, datatoc_bfont_pfb_size);

  /* Initialize FFMPEG if built in, also needed for background-mode if videos are
   * rendered via FFMPEG. */
  BKE_sound_init_once();

  BKE_materials_init();
 
#ifndef WITH_PYTHON_MODULE
  if (G.background == 0) {
    BLI_args_parse(ba, ARG_PASS_SETTINGS_GUI, NULL, NULL);
  }
  BLI_args_parse(ba, ARG_PASS_SETTINGS_FORCE, NULL, NULL);
#endif

  WM_init(C, argc, (const char **)argv);

  //WM_exit(C);
  /* Need to be after WM init so that userpref are loaded. */
  RE_engines_init_experimental();

#ifndef WITH_PYTHON
  printf(
      "\n* WARNING * - Blender compiled without Python!\n"
      "this is not intended for typical usage\n\n");
#endif

  CTX_py_init_set(C, true);
  //WM_keyconfig_init(C);

#ifdef WITH_FREESTYLE
  /* Initialize Freestyle. */
  FRS_init();
  FRS_set_context(C);
#endif

  /* OK we are ready for it */
#ifndef WITH_PYTHON_MODULE
  /* Handles #ARG_PASS_FINAL. */
  main_args_setup_post(C, ba);
#endif

  /* Explicitly free data allocated for argument parsing:
   * - 'ba'
   * - 'argv' on WIN32.
   */
  callback_main_atexit(&app_init_data);
  BKE_blender_atexit_unregister(callback_main_atexit, &app_init_data);

  /* End argument parsing, allow memory leaks to be printed. */
  MEM_use_memleak_detection(true);
  
  /* Paranoid, avoid accidental re-use. */
#ifndef WITH_PYTHON_MODULE
  ba = NULL;
  (void)ba;
#endif

#ifdef USE_WIN32_UNICODE_ARGS
  argv = NULL;
  (void)argv;
#endif

#ifndef WITH_PYTHON_MODULE
  if (G.background) {
    /* Using window-manager API in background-mode is a bit odd, but works fine. */
    WM_exit(C);
  }
  else {

    /* When no file is loaded, show the splash screen. */
    const char *blendfile_path = BKE_main_blendfile_path_from_global();
    if (blendfile_path[0] == '\0') {
      WM_init_splash(C);
    }
  
    WM_main(C);
  }
#endif /* WITH_PYTHON_MODULE */

  return 0;
} /* End of `int main(...)` function. */

#ifdef WITH_PYTHON_MODULE
void main_python_exit(void)
{
  WM_exit_ex((bContext *)evil_C, true);
  evil_C = NULL;
}
#endif

/** \} */
