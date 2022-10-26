#ifndef TT_UNIX_H
#define TT_UNIX_H

#include <unistd.h>
#include <uv.h>

#include "../tt.h"

struct tt_pty_s {
  uv_tty_t tty;

  int fd;
  pid_t pid;

  int flags;

  tt_pty_read_cb on_read;
};

struct tt_pty_write_s {
  uv_write_t req;

  tt_pty_t *handle;

  tt_pty_write_cb on_write;
};

#endif // TT_UNIX_H
