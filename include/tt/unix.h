#ifndef TT_UNIX_H
#define TT_UNIX_H

#include <unistd.h>
#include <uv.h>

#include "../tt.h"

struct tt_pty_s {
  uv_tty_t tty;

  int fd;
  uv_pid_t pid;

  int flags;

  uv_async_t exit;
  int exit_status;

  uv_thread_t thread;

  int active;

  tt_pty_read_cb on_read;
  tt_pty_exit_cb on_exit;
  tt_pty_close_cb on_close;
};

struct tt_pty_write_s {
  uv_write_t req;

  tt_pty_t *handle;

  tt_pty_write_cb on_write;
};

#endif // TT_UNIX_H
