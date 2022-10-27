#include <assert.h>
#include <uv.h>

#include "../../include/tt.h"

#ifndef PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE
#define PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE 0x00020016
#endif

typedef enum {
  TT_PTY_READING = 1,
} tt_pty_flags;

static inline int
tt_create_console (tt_pty_t *pty, COORD size, HANDLE *in, HANDLE *out) {
  HANDLE in_read = NULL, in_write = NULL;
  HANDLE out_read = NULL, out_write = NULL;

  BOOL success;

  success = CreatePipe(&in_read, &in_write, NULL, 0);
  if (!success) goto err;

  success = CreatePipe(&out_read, &out_write, NULL, 0);
  if (!success) goto err;

  HPCON handle;

  HRESULT res = CreatePseudoConsole(size, in_read, out_write, 0, &handle);

  if (res < 0) {
    SetLastError(res);
    goto err;
  }

  pty->console.handle = handle;
  pty->console.in = in_write;
  pty->console.out = out_read;
  pty->console.process = NULL;

  *in = in_read;
  *out = out_write;

  return 0;

err:
  if (in_read) CloseHandle(in_read);
  if (in_write) CloseHandle(in_write);
  if (out_read) CloseHandle(out_read);
  if (out_write) CloseHandle(out_write);

  return uv_translate_sys_error(GetLastError());
}

static inline int
tt_prepare_startup_information (tt_pty_t *pty) {
  STARTUPINFOEXW info;
  memset(&info, 0, sizeof(info));

  info.StartupInfo.cb = sizeof(STARTUPINFOEXW);
  info.StartupInfo.dwFlags |= STARTF_USESTDHANDLES;

  size_t attr_len;
  InitializeProcThreadAttributeList(NULL, 1, 0, &attr_len);

  info.lpAttributeList = malloc(attr_len);

  BOOL success;

  success = InitializeProcThreadAttributeList(
    info.lpAttributeList,
    1,
    0,
    &attr_len
  );

  if (!success) goto err;

  success = UpdateProcThreadAttribute(
    info.lpAttributeList,
    0,
    PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
    pty->console.handle,
    sizeof(pty->console.handle),
    NULL,
    NULL
  );

  if (!success) goto err;

  pty->console.info = info;

  return 0;

err:
  if (info.lpAttributeList) {
    DeleteProcThreadAttributeList(info.lpAttributeList);
    free(info.lpAttributeList);
  }

  return uv_translate_sys_error(GetLastError());
}

static void
on_exit_wait (void *context, BOOLEAN fired) {
  tt_pty_t *handle = (tt_pty_t *) context;

  uv_async_send(&handle->exit);
}

static void
on_exit (uv_async_t *async) {
  tt_pty_t *handle = (tt_pty_t *) async->data;

  tt_pty_read_stop(handle);

  uv_close((uv_handle_t *) &handle->exit, NULL);
  uv_close((uv_handle_t *) &handle->input, NULL);
  uv_close((uv_handle_t *) &handle->output, NULL);

  UnregisterWait(handle->console.exit);

  ClosePseudoConsole(handle->console.handle);

  CloseHandle(handle->console.in);
  CloseHandle(handle->console.out);
  CloseHandle(handle->console.process);
}

static inline int
tt_launch_process (tt_pty_t *pty, PWCHAR cmd, HANDLE in, HANDLE out) {
  PROCESS_INFORMATION info;
  memset(&info, 0, sizeof(info));

  BOOL success;

  success = CreateProcessW(
    NULL,
    cmd,
    NULL,
    NULL,
    FALSE,
    EXTENDED_STARTUPINFO_PRESENT,
    NULL,
    NULL,
    &pty->console.info.StartupInfo,
    &info
  );

  if (!success) goto err;

  CloseHandle(info.hThread);
  CloseHandle(in);
  CloseHandle(out);

  pty->pid = info.dwProcessId;
  pty->console.process = info.hProcess;

  RegisterWaitForSingleObject(
    &pty->console.exit,
    pty->console.process,
    on_exit_wait,
    (void *) pty,
    INFINITE,
    WT_EXECUTEONLYONCE
  );

  return 0;

err:
  return uv_translate_sys_error(GetLastError());
}

int
tt_pty_spawn (uv_loop_t *loop, tt_pty_t *handle, const tt_term_options_t *term, const tt_process_options_t *process) {
  int err = 0;

  COORD size = {
    .X = term->width,
    .Y = term->height,
  };

  HANDLE in = NULL, out = NULL;

  err = tt_create_console(handle, size, &in, &out);
  if (err < 0) goto err;

  err = tt_prepare_startup_information(handle);
  if (err < 0) goto err;

  PWCHAR cmd = L"node test/fixtures/hello.mjs";

  err = tt_launch_process(handle, cmd, in, out);
  if (err < 0) goto err;

  handle->exit.data = handle;
  handle->input.data = handle;
  handle->output.data = handle;

  err = uv_async_init(loop, &handle->exit, on_exit);
  assert(err == 0);

  err = uv_pipe_init(loop, &handle->input, 0);
  assert(err == 0);

  err = uv_pipe_open(&handle->input, uv_open_osfhandle(handle->console.in));
  assert(err == 0);

  err = uv_pipe_init(loop, &handle->output, 0);
  assert(err == 0);

  err = uv_pipe_open(&handle->output, uv_open_osfhandle(handle->console.out));
  assert(err == 0);

  return 0;

err:
  if (in) CloseHandle(in);
  if (out) CloseHandle(out);

  return err;
}

static void
on_read (uv_stream_t *uv_stream, ssize_t read_len, const uv_buf_t *buf) {
  tt_pty_t *handle = (tt_pty_t *) uv_stream->data;

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

  return uv_read_start((uv_stream_t *) &handle->output, on_alloc, on_read);
}

int
tt_pty_read_stop (tt_pty_t *handle) {
  if (!(handle->flags & TT_PTY_READING)) {
    return 0;
  }

  handle->flags &= ~TT_PTY_READING;
  handle->on_read = NULL;

  return uv_read_stop((uv_stream_t *) &handle->output);
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

  return uv_write(&req->req, (uv_stream_t *) &handle->input, bufs, bufs_len, on_write);
}
