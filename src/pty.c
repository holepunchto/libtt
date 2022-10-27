#include <uv.h>

#include "../include/tt.h"

uv_pid_t
tt_pty_get_pid (tt_pty_t *handle) {
  return handle->pid;
}
