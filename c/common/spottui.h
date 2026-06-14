#ifndef HAM_SPOTTUI_H
#define HAM_SPOTTUI_H

#include "spot.h"
#include "config.h"

#define HAM_SPOTTUI_MAX 256

/* Initialize the TUI. `title` goes in the top header.
   `fifo_path` is optional (can be NULL) — when non-NULL, sending a spot to
   qrz writes to this FIFO path. */
int  ham_tui_init(const char *title, const char *fifo_path);
void ham_tui_run(void);
void ham_tui_cleanup(void);

/* Replace the full spot list (producer thread calls this). */
void ham_tui_set_spots(const ham_spot *arr, int n);
/* Prepend a single spot (for streaming sources). */
void ham_tui_add_spot(const ham_spot *s);

/* Ask the TUI to exit from a background thread. */
void ham_tui_request_exit(void);
int  ham_tui_running(void);

#endif
