/* The M21 release baseline is PNG.  Keep the host release probe PNG-only so
 * it does not require JPEG development headers merely to exercise OKGF's
 * signature dispatch in image_read.c. */
#include "okgf.h"

OkgfJpegReadContext *OKGF_CALL OKGR_ReadStart_JPEG_Buf(const uint8_t *s, int32_t n, int32_t *w, int32_t *h) { (void)s; (void)n; (void)w; (void)h; return 0; }
int32_t OKGF_CALL OKGF_Read_JPEG(OkgfJpegReadContext *c, void *p, int32_t q) { (void)c; (void)p; (void)q; return 0; }
void OKGF_CALL okgf_cancel_read_jpeg(OkgfJpegReadContext *c) { (void)c; }
