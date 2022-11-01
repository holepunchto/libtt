#ifndef TT_WIN_H
#define TT_WIN_H

#include <uv.h>

#include "../tt.h"

struct tt_pty_s {
  uv_pipe_t in;
  uv_pipe_t out;

  struct {
    void *handle;
    void *in;
    void *out;
    void *process;
    STARTUPINFOEXW info;
  } console;

  uv_pid_t pid;

  int flags;

  uv_async_t exit;
  long exit_status;
  int term_signal;

  uv_thread_t thread;

  int width;
  int height;

  int active;

  tt_pty_alloc_cb on_alloc;
  tt_pty_read_cb on_read;
  tt_pty_exit_cb on_exit;
  tt_pty_close_cb on_close;
};

struct tt_pty_write_s {
  uv_write_t req;

  tt_pty_t *handle;

  tt_pty_write_cb on_write;
};

#endif // TT_WIN_H
