#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <uv.h>

#include "../include/tt.h"

uv_loop_t *loop;

static void
on_read (uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
  if (nread > 0) printf("%.*s", (int) buf->len, buf->base);
}

static void
on_alloc (uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
  buf->base = malloc(suggested_size);
  buf->len = suggested_size;
}

int
main () {
  loop = uv_default_loop();

  tt_term_options_t term = {
    .width = 80,
    .height = 60,
  };

  tt_process_options_t process = {
    .file = "tty",
  };

  tt_pty_t pty;
  int e = tt_pty_spawn(loop, &pty, &term, &process);
  assert(e == 0);

  uv_read_start((uv_stream_t *) &pty, on_alloc, on_read);

  uv_run(loop, UV_RUN_DEFAULT);

  return 0;
}
