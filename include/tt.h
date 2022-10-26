#ifndef TT_H
#define TT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <uv.h>

typedef struct tt_pty_s tt_pty_t;
typedef struct tt_pty_write_s tt_pty_write_t;

typedef struct tt_term_options_s tt_term_options_t;
typedef struct tt_process_options_s tt_process_options_t;

typedef void (*tt_pty_read_cb)(tt_pty_t *handle, ssize_t read_len, const uv_buf_t *buf);
typedef void (*tt_pty_write_cb)(tt_pty_write_t *req, int status);

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

int
tt_pty_read_start (tt_pty_t *handle, tt_pty_read_cb cb);

int
tt_pty_read_stop (tt_pty_t *handle);

int
tt_pty_write (tt_pty_write_t *req, tt_pty_t *handle, const uv_buf_t bufs[], unsigned int bufs_len, tt_pty_write_cb cb);

#ifdef __cplusplus
}
#endif

#endif // TT_H
