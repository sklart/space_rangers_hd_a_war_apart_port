/* Test-link stubs for codecs deliberately not exercised by the BMP fixture.
 * The production build links full OKGF; these only let image_read.c be tested
 * on a Windows host which does not provide libjpeg development files. */
#include "okgf.h"

OkgfPngReadContext *OKGF_CALL OKGR_ReadStart_PNG_Buf(const uint8_t *s, int32_t n, int32_t *w, int32_t *h) { (void)s; (void)n; (void)w; (void)h; return 0; }
OkgfPngReadContext *OKGF_CALL OKGR_ReadStart_PNGPAL_Buf(const uint8_t *s, int32_t n, int32_t *w, int32_t *h, int32_t *p) { (void)s; (void)n; (void)w; (void)h; (void)p; return 0; }
int32_t OKGF_CALL OKGF_Read_PNG(OkgfPngReadContext *c, void *p, int32_t q, uint32_t r, uint32_t g, uint32_t b, uint32_t a, int32_t z) { (void)c; (void)p; (void)q; (void)r; (void)g; (void)b; (void)a; (void)z; return 0; }
int32_t OKGF_CALL OKGF_Read_PNGPAL(OkgfPngReadContext *c, void *p, int32_t q, void *v) { (void)c; (void)p; (void)q; (void)v; return 0; }
void OKGF_CALL okgf_cancel_read_png(OkgfPngReadContext *c) { (void)c; }

OkgfJpegReadContext *OKGF_CALL OKGR_ReadStart_JPEG_Buf(const uint8_t *s, int32_t n, int32_t *w, int32_t *h) { (void)s; (void)n; (void)w; (void)h; return 0; }
int32_t OKGF_CALL OKGF_Read_JPEG(OkgfJpegReadContext *c, void *p, int32_t q) { (void)c; (void)p; (void)q; return 0; }
void OKGF_CALL okgf_cancel_read_jpeg(OkgfJpegReadContext *c) { (void)c; }

OkgfPsdReadContext *OKGF_CALL OKGR_ReadStart_PSD_Buf(const uint8_t *s, int32_t n, int32_t *w, int32_t *h) { (void)s; (void)n; (void)w; (void)h; return 0; }
OkgfPsdReadContext *OKGF_CALL OKGR_ReadStart_PSDPAL_Buf(const uint8_t *s, int32_t n, int32_t *w, int32_t *h, int32_t *p) { (void)s; (void)n; (void)w; (void)h; (void)p; return 0; }
int32_t OKGF_CALL OKGF_Read_PSD(OkgfPsdReadContext *c, void *p, int32_t q) { (void)c; (void)p; (void)q; return 0; }
int32_t OKGF_CALL OKGF_Read_PSDPAL(OkgfPsdReadContext *c, void *p, int32_t q, void *v) { (void)c; (void)p; (void)q; (void)v; return 0; }
void OKGF_CALL okgf_cancel_read_psd(OkgfPsdReadContext *c) { (void)c; }
