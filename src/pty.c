#include <uv.h>

#include "../include/tt.h"

uv_pid_t
tt_pty_get_pid (tt_pty_t *handle) {
  return handle->pid;
}

int
tt_pty_get_size (tt_pty_t *handle, int *width, int *height) {
  *width = handle->width;
  *height = handle->height;

  return 0;
}
