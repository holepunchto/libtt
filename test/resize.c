#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <uv.h>

#include "../include/tt.h"

uv_loop_t *loop;

static void
on_process_exit (tt_pty_t *handle, int64_t exit_status, int term_signal) {
  tt_pty_close(handle, NULL);
}

int
main () {
  int e;

  loop = uv_default_loop();

  tt_term_options_t term = {
    .width = 80,
    .height = 60,
  };

  tt_process_options_t process = {
    .file = "node",
    .args = (char *[]){"node", "test/fixtures/resize.mjs", NULL},
  };

  tt_pty_t pty;
  e = tt_pty_spawn(loop, &pty, &term, &process, on_process_exit);
  assert(e == 0);

  e = tt_pty_resize(&pty, 120, 90);
  assert(e == 0);

  int width, height;
  e = tt_pty_get_size(&pty, &width, &height);
  assert(e == 0);

  assert(width == 120);
  assert(height == 90);

  uv_run(loop, UV_RUN_DEFAULT);

  return 0;
}
