#ifndef LABS_RAYCASTER_WIDGETS_CONTROLLER_H
#define LABS_RAYCASTER_WIDGETS_CONTROLLER_H

#include "polygon.h"
#include "ray.h"

#include <QtCore/QPointF>
#include <QtCore/QRectF>
#include <vector>

class Controller {
   public:
    Controller();

    const std::vector<Polygon>& GetPolygons() const;
    void AddPolygon(const Polygon& polygon);
    void AddVertexToLastPolygon(const QPointF& new_vertex);
    void UpdateLastPolygon(const QPointF& new_vertex);
    void FinishLastPolygon();

    QPointF GetLightSource() const;
    void SetLightSource(const QPointF& light_source);
    const std::vector<QPointF>& GetLightSources() const;

    void SetCanvasBounds(const QRectF& canvas_bounds);

    std::vector<Ray> CastRays() const;
    std::vector<Ray> CastRays(const QPointF& source) const;
    void IntersectRays(std::vector<Ray>* rays) const;
    void RemoveAdjacentRays(std::vector<Ray>* rays) const;
    Polygon CreateLightArea() const;
    Polygon CreateLightArea(const QPointF& source) const;
    std::vector<Polygon> CreateLightAreas() const;

   private:
    double MaxRayLength() const;
    QPointF ClampToCanvas(const QPointF& point) const;
    void RebuildBoundaryPolygon();
    void UpdateLightSources();

    std::vector<Polygon> polygons_;
    Polygon boundary_polygon_;
    QRectF canvas_bounds_;
    QPointF light_source_;
    std::vector<QPointF> light_source_offsets_;
    std::vector<QPointF> light_sources_;
};

#endif
