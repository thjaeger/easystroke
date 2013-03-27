/*
 * Copyright (c) 2009, Thomas Jaeger <ThJaeger@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#define _GNU_SOURCE

#include "stroke.h"
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <stdbool.h>

const double stroke_infinity = 0.2;
#define EPS 0.000001

struct point {
	double x;
	double y;
	double t;
	double dt;
	double alpha;
};

struct _stroke_t {
	int n;
	int capacity;
	struct point *p;
	double minX;
	double maxX;
	double minY;
	double maxY;
};

stroke_t *stroke_alloc(int n) {
	assert(n > 0);
	stroke_t *s = malloc(sizeof(stroke_t));
	s->n = 0;
	s->capacity = n;
	s->p = calloc(n, sizeof(struct point));
	return s;
}

void stroke_add_point(stroke_t *s, double x, double y) {
	assert(s->capacity > s->n);
	s->p[s->n].x = x;
	s->p[s->n].y = y;
	if (s->n==0) {
		s->minX = x; s->maxX = x; s->minY = y; s->maxY = y;
	} else {
		if (x < s->minX) s->minX = x;
		if (x > s->maxX) s->maxX = x;
		if (y < s->minY) s->minY = y;
		if (y > s->maxY) s->maxY = y;
	}
	s->n++;
}

inline static double angle_difference(double alpha, double beta) {
	// return 1.0 - cos((alpha - beta) * M_PI);
	double d = alpha - beta;
	if (d < -1.0)
		d += 2.0;
	else if (d > 1.0)
		d -= 2.0;
	return d;
}
double stroke_radians_difference(double alpha, double beta) {
	return angle_difference(alpha, beta);
}

double stroke_calc_angle(struct point *p, struct point *q) {
	return atan2(p->y - q->y, p->x - q->x)/M_PI;
}

/* subdivide the square with 5x5 grid */
void point_position( const struct point *p, int *grid_x, int *grid_y) {
	*grid_x = p->x * 5.0;
	*grid_y = p->y * 5.0;
}

#include <stdio.h>

double how_compatible_points(const struct point *p1, const struct point *p2) {
	int grid_1_x, grid_1_y, grid_2_x, grid_2_y;
	int diff_x, diff_y;
	double compatible = 0.0;
	point_position( p1, &grid_1_x, &grid_1_y );
	point_position( p2, &grid_2_x, &grid_2_y );
	diff_x = abs(grid_1_x - grid_2_x);
	diff_y = abs(grid_1_y - grid_2_y);
	switch(diff_x){
	case 0: compatible += 1.0; break;
	case 1: compatible += 0.5; break;
	}
	switch(diff_y){
	case 0: compatible += 1.0; break;
	case 1: compatible += 0.5; break;
	}
	return compatible / 2.0;
}

double stroke_how_compatible(const stroke_t *stroke, const stroke_t *stroke2) {
	double compatible_start = how_compatible_points(&stroke->p[0],&stroke2->p[0]);
	double compatible_end = how_compatible_points(&stroke->p[stroke->n - 1],&stroke2->p[stroke2->n - 1]);

	return ( compatible_start + compatible_end ) / 2.0;
}

double stroke_dist_start(const stroke_t *stroke, const stroke_t *stroke2) {
	return fabs(hypot(stroke->p[0].x - stroke2->p[0].x, stroke->p[0].y - stroke2->p[0].y));
}

double stroke_dist_middle(const stroke_t *stroke, const stroke_t *stroke2) {
	return fabs(hypot(stroke->p[stroke->n / 2].x - stroke2->p[stroke2->n / 2].x, stroke->p[stroke->n / 2].y - stroke2->p[stroke2->n / 2].y));
}

double stroke_dist_end(const stroke_t *stroke, const stroke_t *stroke2) {
	return fabs(hypot(stroke->p[stroke->n - 1].x - stroke2->p[stroke2->n - 1].x, stroke->p[stroke->n - 1].y - stroke2->p[stroke2->n - 1].y));
}

double stroke_orient_start(const stroke_t *stroke, const stroke_t *stroke2) {
	return stroke_calc_angle(&stroke->p[0],&stroke2->p[0]);
}

double stroke_orient_middle(const stroke_t *stroke, const stroke_t *stroke2) {
	return stroke_calc_angle(&stroke->p[stroke->n / 2],&stroke2->p[stroke2->n / 2]);
}

double stroke_orient_end(const stroke_t *stroke, const stroke_t *stroke2) {
	return stroke_calc_angle(&stroke->p[stroke->n - 1],&stroke2->p[stroke2->n - 1]);
}

void stroke_normalize(stroke_t *s,stroke_t *s2) {
	if (s && s2){
		double minX = s->minX, minY = s->minY, maxX = s->maxX, maxY = s->maxY;
		if (s2->minX < s->minX) minX = s2->minX;
		if (s2->minY < s->minY) minY = s2->minY;
		if (s2->maxX > s->maxX) maxX = s2->maxX;
		if (s2->maxY > s->maxY) maxY = s2->maxY;
		s->minX = s2->minX = minX;
		s->minY = s2->minY = minY;
		s->maxX = s2->maxX = maxX;
		s->maxY = s2->maxY = maxY;
	}
}

void stroke_finish(stroke_t *s) {
	assert(s->capacity > 0);
	s->capacity = -1;

	int n = s->n - 1;
	double total = 0.0;
	s->p[0].t = 0.0;
	for (int i = 0; i < n; i++) {
		total += hypot(s->p[i+1].x - s->p[i].x, s->p[i+1].y - s->p[i].y);
		s->p[i+1].t = total;
	}
	for (int i = 0; i <= n; i++)
		s->p[i].t /= total;
	double scaleX = s->maxX - s->minX;
	double scaleY = s->maxY - s->minY;
	double scale = (scaleX > scaleY) ? scaleX : scaleY;
	if (scale < 0.001) scale = 1;
	for (int i = 0; i <= n; i++) {
		s->p[i].x = (s->p[i].x-(s->minX+s->maxX)/2)/scale + 0.5;
		s->p[i].y = (s->p[i].y-(s->minY+s->maxY)/2)/scale + 0.5;
	}

	for (int i = 0; i < n; i++) {
		s->p[i].dt = s->p[i+1].t - s->p[i].t;
		s->p[i].alpha = stroke_calc_angle(&s->p[i+1], &s->p[i])/M_PI;
	}

}

void stroke_free(stroke_t *s) {
	if (s)
		free(s->p);
	free(s);
}

int stroke_get_size(const stroke_t *s) { return s->n; }

void stroke_get_point(const stroke_t *s, int n, double *x, double *y) {
	assert(n < s->n);
	if (x)
		*x = s->p[n].x;
	if (y)
		*y = s->p[n].y;
}

double stroke_get_time(const stroke_t *s, int n) {
	assert(n < s->n);
	return s->p[n].t;
}

double stroke_get_angle(const stroke_t *s, int n) {
	assert(n+1 < s->n);
	return s->p[n].alpha;
}

inline static double sqr(double x) { return x*x; }

double stroke_angle_difference(const stroke_t *a, const stroke_t *b, int i, int j) {
	return fabs(angle_difference(stroke_get_angle(a, i), stroke_get_angle(b, j)));
}

/* To compare two gestures, we use dynamic programming to minimize (an
 * approximation) of the integral over square of the angle difference among
 * (roughly) all reparametrizations whose slope is always between 1/2 and 2.
 */
double stroke_compare(const stroke_t *a, const stroke_t *b, int *path_x, int *path_y) {
	int m = a->n - 1;
	int n = b->n - 1;

	double dist[m+1][n+1];
	int prev_x[m+1][n+1];
	int prev_y[m+1][n+1];
	for (int i = 0; i < m; i++)
		for (int j = 0; j < n; j++)
			dist[i][j] = stroke_infinity;
	dist[m][n] = stroke_infinity;
	dist[0][0] = 0.0;

	for (int x = 0; x < m; x++) {
		for (int y = 0; y < n; y++) {
			if (dist[x][y] >= stroke_infinity)
				continue;
			double tx  = a->p[x].t;
			double ty  = b->p[y].t;
			int max_x = x;
			int max_y = y;
			int k = 0;

			inline void step(int x2, int y2) {
				double dtx = a->p[x2].t - tx;
				double dty = b->p[y2].t - ty;
				if (dtx >= dty * 2.2 || dty >= dtx * 2.2 || dtx < EPS || dty < EPS)
					return;
				k++;

				double d = 0.0;
				int i = x, j = y;
				double next_tx = (a->p[i+1].t - tx) / dtx;
				double next_ty = (b->p[j+1].t - ty) / dty;
				double cur_t = 0.0;

				for (;;) {
					double ad = sqr(angle_difference(a->p[i].alpha, b->p[j].alpha));
					double next_t = next_tx < next_ty ? next_tx : next_ty;
					bool done = next_t >= 1.0 - EPS;
					if (done)
						next_t = 1.0;
					d += (next_t - cur_t)*ad;
					if (done)
						break;
					cur_t = next_t;
					if (next_tx < next_ty)
						next_tx = (a->p[++i+1].t - tx) / dtx;
					else
						next_ty = (b->p[++j+1].t - ty) / dty;
				}
				double new_dist = dist[x][y] + d * (dtx + dty);
				if (new_dist != new_dist) abort();

				if (new_dist >= dist[x2][y2])
					return;

				prev_x[x2][y2] = x;
				prev_y[x2][y2] = y;
				dist[x2][y2] = new_dist;
			}

			while (k < 4) {
				if (a->p[max_x+1].t - tx > b->p[max_y+1].t - ty) {
					max_y++;
					if (max_y == n) {
						step(m, n);
						break;
					}
					for (int x2 = x+1; x2 <= max_x; x2++)
						step(x2, max_y);
				} else {
					max_x++;
					if (max_x == m) {
						step(m, n);
						break;
					}
					for (int y2 = y+1; y2 <= max_y; y2++)
						step(max_x, y2);
				}
			}
		}
	}
	double cost = dist[m][n];
	if (path_x && path_y) {
		if (cost < stroke_infinity) {
			int x = m;
			int y = n;
			int k = 0;
			while (x || y) {
				int old_x = x;
				x = prev_x[x][y];
				y = prev_y[old_x][y];
				path_x[k] = x;
				path_y[k] = y;
				k++;
			}
		} else {
			path_x[0] = 0;
			path_y[0] = 0;
		}
	}
	return cost;
}
