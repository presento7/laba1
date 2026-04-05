#ifndef LABS_RAYCASTER_WIDGETS_RAY_H
#define LABS_RAYCASTER_WIDGETS_RAY_H

#include <QtCore/QPointF>

class Ray {
   public:
    Ray(const QPointF& begin = QPointF(), const QPointF& end = QPointF(), double angle = 0.0);

    const QPointF& GetBegin() const;
    void SetBegin(const QPointF& begin);

    const QPointF& GetEnd() const;
    void SetEnd(const QPointF& end);

    double GetAngle() const;
    void SetAngle(double angle);

    Ray Rotate(double angle) const;

   private:
    QPointF begin_;
    QPointF end_;
    double angle_;
};

#endif
