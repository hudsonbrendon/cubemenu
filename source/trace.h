#pragma once
/* Start-up trace: each step is shown on screen and, once the card is mounted, appended to
 * /cubemenu/boot.log, so a hang on a real console can be located. */
void trace(const char *fmt, ...);
void trace_card_ready(void);     /* card mounted: flush earlier steps to the log */
void trace_video_ready(void);    /* GX is up: start drawing */
void trace_done(void);           /* menu is up: stop drawing and logging */
