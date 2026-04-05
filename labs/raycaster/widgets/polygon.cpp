#include "polygon.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

constexpr double kEpsilon = 1e-9;

struct Intersection {
    double distance;
    QPointF point;
};

double Cross(const QPointF& lhs, const QPointF& rhs) {
    return lhs.x() * rhs.y() - lhs.y() * rhs.x();
}

double Dot(const QPointF& lhs, const QPointF& rhs) {
    return lhs.x() * rhs.x() + lhs.y() * rhs.y();
}

double Length(const QPointF& vector) {
    return std::hypot(vector.x(), vector.y());
}

QPointF Normalize(const QPointF& vector) {
    double length = Length(vector);
    if (length < kEpsilon) {
        return QPointF(0.0, 0.0);
    }
    return QPointF(vector.x() / length, vector.y() / length);
}

QPointF DirectionForRay(const Ray& ray) {
    QPointF direction = ray.GetEnd() - ray.GetBegin();
    if (Length(direction) >= kEpsilon) {
        return Normalize(direction);
    }
    return QPointF(std::cos(ray.GetAngle()), std::sin(ray.GetAngle()));
}

std::optional<Intersection> IntersectRayWithSegment(
    const Ray& ray, const QPointF& segment_begin, const QPointF& segment_end) {
    const QPointF direction = DirectionForRay(ray);
    const QPointF segment = segment_end - segment_begin;
    const QPointF between = segment_begin - ray.GetBegin();

    const double ray_cross_segment = Cross(direction, segment);
    const double between_cross_direction = Cross(between, direction);

    if (std::abs(ray_cross_segment) < kEpsilon) {
        if (std::abs(between_cross_direction) >= kEpsilon) {
            return std::nullopt;
        }

        double segment_begin_projection = Dot(segment_begin - ray.GetBegin(), direction);
        double segment_end_projection = Dot(segment_end - ray.GetBegin(), direction);
        if (segment_begin_projection > segment_end_projection) {
            std::swap(segment_begin_projection, segment_end_projection);
        }
        if (segment_end_projection < 0.0) {
            return std::nullopt;
        }

        double distance = std::max(0.0, segment_begin_projection);
        return Intersection{distance, ray.GetBegin() + direction * distance};
    }

    const double ray_distance = Cross(between, segment) / ray_cross_segment;
    const double segment_factor = Cross(between, direction) / ray_cross_segment;

    if (ray_distance < -kEpsilon || segment_factor < -kEpsilon || segment_factor > 1.0 + kEpsilon) {
        return std::nullopt;
    }

    const double clamped_distance = std::max(0.0, ray_distance);
    return Intersection{clamped_distance, ray.GetBegin() + direction * clamped_distance};
}

}  // namespace

Polygon::Polygon(const std::vector<QPointF>& vertices) : vertices_(vertices) {
}

const std::vector<QPointF>& Polygon::GetVertices() const {
    return vertices_;
}

void Polygon::AddVertex(const QPointF& vertex) {
    vertices_.push_back(vertex);
}

void Polygon::UpdateLastVertex(const QPointF& new_vertex) {
    if (!vertices_.empty()) {
        vertices_.back() = new_vertex;
    }
}

void Polygon::RemoveLastVertex() {
    if (!vertices_.empty()) {
        vertices_.pop_back();
    }
}

std::size_t Polygon::Size() const {
    return vertices_.size();
}

bool Polygon::Empty() const {
    return vertices_.empty();
}

std::optional<QPointF> Polygon::IntersectRay(const Ray& ray) const {
    if (vertices_.size() < 2) {
        return std::nullopt;
    }

    const std::size_t edge_count = vertices_.size() >= 3 ? vertices_.size() : 1;
    double closest_distance = std::numeric_limits<double>::infinity();
    std::optional<QPointF> closest_point;

    for (std::size_t index = 0; index < edge_count; ++index) {
        const QPointF& begin = vertices_[index];
        const QPointF& end = vertices_[(index + 1) % vertices_.size()];
        const auto intersection = IntersectRayWithSegment(ray, begin, end);
        if (!intersection.has_value()) {
            continue;
        }
        if (intersection->distance < closest_distance) {
            closest_distance = intersection->distance;
            closest_point = intersection->point;
        }
    }

    return closest_point;
}
