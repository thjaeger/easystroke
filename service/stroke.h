#pragma once

#include <vector>

/**
 * Represents a raw cursor position, where x and y are relative to the screen.
 * t is the time that it moved into that position (according to eg. X server), or zero if unknown - they are only used
 * relatively, for e.g. timeouts from the first cursor position to the last.
 */
struct CursorPosition {
public:
    CursorPosition() = default;
    CursorPosition(double x, double y, long t): x(x), y(y), t(t) {}

    double x;
    double y;
    long t;
};

/**
 * Represents an abstract stroke position, where x and y are only relative to other points in the same stroke.
 * This allows efficient comparison of two strokes.
 */
struct StrokePoint {
public:
    StrokePoint(double x, double y): x(x), y(y), t(0), dt(0), alpha(0) {};

    double x;
    double y;
    double t;
    double dt;
    double alpha;
};

/**
 * Represents an abstract stroke of a cursor - i.e. a theoretical path from point A to point B.
 * Points are only relative to each other, which allows us to compare strokes that are similar but of different sizes.
 */
class Stroke {
private:
    std::vector<StrokePoint> p;

public:
    explicit Stroke(const std::vector<CursorPosition>& points);

    static double compare(const Stroke& a, const Stroke& b, int *path_x, int *path_y);

    [[nodiscard]] std::size_t size() const { return p.size(); }
};

extern const double stroke_infinity;
