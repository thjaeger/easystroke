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
	double minX = s->p[0].x, minY = s->p[0].y, maxX = minX, maxY = minY;
	for (int i = 1; i <= n; i++) {
		if (s->p[i].x < minX) minX = s->p[i].x;
		if (s->p[i].x > maxX) maxX = s->p[i].x;
		if (s->p[i].y < minY) minY = s->p[i].y;
		if (s->p[i].y > maxY) maxY = s->p[i].y;
	}
	double scaleX = maxX - minX;
	double scaleY = maxY - minY;
	double scale = (scaleX > scaleY) ? scaleX : scaleY;
	if (scale < 0.001) scale = 1;
	for (int i = 0; i <= n; i++) {
		s->p[i].x = (s->p[i].x-(minX+maxX)/2)/scale + 0.5;
		s->p[i].y = (s->p[i].y-(minY+maxY)/2)/scale + 0.5;
	}

	for (int i = 0; i < n; i++) {
		s->p[i].dt = s->p[i+1].t - s->p[i].t;
		s->p[i].alpha = atan2(s->p[i+1].y - s->p[i].y, s->p[i+1].x - s->p[i].x)/M_PI;
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
	int n = 16;
	int m = n;
	double dt = 1.0/(double)n;
	double dist[m+1][n+1];
	int prev_x[m+1][n+1];
	int prev_y[m+1][n+1];

	for (int i = 0; i < m; i++)
		for (int j = 0; j < n; j++)
			dist[i][j] = stroke_infinity;
	dist[m][n] = stroke_infinity;
	dist[0][0] = 0.0;

	double ai[m];
	double bj[n];

	for (int x = 0, i = 0; x < m; x++) {
		while (a->p[i+1].t < dt*x) i++;
		ai[x] = i;
	}
	for (int y = 0, j = 0; y < n; y++) {
		while (b->p[j+1].t < dt*y) j++;
		bj[y] = j;
	}

	for (int x = 0; x < m; x++) {
		for (int y = 0; y < n; y++) {
			if (dist[x][y] >= stroke_infinity)
				continue;
			double tx  = dt * x;
			double ty  = dt * y;

			inline void step(int x2, int y2) {
				double d = 0.0;
				int i = ai[x], j = bj[y];

				double next_tx = (a->p[i+1].t - tx) / (x2-x);
				double next_ty = (b->p[j+1].t - ty) / (y2-y);
				double cur_t = 0.0;

				for (;;) {
					double ad = sqr(angle_difference(a->p[i].alpha, b->p[j].alpha));
					double next_t = next_tx < next_ty ? next_tx : next_ty;
					bool done = next_t >= dt - EPS;
					if (done)
						next_t = dt;
					d += (next_t - cur_t)*ad;
					if (done)
						break;
					cur_t = next_t;
					if (next_tx < next_ty)
						next_tx = (a->p[++i+1].t - tx) / (x2-x);
					else
						next_ty = (b->p[++j+1].t - ty) / (y2-y);
				}

				double new_dist = dist[x][y] + d * (x2-x + y2-y);
				if (new_dist != new_dist) abort();

				if (new_dist >= dist[x2][y2])
					return;

				prev_x[x2][y2] = x;
				prev_y[x2][y2] = y;
				dist[x2][y2] = new_dist;
			}

			step(x+1,y+1);
			if (x+1 < m)
				step(x+2, y+1);
			if (y+1 < n)
				step(x+1, y+2);
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
