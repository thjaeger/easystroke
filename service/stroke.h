#pragma once

#ifdef  __cplusplus
extern "C" {
#endif

struct _stroke_t;

typedef struct _stroke_t stroke_t;

stroke_t *stroke_alloc(int n);
void stroke_add_point(stroke_t *stroke, double x, double y);
void stroke_finish(stroke_t *stroke);
void stroke_free(stroke_t *stroke);

int stroke_get_size(const stroke_t *stroke);
void stroke_get_point(const stroke_t *stroke, int n, double *x, double *y);
double stroke_get_time(const stroke_t *stroke, int n);
double stroke_get_angle(const stroke_t *stroke, int n);
double stroke_angle_difference(const stroke_t *a, const stroke_t *b, int i, int j);

double stroke_compare(const stroke_t *a, const stroke_t *b, int *path_x, int *path_y);

extern const double stroke_infinity;

#ifdef  __cplusplus
}
#endif
