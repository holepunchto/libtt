#ifndef TT_H
#define TT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <uv.h>

typedef struct tt_pty_s tt_pty_t;
typedef struct tt_term_options_s tt_term_options_t;
typedef struct tt_process_options_s tt_process_options_t;

#if defined(_WIN32)
#include "tt/win.h"
#else
#include "tt/unix.h"
#endif

struct tt_term_options_s {
  int width;
  int height;
};

struct tt_process_options_s {
  const char *file;
  char **args;
  const char *cwd;
};

int
tt_pty_spawn (uv_loop_t *loop, tt_pty_t *handle, const tt_term_options_t *term, const tt_process_options_t *process);

#ifdef __cplusplus
}
#endif

#endif // TT_H
