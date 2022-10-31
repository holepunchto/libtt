#include <assert.h>
#include <uv.h>

#include "../include/tt.h"

uv_loop_t *loop;

int
main () {
  int e;

  loop = uv_default_loop();

  tt_process_options_t process = {
    .file = "this-does-not-exist",
  };

  tt_pty_t pty;
  e = tt_pty_spawn(loop, &pty, NULL, &process, NULL);
  assert(e == UV_ENOENT);

  return 0;
}
