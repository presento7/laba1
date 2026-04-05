#ifndef LABS_RAYCASTER_WIDGETS_POLYGON_H
#define LABS_RAYCASTER_WIDGETS_POLYGON_H

#include "ray.h"

#include <QtCore/QPointF>
#include <optional>
#include <vector>

class Polygon {
   public:
    explicit Polygon(const std::vector<QPointF>& vertices = {});

    const std::vector<QPointF>& GetVertices() const;

    void AddVertex(const QPointF& vertex);
    void UpdateLastVertex(const QPointF& new_vertex);
    void RemoveLastVertex();

    std::size_t Size() const;
    bool Empty() const;

    std::optional<QPointF> IntersectRay(const Ray& ray) const;

   private:
    std::vector<QPointF> vertices_;
};

#endif
