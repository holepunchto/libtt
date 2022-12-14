#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <uv.h>

#include "../include/tt.h"

uv_loop_t *loop;

bool close_called = false;
bool exit_called = false;
bool read_called = false;

static void
on_close (tt_pty_t *handle) {
  close_called = true;
}

static void
on_process_exit (tt_pty_t *handle, int64_t exit_status, int term_signal) {
  exit_called = true;

  assert(exit_status == 0);

  tt_pty_close(handle, on_close);
}

static void
on_read (tt_pty_t *pty, ssize_t read_len, const uv_buf_t *buf) {
  read_called = true;

  assert(read_len > 0);
  printf("%.*s", (int) read_len, buf->base);

  if (buf->base) free(buf->base);
}

static void
on_alloc (tt_pty_t *pty, size_t suggested_size, uv_buf_t *buf) {
  buf->base = malloc(suggested_size);
  buf->len = suggested_size;
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
    .args = (char *[]){"node", "test/fixtures/hello.mjs", NULL},
  };

  tt_pty_t pty;
  e = tt_pty_spawn(loop, &pty, &term, &process, on_process_exit);
  assert(e == 0);

  e = tt_pty_read_start(&pty, on_alloc, on_read);
  assert(e == 0);

  uv_run(loop, UV_RUN_DEFAULT);

  assert(close_called);
  assert(exit_called);
  assert(read_called);

  return 0;
}
