#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
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

  if (handle->active == 0 && handle->on_close) handle->on_close(handle);
}

static void
wait_for_exit (void *data) {
  tt_pty_t *handle = (tt_pty_t *) data;

  int status;

  while (true) {
    int pid = waitpid(handle->pid, &status, 0);

    if (pid == handle->pid) break;
  }

  if (WIFEXITED(status)) {
    handle->exit_status = WEXITSTATUS(status);
  }

  uv_async_send(&handle->exit);
}

static void
on_exit (uv_async_t *async) {
  tt_pty_t *handle = (tt_pty_t *) async->data;

  handle->on_exit(handle, handle->exit_status);

  uv_close((uv_handle_t *) async, on_close);
}

int
tt_pty_spawn (uv_loop_t *loop, tt_pty_t *handle, const tt_term_options_t *term, const tt_process_options_t *process, tt_pty_exit_cb exit_cb) {
  handle->flags = 0;
  handle->exit_status = 0;
  handle->active = 0;
  handle->on_exit = exit_cb;

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

    break;
  }

  default:
    handle->fd = fd;
    handle->pid = pid;
    handle->tty.data = (void *) handle;
    handle->exit.data = (void *) handle;

    err = uv_tty_init(loop, &handle->tty, fd, 0);
    assert(err == 0);
    handle->active++;

    err = uv_async_init(loop, &handle->exit, on_exit);
    assert(err == 0);
    handle->active++;

    err = uv_thread_create(&handle->thread, wait_for_exit, (void *) handle);
    assert(err == 0);
  }

  return 0;
}

static void
on_read (uv_stream_t *uv_stream, ssize_t read_len, const uv_buf_t *buf) {
  tt_pty_t *handle = (tt_pty_t *) uv_stream;

  handle->on_read(handle, read_len, buf);

  free(buf->base);
}

static void
on_alloc (uv_handle_t *uv_handle, size_t suggested_size, uv_buf_t *buf) {
  buf->base = malloc(suggested_size);
  buf->len = suggested_size;
}

int
tt_pty_read_start (tt_pty_t *handle, tt_pty_read_cb cb) {
  if (handle->flags & TT_PTY_READING) {
    return UV_EALREADY;
  }

  handle->flags |= TT_PTY_READING;
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

void
tt_pty_close (tt_pty_t *handle, tt_pty_close_cb cb) {
  handle->on_close = cb;

  uv_close((uv_handle_t *) &handle->tty, on_close);
}
