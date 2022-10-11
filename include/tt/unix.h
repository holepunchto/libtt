#ifndef TT_UNIX_H
#define TT_UNIX_H

#include <unistd.h>
#include <uv.h>

#include "../tt.h"

struct tt_pty_s {
  uv_tty_t tty;
  int fd;
  pid_t pid;
};

#endif // TT_UNIX_H
