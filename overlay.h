#pragma once
#ifdef __cplusplus
extern "C" {
#endif

void overlay_start();
void overlay_redraw();
void overlay_stop();
void overlay_run();
void overlay_step(double seconds);

// Existing single-text API (still works)
void overlay_set_text_utf8(const char* utf8);
void overlay_clear_text();
void overlay_set_text_size(double points);
void overlay_set_text_color(double r, double g, double b, double a);
void overlay_set_text_position(double x, double y);

// NEW: multi-label API
void overlay_label_set(const char* key, const char* utf8,
                       double x, double y, double points,
                       double r, double g, double b, double a);
void overlay_label_remove(const char* key);
void overlay_labels_clear();

// NEW: rectangle API (strokeAlpha <= 0 disables stroke, fillAlpha <= 0 disables fill)
void overlay_rect_set(const char* key,
                      double x, double y, double width, double height,
                      double strokeWidth,
                      double strokeR, double strokeG, double strokeB, double strokeA,
                      double fillR, double fillG, double fillB, double fillA);
void overlay_rect_remove(const char* key);
void overlay_rects_clear();

#ifdef __cplusplus
}
#endif