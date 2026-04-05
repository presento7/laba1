#include "controller.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr double kAngleOffset = 0.0001;
constexpr double kRayMergeDistance = 1.0;
constexpr double kPointEqualityDistance = 1.0;
constexpr double kLightMargin = 12.0;

double NormalizeAngle(double angle) {
    constexpr double kTwoPi = 6.28318530717958647692;
    while (angle < 0.0) {
        angle += kTwoPi;
    }
    while (angle >= kTwoPi) {
        angle -= kTwoPi;
    }
    return angle;
}

double DistanceSquared(const QPointF& lhs, const QPointF& rhs) {
    const double dx = lhs.x() - rhs.x();
    const double dy = lhs.y() - rhs.y();
    return dx * dx + dy * dy;
}

QPointF PointOnRay(const QPointF& begin, double angle, double distance) {
    return QPointF(begin.x() + std::cos(angle) * distance, begin.y() + std::sin(angle) * distance);
}

bool PointsClose(const QPointF& lhs, const QPointF& rhs, double tolerance) {
    return DistanceSquared(lhs, rhs) <= tolerance * tolerance;
}

}  // namespace

Controller::Controller()
    : light_source_(320.0, 240.0)
    , light_source_offsets_({
        QPointF(0.0, 0.0),
        QPointF(-6.0, -3.0),
        QPointF(6.0, -3.0),
        QPointF(-4.0, 5.0),
        QPointF(4.0, 5.0),
      }) {
    UpdateLightSources();
}

const std::vector<Polygon>& Controller::GetPolygons() const {
    return polygons_;
}

void Controller::AddPolygon(const Polygon& polygon) {
    polygons_.push_back(polygon);
}

void Controller::AddVertexToLastPolygon(const QPointF& new_vertex) {
    if (!polygons_.empty()) {
        polygons_.back().AddVertex(new_vertex);
    }
}

void Controller::UpdateLastPolygon(const QPointF& new_vertex) {
    if (!polygons_.empty()) {
        polygons_.back().UpdateLastVertex(new_vertex);
    }
}

void Controller::FinishLastPolygon() {
    if (polygons_.empty()) {
        return;
    }

    Polygon& polygon = polygons_.back();
    if (polygon.Size() >= 2) {
        const auto& vertices = polygon.GetVertices();
        if (PointsClose(
                vertices[polygon.Size() - 1], vertices[polygon.Size() - 2],
                kPointEqualityDistance)) {
            polygon.RemoveLastVertex();
        }
    }

    if (polygon.Size() >= 2) {
        const auto& vertices = polygon.GetVertices();
        if (PointsClose(vertices.front(), vertices.back(), kPointEqualityDistance)) {
            polygon.RemoveLastVertex();
        }
    }

    if (polygon.Size() < 3) {
        polygons_.pop_back();
    }
}

QPointF Controller::GetLightSource() const {
    return light_source_;
}

void Controller::SetLightSource(const QPointF& light_source) {
    light_source_ = ClampToCanvas(light_source);
    UpdateLightSources();
}

const std::vector<QPointF>& Controller::GetLightSources() const {
    return light_sources_;
}

void Controller::SetCanvasBounds(const QRectF& canvas_bounds) {
    canvas_bounds_ = canvas_bounds;
    RebuildBoundaryPolygon();
    light_source_ = ClampToCanvas(light_source_);
    UpdateLightSources();
}

std::vector<Ray> Controller::CastRays() const {
    return CastRays(light_source_);
}

std::vector<Ray> Controller::CastRays(const QPointF& source) const {
    std::vector<Ray> rays;
    const double max_ray_length = MaxRayLength();

    auto append_rays_for_polygon = [&](const Polygon& polygon) {
        for (const QPointF& vertex : polygon.GetVertices()) {
            if (PointsClose(source, vertex, 0.5)) {
                continue;
            }

            const double angle = std::atan2(vertex.y() - source.y(), vertex.x() - source.x());
            const Ray direct_ray(source, vertex, angle);
            rays.push_back(direct_ray);

            Ray clockwise_ray = direct_ray.Rotate(-kAngleOffset);
            clockwise_ray.SetEnd(PointOnRay(source, clockwise_ray.GetAngle(), max_ray_length));
            rays.push_back(clockwise_ray);

            Ray counterclockwise_ray = direct_ray.Rotate(kAngleOffset);
            counterclockwise_ray.SetEnd(
                PointOnRay(source, counterclockwise_ray.GetAngle(), max_ray_length));
            rays.push_back(counterclockwise_ray);
        }
    };

    append_rays_for_polygon(boundary_polygon_);
    for (const Polygon& polygon : polygons_) {
        append_rays_for_polygon(polygon);
    }
    return rays;
}

void Controller::IntersectRays(std::vector<Ray>* rays) const {
    if (rays == nullptr) {
        return;
    }

    for (Ray& ray : *rays) {
        double closest_distance = std::sqrt(DistanceSquared(ray.GetBegin(), ray.GetEnd()));
        QPointF closest_point = ray.GetEnd();

        auto update_from_polygon = [&](const Polygon& polygon) {
            const auto intersection = polygon.IntersectRay(ray);
            if (!intersection.has_value()) {
                return;
            }

            const double intersection_distance =
                std::sqrt(DistanceSquared(ray.GetBegin(), *intersection));
            if (intersection_distance < closest_distance) {
                closest_distance = intersection_distance;
                closest_point = *intersection;
            }
        };

        update_from_polygon(boundary_polygon_);
        for (const Polygon& polygon : polygons_) {
            update_from_polygon(polygon);
        }

        ray.SetEnd(closest_point);
    }
}

void Controller::RemoveAdjacentRays(std::vector<Ray>* rays) const {
    if (rays == nullptr || rays->empty()) {
        return;
    }

    std::vector<Ray> filtered_rays;
    filtered_rays.reserve(rays->size());

    for (const Ray& ray : *rays) {
        if (filtered_rays.empty() ||
            !PointsClose(filtered_rays.back().GetEnd(), ray.GetEnd(), kRayMergeDistance)) {
            filtered_rays.push_back(ray);
        }
    }

    if (filtered_rays.size() > 1 &&
        PointsClose(
            filtered_rays.front().GetEnd(), filtered_rays.back().GetEnd(), kRayMergeDistance)) {
        filtered_rays.pop_back();
    }

    *rays = std::move(filtered_rays);
}

Polygon Controller::CreateLightArea() const {
    return CreateLightArea(light_source_);
}

Polygon Controller::CreateLightArea(const QPointF& source) const {
    std::vector<Ray> rays = CastRays(source);
    IntersectRays(&rays);

    std::sort(rays.begin(), rays.end(), [](const Ray& lhs, const Ray& rhs) {
        return NormalizeAngle(lhs.GetAngle()) < NormalizeAngle(rhs.GetAngle());
    });
    RemoveAdjacentRays(&rays);

    std::vector<QPointF> vertices;
    vertices.reserve(rays.size());
    for (const Ray& ray : rays) {
        vertices.push_back(ray.GetEnd());
    }
    return Polygon(vertices);
}

std::vector<Polygon> Controller::CreateLightAreas() const {
    std::vector<Polygon> areas;
    areas.reserve(light_sources_.size());
    for (const QPointF& source : light_sources_) {
        areas.push_back(CreateLightArea(source));
    }
    return areas;
}

double Controller::MaxRayLength() const {
    if (!canvas_bounds_.isValid()) {
        return 2000.0;
    }
    return std::hypot(canvas_bounds_.width(), canvas_bounds_.height()) + 32.0;
}

QPointF Controller::ClampToCanvas(const QPointF& point) const {
    if (!canvas_bounds_.isValid()) {
        return point;
    }

    const double left = std::min(canvas_bounds_.left() + kLightMargin, canvas_bounds_.right());
    const double right = std::max(canvas_bounds_.right() - kLightMargin, canvas_bounds_.left());
    const double top = std::min(canvas_bounds_.top() + kLightMargin, canvas_bounds_.bottom());
    const double bottom = std::max(canvas_bounds_.bottom() - kLightMargin, canvas_bounds_.top());

    return QPointF(std::clamp(point.x(), left, right), std::clamp(point.y(), top, bottom));
}

void Controller::RebuildBoundaryPolygon() {
    if (!canvas_bounds_.isValid()) {
        boundary_polygon_ = Polygon();
        return;
    }

    boundary_polygon_ = Polygon({
      canvas_bounds_.topLeft(),
      canvas_bounds_.topRight(),
      canvas_bounds_.bottomRight(),
      canvas_bounds_.bottomLeft(),
    });
}

void Controller::UpdateLightSources() {
    light_sources_.clear();
    light_sources_.reserve(light_source_offsets_.size());
    for (const QPointF& offset : light_source_offsets_) {
        light_sources_.push_back(ClampToCanvas(light_source_ + offset));
    }
}
