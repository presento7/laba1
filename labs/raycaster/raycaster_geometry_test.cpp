#include "widgets/controller.h"
#include "widgets/polygon.h"
#include "widgets/ray.h"

#include <catch2/catch_test_macros.hpp>
#include <cmath>

namespace {

constexpr double kEpsilon = 1e-6;

bool Close(double lhs, double rhs) {
    return std::abs(lhs - rhs) < kEpsilon;
}

}  // namespace

TEST_CASE("Ray rotation keeps the origin and updates the angle") {
    const Ray ray(QPointF(0.0, 0.0), QPointF(10.0, 0.0), 0.0);
    const Ray rotated = ray.Rotate(1.5707963267948966);

    REQUIRE(Close(rotated.GetBegin().x(), 0.0));
    REQUIRE(Close(rotated.GetBegin().y(), 0.0));
    REQUIRE(Close(rotated.GetEnd().x(), 0.0));
    REQUIRE(Close(rotated.GetEnd().y(), 10.0));
    REQUIRE(Close(rotated.GetAngle(), 1.5707963267948966));
}

TEST_CASE("Polygon intersects a ray at the nearest edge") {
    const Polygon polygon({
      QPointF(4.0, -1.0),
      QPointF(6.0, -1.0),
      QPointF(6.0, 1.0),
      QPointF(4.0, 1.0),
    });
    const Ray ray(QPointF(0.0, 0.0), QPointF(10.0, 0.0), 0.0);

    const auto intersection = polygon.IntersectRay(ray);

    REQUIRE(intersection.has_value());
    REQUIRE(Close(intersection->x(), 4.0));
    REQUIRE(Close(intersection->y(), 0.0));
}

TEST_CASE("Controller creates a bounded light area") {
    Controller controller;
    controller.SetCanvasBounds(QRectF(0.0, 0.0, 100.0, 100.0));
    controller.SetLightSource(QPointF(20.0, 50.0));
    controller.AddPolygon(Polygon({
      QPointF(60.0, 30.0),
      QPointF(80.0, 30.0),
      QPointF(80.0, 70.0),
      QPointF(60.0, 70.0),
    }));

    const Polygon light_area = controller.CreateLightArea();

    REQUIRE(light_area.GetVertices().size() >= 3);

    bool found_blocking_edge = false;
    for (const QPointF& vertex : light_area.GetVertices()) {
        if (Close(vertex.x(), 60.0)) {
            found_blocking_edge = true;
            break;
        }
    }
    REQUIRE(found_blocking_edge);
}
