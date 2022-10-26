#include <uv.h>

#include "../../include/tt.h"

#ifndef PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE
#define PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE 0x00020016
#endif

typedef enum {
  TT_PTY_READING = 1,
} tt_pty_flags;

int
tt_create_console (tt_pty_t *pty, COORD size, HANDLE *inputRead, HANDLE *outputWrite) {
  HANDLE inRead = NULL, inWrite = NULL;
  HANDLE outRead = NULL, outWrite = NULL;

  if (!CreatePipe(&inRead, &inWrite, NULL, 0)) {
    goto err;
  }

  if (!CreatePipe(&outRead, &outWrite, NULL, 0)) {
    goto err;
  }

  HPCON handle;

  HRESULT res = CreatePseudoConsole(size, inRead, outWrite, 0, &handle);

  if (FAILED(res)) {
    SetLastError(res);
    goto err;
  }

  pty->console.handle = handle;
  pty->console.in = inWrite;
  pty->console.out = outRead;
  pty->console.process = NULL;

  *inputRead = inRead;
  *outputWrite = outWrite;

  return 0;

err:
  if (inRead) CloseHandle(inRead);
  if (inWrite) CloseHandle(inWrite);
  if (outRead) CloseHandle(outRead);
  if (outWrite) CloseHandle(outWrite);

  return uv_translate_sys_error(GetLastError());
}

int
tt_prepare_startup_information (tt_pty_t *pty, STARTUPINFOEXW *psi) {
  STARTUPINFOEXW si;
  memset(&si, 0, sizeof(si));

  si.StartupInfo.cb = sizeof(STARTUPINFOEXW);
  si.StartupInfo.dwFlags |= STARTF_USESTDHANDLES;

  size_t bytesRequired;
  InitializeProcThreadAttributeList(NULL, 1, 0, &bytesRequired);

  si.lpAttributeList = malloc(bytesRequired);

  if (!InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &bytesRequired)) {
    goto err;
  }

  if (!UpdateProcThreadAttribute(si.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, pty->console.handle, sizeof(pty->console.handle), NULL, NULL)) {
    goto err;
  }

  *psi = si;

  return 0;

err:
  if (si.lpAttributeList) free(si.lpAttributeList);

  return uv_translate_sys_error(GetLastError());
}

int
tt_launch_process (tt_pty_t *pty, const wchar_t *cmd, HANDLE in, HANDLE out, STARTUPINFOEXW *si) {
  wchar_t *cmdw = _wcsdup(cmd);

  PROCESS_INFORMATION pi;
  memset(&pi, 0, sizeof(pi));

  if (!CreateProcessW(NULL, cmdw, NULL, NULL, FALSE, EXTENDED_STARTUPINFO_PRESENT, NULL, NULL, &si->StartupInfo, &pi)) {
    goto err;
  }

  CloseHandle(pi.hThread);
  CloseHandle(in);
  CloseHandle(out);

  free(cmdw);

  pty->console.process = pi.hProcess;

  return 0;

err:
  free(cmdw);

  return uv_translate_sys_error(GetLastError());
}

static void
on_exit_event (void *context, BOOLEAN fired) {
  tt_pty_t *handle = (tt_pty_t *) context;

  uv_async_send(&handle->exit);
}

static void
on_exit (uv_async_t *async) {
  tt_pty_t *handle = (tt_pty_t *) async->data;

  UnregisterWait(handle->console.exit);
}

int
tt_pty_spawn (uv_loop_t *loop, tt_pty_t *handle, const tt_term_options_t *term, const tt_process_options_t *process) {
  int err;

  COORD size = {
    .X = term->width,
    .Y = term->height,
  };

  HANDLE in, out;

  err = tt_create_console(handle, size, &in, &out);
  if (err < 0) return err;

  STARTUPINFOEXW si;

  err = tt_prepare_startup_information(handle, &si);
  if (err < 0) return err;

  PWCHAR cmd = L"node test/fixtures/hello.mjs";

  err = tt_launch_process(handle, cmd, in, out, &si);
  if (err < 0) return err;

  uv_async_init(loop, &handle->exit, on_exit);

  RegisterWaitForSingleObject(
    &handle->console.exit,
    handle->console.process,
    on_exit_event,
    (void *) handle,
    INFINITE,
    WT_EXECUTEONLYONCE
  );

  handle->input.data = handle;
  handle->output.data = handle;

  err = uv_pipe_init(loop, &handle->input, 0);
  if (err < 0) return err;

  err = uv_stream_set_blocking((uv_stream_t *) &handle->input, 1);
  if (err < 0) return err;

  err = uv_pipe_open(&handle->input, uv_open_osfhandle(handle->console.in));
  if (err < 0) return err;

  err = uv_pipe_init(loop, &handle->output, 0);
  if (err < 0) return err;

  err = uv_stream_set_blocking((uv_stream_t *) &handle->output, 1);
  if (err < 0) return err;

  err = uv_pipe_open(&handle->output, uv_open_osfhandle(handle->console.out));
  if (err < 0) return err;

  return 0;
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
