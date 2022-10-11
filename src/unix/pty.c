#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <util.h>
#include <uv.h>

#include "../../include/tt.h"

int
tt_pty_spawn (uv_loop_t *loop, tt_pty_t *handle, const tt_term_options_t *term, const tt_process_options_t *process) {
  int fd, err;

  struct winsize size = {
    .ws_col = term->width,
    .ws_row = term->height,
  };

  pid_t pid = forkpty(&fd, NULL, NULL, &size);

  switch (pid) {
  case -1:
    return uv_translate_sys_error(errno);

  case 0: {
    if (process->cwd != NULL) chdir(process->cwd);

    if (execvp(process->file, process->args)) {
      exit(1);
    }
  }

  default:
    handle->fd = fd;
    handle->pid = pid;

    err = uv_tty_init(loop, &handle->tty, fd, 0);
    if (err < 0) return err;
  }

  return 0;
}
