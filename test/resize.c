#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <uv.h>

#include "../include/tt.h"

uv_loop_t *loop;

static void
on_read (tt_pty_t *pty, ssize_t read_len, const uv_buf_t *buf) {
  printf("read\n");
  if (read_len == UV_EOF) return;

  assert(read_len > 0);
  printf("%.*s", (int) read_len, buf->base);

  if (buf->base) free(buf->base);
}

static void
on_alloc (tt_pty_t *pty, size_t suggested_size, uv_buf_t *buf) {
  buf->base = malloc(suggested_size);
  buf->len = suggested_size;
}

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

  e = tt_pty_read_start(&pty, on_alloc, on_read);
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
