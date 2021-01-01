#pragma once

#include <vector>

struct Point {
public:
    Point(double x, double y): x(x), y(y), t(0), dt(0), alpha(0) {};

    double x;
    double y;
    double t;
    double dt;
    double alpha;
};

class Stroke2 {
private:
    std::vector<Point> p;

    static inline void step(
            const Stroke2 &a,
            const Stroke2 &b,
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
            const int y2);

public:
    void addPoint(double x, double y);

    int size();

    void finish();

    static double compare(const Stroke2 &a, const Stroke2 &b, int *path_x, int *path_y);
};

extern const double stroke_infinity;
