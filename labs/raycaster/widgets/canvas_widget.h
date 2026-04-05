#ifndef LABS_RAYCASTER_WIDGETS_CANVAS_WIDGET_H
#define LABS_RAYCASTER_WIDGETS_CANVAS_WIDGET_H

#include "controller.h"

#include <QtWidgets/QWidget>

class CanvasWidget : public QWidget {
   public:
    enum class Mode {
        kLight,
        kPolygons,
    };

    explicit CanvasWidget(QWidget* parent = nullptr);

    void SetMode(Mode mode);
    QSize sizeHint() const override;

   protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

   private:
    QRectF SceneBounds() const;
    QPointF ClampToScene(const QPointF& point) const;

    void DrawBackground(QPainter* painter);
    void DrawLightAreas(QPainter* painter);
    void DrawPolygons(QPainter* painter);
    void DrawLightSources(QPainter* painter);
    void DrawHud(QPainter* painter);

    Controller controller_;
    Mode mode_;
    bool drawing_polygon_;
    QPointF last_committed_vertex_;
};

#endif
