#include <assert.h>
#include <uv.h>

#include "../../include/tt.h"
#include "console.h"

#ifndef PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE
#define PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE 0x00020016
#endif

typedef enum {
  TT_PTY_READING = 1,
} tt_pty_flags;

static inline int
tt_to_wstring (const char *str, int len, PWCHAR wstr) {
  len = MultiByteToWideChar(CP_UTF8, 0, str, -1, wstr, len);

  if (len == 0) return uv_translate_sys_error(GetLastError());

  return len;
}

static inline int
tt_console_init (tt_pty_t *pty) {
  pty->console.handle = NULL;
  pty->console.in = NULL;
  pty->console.out = NULL;
  pty->console.process = NULL;

  memset(&pty->console.info, 0, sizeof(pty->console.info));

  return 0;
}

static inline void
tt_console_info_destroy (tt_pty_t *pty) {
  if (pty->console.info.lpAttributeList) {
    DeleteProcThreadAttributeList(pty->console.info.lpAttributeList);
    free(pty->console.info.lpAttributeList);
  }
}

static inline void
tt_console_destroy (tt_pty_t *pty) {
  if (pty->console.handle) ClosePseudoConsole(pty->console.handle);

  if (pty->console.in) CloseHandle(pty->console.in);
  if (pty->console.out) CloseHandle(pty->console.out);
  if (pty->console.process) CloseHandle(pty->console.process);

  tt_console_info_destroy(pty);
}

static inline int
tt_console_open (tt_pty_t *pty, COORD size, HANDLE *in, HANDLE *out) {
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
tt_prepare_console_info (tt_pty_t *pty) {
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
  tt_console_info_destroy(pty);

  return uv_translate_sys_error(GetLastError());
}

static inline int
tt_prepare_console_command (const tt_process_options_t *process, PWCHAR *pcmd) {
  PWCHAR cmd = NULL;

  const char *file = process->file;

  int err;

  int len = tt_to_wstring(process->file, 0, NULL);
  if (len < 0) {
    err = len;
    goto err;
  }

  cmd = malloc(len * sizeof(WCHAR));

  err = tt_to_wstring(process->file, len, cmd);
  if (err < 0) goto err;

  if (process->args != NULL) {
    int i = 1, offset = len - 1;

    while (TRUE) {
      char *arg = process->args[i++];

      if (arg == NULL) break;

      int arg_len = tt_to_wstring(arg, 0, NULL);
      if (arg_len < 0) {
        err = arg_len;
        goto err;
      }

      cmd[offset++] = L' ';

      len += arg_len + 1;

      cmd = realloc(cmd, len * sizeof(WCHAR));

      err = tt_to_wstring(arg, arg_len, cmd + offset);
      if (err < 0) goto err;

      offset += arg_len;
    }
  }

  *pcmd = cmd;

  return 0;

err:
  if (cmd) free(cmd);

  return err;
}

static inline int
tt_prepare_console_environment (const tt_process_options_t *process, PWCHAR *penv) {
  if (process->env == NULL) return 0;

  PWCHAR env = NULL;

  int err, len = 1, i = 0, offset = 0;

  while (TRUE) {
    char *pair = process->env[i++];

    if (pair == NULL) break;

    int pair_len = tt_to_wstring(pair, 0, NULL);
    if (pair_len < 0) {
      err = pair_len;
      goto err;
    }

    len += pair_len;

    env = realloc(env, len * sizeof(WCHAR));

    err = tt_to_wstring(pair, pair_len, env + offset);
    if (err < 0) goto err;

    offset += pair_len;
  }

  env[offset] = NULL;

  *penv = env;

  return 0;

err:
  if (env) free(env);

  return err;
}

static inline int
tt_prepare_console_directory (const tt_process_options_t *process, PWCHAR *pcmd) {
  if (process->cwd == NULL) return 0;

  PWCHAR cwd = NULL;

  int err;

  int len = tt_to_wstring(process->cwd, 0, NULL);
  if (len < 0) {
    err = len;
    goto err;
  }

  cwd = malloc(len * sizeof(WCHAR));

  err = tt_to_wstring(process->cwd, len, cwd);
  if (err < 0) goto err;

  *pcmd = cwd;

  return 0;

err:
  if (cwd) free(cwd);

  return err;
}

static void
on_close (uv_handle_t *uv_handle) {
  tt_pty_t *handle = (tt_pty_t *) uv_handle->data;

  handle->active--;

  if (handle->active == 0) {
    tt_console_destroy(handle);

    if (handle->on_close) handle->on_close(handle);
  }
}

static void
wait_for_exit (void *data) {
  tt_pty_t *handle = (tt_pty_t *) data;

  WaitForSingleObject(handle->console.process, INFINITE);

  uv_async_send(&handle->exit);
}

static void
on_process_exit (uv_async_t *async) {
  tt_pty_t *handle = (tt_pty_t *) async->data;

  GetExitCodeProcess(handle->console.process, &handle->exit_status);

  handle->on_exit(handle, handle->exit_status, handle->term_signal);

  uv_close((uv_handle_t *) &handle->exit, on_close);
}

static inline int
tt_console_spawn (tt_pty_t *pty, PWCHAR cmd, PWCHAR env, PWCHAR cwd, HANDLE in, HANDLE out) {
  PROCESS_INFORMATION info;
  memset(&info, 0, sizeof(info));

  BOOL success = CreateProcessW(
    NULL,
    cmd,
    NULL,
    NULL,
    FALSE,
    EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT,
    env,
    cwd,
    &pty->console.info.StartupInfo,
    &info
  );

  if (!success) goto err;

  CloseHandle(info.hThread);
  CloseHandle(in);
  CloseHandle(out);

  pty->pid = info.dwProcessId;
  pty->console.process = info.hProcess;

  return 0;

err:
  return uv_translate_sys_error(GetLastError());
}

int
tt_pty_spawn (uv_loop_t *loop, tt_pty_t *handle, const tt_term_options_t *term, const tt_process_options_t *process, tt_pty_exit_cb exit_cb) {
  handle->flags = 0;
  handle->exit_status = 0;
  handle->term_signal = 0;
  handle->active = 0;
  handle->on_exit = exit_cb;

  handle->in.data = handle;
  handle->out.data = handle;
  handle->exit.data = handle;

  int err;

  err = tt_console_init(handle);
  if (err < 0) goto err;

  int width = term ? term->width : 80;
  int height = term ? term->height : 60;

  COORD size = {
    .X = width,
    .Y = height,
  };

  HANDLE in = NULL, out = NULL;

  err = tt_console_open(handle, size, &in, &out);
  if (err < 0) goto err;

  err = tt_prepare_console_info(handle);
  if (err < 0) goto err;

  PWCHAR cmd = NULL, env = NULL, cwd = NULL;

  err = tt_prepare_console_command(process, &cmd);
  if (err < 0) goto err;

  err = tt_prepare_console_environment(process, &env);
  if (err < 0) goto err;

  err = tt_prepare_console_directory(process, &cwd);
  if (err < 0) goto err;

  err = tt_console_spawn(handle, cmd, env, cwd, in, out);
  if (err < 0) goto err;

  free(cmd);
  free(env);
  free(cwd);

  handle->width = width;
  handle->height = height;

  err = uv_async_init(loop, &handle->exit, on_process_exit);
  assert(err == 0);
  handle->active++;

  err = uv_pipe_init(loop, &handle->in, 0);
  assert(err == 0);
  handle->active++;

  err = uv_pipe_open(&handle->in, uv_open_osfhandle(handle->console.in));
  assert(err == 0);
  handle->console.in = NULL;

  err = uv_pipe_init(loop, &handle->out, 0);
  assert(err == 0);
  handle->active++;

  err = uv_pipe_open(&handle->out, uv_open_osfhandle(handle->console.out));
  assert(err == 0);
  handle->console.out = NULL;

  err = uv_thread_create(&handle->thread, wait_for_exit, (void *) handle);
  assert(err == 0);

  return 0;

err:
  if (in) CloseHandle(in);
  if (out) CloseHandle(out);

  if (cmd) free(cmd);
  if (env) free(env);
  if (cwd) free(cwd);

  tt_console_destroy(handle);

  return err;
}

static void
on_read (uv_stream_t *uv_stream, ssize_t read_len, const uv_buf_t *buf) {
  tt_pty_t *handle = (tt_pty_t *) uv_stream->data;

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

  return uv_read_start((uv_stream_t *) &handle->out, on_alloc, on_read);
}

int
tt_pty_read_stop (tt_pty_t *handle) {
  if (!(handle->flags & TT_PTY_READING)) {
    return 0;
  }

  handle->flags &= ~TT_PTY_READING;
  handle->on_read = NULL;

  return uv_read_stop((uv_stream_t *) &handle->out);
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

  return uv_write(&req->req, (uv_stream_t *) &handle->in, bufs, bufs_len, on_write);
}

int
tt_pty_resize (tt_pty_t *handle, int width, int height) {
  COORD size = {
    .X = width,
    .Y = height,
  };

  HRESULT res = ResizePseudoConsole(handle->console.handle, size);

  if (res < 0) {
    SetLastError(res);
    goto err;
  }

  handle->width = width;
  handle->height = height;

  return 0;

err:
  return uv_translate_sys_error(GetLastError());
}

void
tt_pty_close (tt_pty_t *handle, tt_pty_close_cb cb) {
  handle->on_close = cb;

  uv_close((uv_handle_t *) &handle->in, on_close);
  uv_close((uv_handle_t *) &handle->out, on_close);
}

int
tt_pty_kill (tt_pty_t *handle, int signum) {
  handle->term_signal = signum;

  return uv_kill(handle->pid, signum);
}
