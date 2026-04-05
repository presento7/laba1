#include "ray.h"

#include <cmath>

namespace {

constexpr double kEpsilon = 1e-9;

double ComputeAngle(const QPointF& begin, const QPointF& end) {
    return std::atan2(end.y() - begin.y(), end.x() - begin.x());
}

double Length(const QPointF& vector) {
    return std::hypot(vector.x(), vector.y());
}

QPointF VectorFromAngle(double angle, double length) {
    return QPointF(std::cos(angle) * length, std::sin(angle) * length);
}

}  // namespace

Ray::Ray(const QPointF& begin, const QPointF& end, double angle)
    : begin_(begin), end_(end), angle_(angle) {
    if (Length(end_ - begin_) > kEpsilon) {
        angle_ = ComputeAngle(begin_, end_);
    }
}

const QPointF& Ray::GetBegin() const {
    return begin_;
}

void Ray::SetBegin(const QPointF& begin) {
    begin_ = begin;
    if (Length(end_ - begin_) > kEpsilon) {
        angle_ = ComputeAngle(begin_, end_);
    }
}

const QPointF& Ray::GetEnd() const {
    return end_;
}

void Ray::SetEnd(const QPointF& end) {
    end_ = end;
    if (Length(end_ - begin_) > kEpsilon) {
        angle_ = ComputeAngle(begin_, end_);
    }
}

double Ray::GetAngle() const {
    return angle_;
}

void Ray::SetAngle(double angle) {
    angle_ = angle;
}

Ray Ray::Rotate(double angle) const {
    double length = Length(end_ - begin_);
    if (length < kEpsilon) {
        length = 1.0;
    }
    double rotated_angle = angle_ + angle;
    return Ray(begin_, begin_ + VectorFromAngle(rotated_angle, length), rotated_angle);
}
