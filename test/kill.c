#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <uv.h>

#include "../include/tt.h"

uv_loop_t *loop;

bool exit_called = false;

static void
on_exit (tt_pty_t *handle, int64_t exit_status, int term_signal) {
  exit_called = true;

  assert(exit_status == 1);
  assert(term_signal == SIGTERM);

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
    .args = (char *[]){"node", "test/fixtures/spin.mjs", NULL},
  };

  tt_pty_t pty;
  e = tt_pty_spawn(loop, &pty, &term, &process, on_exit);
  assert(e == 0);

  e = tt_pty_kill(&pty, SIGTERM);
  assert(e == 0);

  uv_run(loop, UV_RUN_DEFAULT);

  assert(exit_called);

  return 0;
}
