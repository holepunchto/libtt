#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <uv.h>

#include "../include/tt.h"

uv_loop_t *loop;

bool close_called = false;
bool exit_called = false;

size_t read_total = 0;
size_t payload_total = 0;

char read_buf[64 * 1024];

static void
on_close(tt_pty_t *handle) {
  close_called = true;
}

static void
on_process_exit(tt_pty_t *handle, int64_t exit_status, int term_signal) {
  exit_called = true;

  assert(exit_status == 0);

  tt_pty_close(handle, on_close);
}

static void
on_read(tt_pty_t *pty, ssize_t read_len, const uv_buf_t *buf) {
  assert(read_len >= 0);

  read_total += read_len;

  for (ssize_t i = 0; i < read_len; i++) {
    if (buf->base[i] == 'a') payload_total++;
  }
}

static void
on_alloc(tt_pty_t *pty, size_t suggested_size, uv_buf_t *buf) {
  size_t offset = read_total % sizeof(read_buf);

  *buf = uv_buf_init(read_buf + offset, sizeof(read_buf) - offset);
}

int
main() {
  int e;

  loop = uv_default_loop();

  tt_term_options_t term = {
    .width = 80,
    .height = 60,
  };

  tt_process_options_t process = {
    .file = "node",
    .args = (char *[]){"node", "test/fixtures/big-write.mjs", NULL},
  };

  tt_pty_t pty;
  e = tt_pty_spawn(loop, &pty, &term, &process, on_process_exit);
  assert(e == 0);

  e = tt_pty_read_start(&pty, on_alloc, on_read);
  assert(e == 0);

  uv_run(loop, UV_RUN_DEFAULT);

  assert(close_called);
  assert(exit_called);
#if defined(_WIN32)
  // The pseudoconsole renders the output, adding escape sequences and line
  // breaks, and may repaint lines it has already sent.
  assert(payload_total >= 64 * 1024);
#else
  assert(read_total == 64 * 1024);
  assert(payload_total == 64 * 1024);
#endif

  return 0;
}
