#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <uv.h>

#include "../include/tt.h"

uv_loop_t *loop;

bool read_called = false;

static void
on_read (tt_pty_t *pty, ssize_t read_len, const uv_buf_t *buf) {
  read_called = true;

  if (read_len > 0) printf("%.*s", (int) buf->len, buf->base);
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
    .file = "tty",
  };

  tt_pty_t pty;
  e = tt_pty_spawn(loop, &pty, &term, &process);
  assert(e == 0);

  e = tt_pty_read_start(&pty, on_read);
  assert(e == 0);

  uv_run(loop, UV_RUN_DEFAULT);

  assert(read_called);

  return 0;
}
