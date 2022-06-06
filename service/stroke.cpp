#include "stroke.h"
#include <cmath>

const double stroke_infinity = 0.2;
#define EPS 0.000001

inline double angle_difference(double alpha, double beta) {
    double d = alpha - beta;
    if (d < -1.0)
        d += 2.0;
    else if (d > 1.0)
        d -= 2.0;
    return d;
}

Stroke::Stroke(const std::vector<CursorPosition>& points) {
    p.reserve(points.size());
    for (auto const& cp : points) {
        p.emplace_back(cp.x, cp.y);
    }

	int n = this->p.size() - 1;
	double total = 0.0;
	p[0].t = 0.0;
	for (int i = 0; i < n; i++) {
		total += hypot(p[i+1].x - p[i].x, p[i+1].y - p[i].y);
		this->p[i+1].t = total;
	}
	for (int i = 0; i <= n; i++)
		p[i].t /= total;
	double minX = p[0].x, minY = p[0].y, maxX = minX, maxY = minY;
	for (int i = 1; i <= n; i++) {
		if (p[i].x < minX) minX = p[i].x;
		if (p[i].x > maxX) maxX = p[i].x;
		if (p[i].y < minY) minY = p[i].y;
		if (p[i].y > maxY) maxY = p[i].y;
	}
	double scaleX = maxX - minX;
	double scaleY = maxY - minY;
	double scale = (scaleX > scaleY) ? scaleX : scaleY;
	if (scale < 0.001) scale = 1;
	for (int i = 0; i <= n; i++) {
		p[i].x = (p[i].x-(minX+maxX)/2)/scale + 0.5;
		p[i].y = (p[i].y-(minY+maxY)/2)/scale + 0.5;
	}

	for (int i = 0; i < n; i++) {
		p[i].dt = p[i+1].t - p[i].t;
		p[i].alpha = atan2(p[i+1].y - p[i].y, p[i+1].x - p[i].x)/M_PI;
	}
}

inline static double sqr(double x) { return x*x; }

inline void step(
    const std::vector<StrokePoint>& a,
    const std::vector<StrokePoint>& b,
    const int N,
    double *dist,
    int *prev_x,
    int *prev_y,
    const int x,
    const int y,
    const double tx,
    const double ty,
    int *k,
    const int x2,
    const int y2) {

    double dtx = a[x2].t - tx;
    double dty = b[y2].t - ty;
    if (dtx >= dty * 2.2 || dty >= dtx * 2.2 || dtx < EPS || dty < EPS)
        return;
    (*k)++;

    double d = 0.0;
    int i = x, j = y;
    double next_tx = (a[i + 1].t - tx) / dtx;
    double next_ty = (b[j + 1].t - ty) / dty;
    double cur_t = 0.0;

    for (;;) {
        double ad = sqr(angle_difference(a[i].alpha, b[j].alpha));
        double next_t = next_tx < next_ty ? next_tx : next_ty;
        bool done = next_t >= 1.0 - EPS;
        if (done)
            next_t = 1.0;
        d += (next_t - cur_t) * ad;
        if (done)
            break;
        cur_t = next_t;
        if (next_tx < next_ty)
            next_tx = (a[++i + 1].t - tx) / dtx;
        else
            next_ty = (b[++j + 1].t - ty) / dty;
    }
    double new_dist = dist[x * N + y] + d * (dtx + dty);

    if (new_dist >= dist[x2 * N + y2])
        return;

    prev_x[x2 * N + y2] = x;
    prev_y[x2 * N + y2] = y;
    dist[x2 * N + y2] = new_dist;
}

/* To compare two gestures, we use dynamic programming to minimize (an
 * approximation) of the integral over square of the angle difference among
 * (roughly) all reparametrizations whose slope is always between 1/2 and 2.
 */
double Stroke::compare(const Stroke& a, const Stroke& b, int *path_x, int *path_y) {
	const int M = a.p.size();
	const int N = b.p.size();
	const int m = M - 1;
	const int n = N - 1;

	double dist[M * N];
	int prev_x[M * N];
	int prev_y[M * N];

    for (int i = 0; i < m; i++)
        for (int j = 0; j < n; j++)
            dist[i*N+j] = stroke_infinity;

    dist[M*N-1] = stroke_infinity;
	dist[0] = 0.0;

	for (int x = 0; x < m; x++) {
        for (int y = 0; y < n; y++) {
            if (dist[x * N + y] >= stroke_infinity)
                continue;
            double tx = a.p[x].t;
            double ty = b.p[y].t;
            int max_x = x;
            int max_y = y;
            int k = 0;

            while (k < 4) {
                if (a.p[max_x + 1].t - tx > b.p[max_y + 1].t - ty) {
                    max_y++;
                    if (max_y == n) {
                        step(a.p, b.p, N, dist, prev_x, prev_y, x, y, tx, ty, &k, m, n);
                        break;
                    }
                    for (int x2 = x + 1; x2 <= max_x; x2++)
                        step(a.p, b.p, N, dist, prev_x, prev_y, x, y, tx, ty, &k, x2, max_y);
                } else {
                    max_x++;
                    if (max_x == m) {
                        step(a.p, b.p, N, dist, prev_x, prev_y, x, y, tx, ty, &k, m, n);
                        break;
                    }
                    for (int y2 = y + 1; y2 <= max_y; y2++)
                        step(a.p, b.p, N, dist, prev_x, prev_y, x, y, tx, ty, &k, max_x, y2);
                }
            }
        }
    }

	double cost = dist[M*N-1];
	if (path_x && path_y) {

		if (cost < stroke_infinity) {
			int x = m;
			int y = n;
			int k = 0;
			while (x || y) {
				int old_x = x;
				x = prev_x[x*N+y];
				y = prev_y[old_x*N+y];
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
