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

				inline double ad(int i, int j) {return sqr(angle_difference(a->p[i].alpha, b->p[j].alpha));}
				double d = (a->p[x].dt + b->p[y].dt) * ad(x,y);
				for (int x_ = x+1; x_ < x2; x_++)
					d += a->p[x_].dt * ad(x_,y);
				for (int y_ = y+1; y_ < y2; y_++)
					d += b->p[y_].dt * ad(x, y_);
				double new_dist = dist[x][y] + d;
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
