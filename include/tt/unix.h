#ifndef TT_UNIX_H
#define TT_UNIX_H

#include <unistd.h>
#include <uv.h>

#include "../tt.h"

struct tt_pty_s {
  uv_tty_t tty;
  uv_process_t process;

  int primary;
  int replica;
  uv_pid_t pid;

  int flags;

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

#endif // TT_UNIX_H
