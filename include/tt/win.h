#ifndef TT_WIN_H
#define TT_WIN_H

#include <uv.h>

#include "../tt.h"

struct tt_pty_s {
  uv_tty_t tty;
};

#endif // TT_WIN_H
