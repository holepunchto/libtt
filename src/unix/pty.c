#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <uv.h>

// https://www.gnu.org/software/gnulib/manual/html_node/forkpty.html
#if defined(__APPLE__) || defined(__OpenBSD__) || defined(__NetBSD__)
#include <util.h>
#elif defined(__FreeBSD__)
#include <libutil.h>
#else
#include <pty.h>
#endif

#include "../../include/tt.h"

typedef enum {
  TT_PTY_READING = 1,
} tt_pty_flags;

static void
on_close (uv_handle_t *uv_handle) {
  tt_pty_t *handle = (tt_pty_t *) uv_handle->data;

  handle->active--;

  if (handle->active == 0) {
    close(handle->primary);
    close(handle->replica);

    if (handle->on_close) handle->on_close(handle);
  }
}

static void
on_process_exit (uv_process_t *process, int64_t exit_status, int term_signal) {
  tt_pty_t *handle = (tt_pty_t *) process->data;

  if (handle->on_exit) handle->on_exit(handle, exit_status, term_signal);

  uv_close((uv_handle_t *) process, on_close);
}

int
tt_pty_spawn (uv_loop_t *loop, tt_pty_t *handle, const tt_term_options_t *term, const tt_process_options_t *process, tt_pty_exit_cb exit_cb) {
  handle->flags = 0;
  handle->active = 0;
  handle->on_exit = exit_cb;

  handle->tty.data = (void *) handle;
  handle->process.data = (void *) handle;

  int primary = -1, replica = -1, err;

  int width = term ? term->width : 80;
  int height = term ? term->height : 60;

  struct winsize size = {
    .ws_col = width,
    .ws_row = height,
  };

  int res = openpty(&primary, &replica, NULL, NULL, &size);

  if (res < 0) {
    err = uv_translate_sys_error(errno);
    goto err;
  };

  uv_process_options_t options = {
    .exit_cb = on_process_exit,
    .file = process->file,
    .args = process->args,
    .env = process->env,
    .cwd = process->cwd,
    .flags = UV_PROCESS_DETACHED,
    .stdio_count = 3,
    .stdio = (uv_stdio_container_t[]){
      {.flags = UV_INHERIT_FD, .data.fd = replica},
      {.flags = UV_INHERIT_FD, .data.fd = replica},
      {.flags = UV_INHERIT_FD, .data.fd = replica},
    },
    .uid = 0,
    .gid = 0,
  };

  err = uv_spawn(loop, &handle->process, &options);
  if (err < 0) goto err;
  handle->active++;

  handle->width = width;
  handle->height = height;
  handle->primary = primary;
  handle->replica = replica;
  handle->pid = handle->process.pid;

  err = uv_tty_init(loop, &handle->tty, primary, 0);
  assert(err == 0);
  handle->active++;

  return 0;

err:
  if (primary != -1) close(primary);
  if (replica != -1) close(replica);

  return err;
}

static void
on_read (uv_stream_t *uv_stream, ssize_t read_len, const uv_buf_t *buf) {
  tt_pty_t *handle = (tt_pty_t *) uv_stream;

  handle->on_read(handle, read_len, buf);
}

static void
on_alloc (uv_handle_t *uv_handle, size_t suggested_size, uv_buf_t *buf) {
  tt_pty_t *handle = (tt_pty_t *) uv_handle->data;

  handle->on_alloc(handle, suggested_size, buf);
}

int
tt_pty_read_start (tt_pty_t *handle, tt_pty_alloc_cb alloc_cb, tt_pty_read_cb cb) {
  if (handle->flags & TT_PTY_READING) {
    return UV_EALREADY;
  }

  handle->flags |= TT_PTY_READING;
  handle->on_alloc = alloc_cb;
  handle->on_read = cb;

  return uv_read_start((uv_stream_t *) &handle->tty, on_alloc, on_read);
}

int
tt_pty_read_stop (tt_pty_t *handle) {
  if (!(handle->flags & TT_PTY_READING)) {
    return 0;
  }

  handle->flags &= ~TT_PTY_READING;
  handle->on_read = NULL;

  return uv_read_stop((uv_stream_t *) &handle->tty);
}

static void
on_write (uv_write_t *uv_req, int status) {
  tt_pty_write_t *req = (tt_pty_write_t *) uv_req;

  req->on_write(req, status);
}

int
tt_pty_write (tt_pty_write_t *req, tt_pty_t *handle, const uv_buf_t bufs[], unsigned int bufs_len, tt_pty_write_cb cb) {
  req->handle = handle;
  req->on_write = cb;
  req->req.data = (void *) req;

  return uv_write(&req->req, (uv_stream_t *) &handle->tty, bufs, bufs_len, on_write);
}

int
tt_pty_resize (tt_pty_t *handle, int width, int height) {
  struct winsize size = {
    .ws_col = width,
    .ws_row = height,
  };

  int res;

  do {
    res = ioctl(handle->primary, TIOCSWINSZ, &size);
  } while (res == -1 && errno == EINTR);

  if (res < 0) return uv_translate_sys_error(errno);

  handle->width = width;
  handle->height = height;

  return 0;
}

void
tt_pty_close (tt_pty_t *handle, tt_pty_close_cb cb) {
  handle->on_close = cb;

  uv_close((uv_handle_t *) &handle->tty, on_close);
}

int
tt_pty_kill (tt_pty_t *handle, int signum) {
  return uv_process_kill(&handle->process, signum);
}
