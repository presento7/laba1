#include "canvas_widget.h"

#include <QtGui/QLinearGradient>
#include <QtGui/QMouseEvent>
#include <QtGui/QPaintEvent>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QPen>
#include <QtGui/QRadialGradient>
#include <algorithm>

namespace {

QColor BackgroundTop() {
    return QColor(11, 17, 32);
}

QColor BackgroundBottom() {
    return QColor(29, 37, 58);
}

QColor PanelBorder() {
    return QColor(129, 146, 176, 80);
}

QColor LightColor() {
    return QColor(255, 234, 170, 60);
}

QColor GlowCoreColor() {
    return QColor(255, 248, 220, 210);
}

QColor PolygonFillColor() {
    return QColor(22, 27, 38, 245);
}

QColor PolygonDraftFillColor() {
    return QColor(68, 82, 120, 80);
}

QColor PolygonOutlineColor() {
    return QColor(185, 205, 255, 180);
}

}  // namespace

CanvasWidget::CanvasWidget(QWidget* parent)
    : QWidget(parent), mode_(Mode::kLight), drawing_polygon_(false) {
    setMouseTracking(true);
    setMinimumSize(820, 560);
    controller_.SetCanvasBounds(SceneBounds());
    controller_.SetLightSource(SceneBounds().center());
}

void CanvasWidget::SetMode(Mode mode) {
    mode_ = mode;
    update();
}

QSize CanvasWidget::sizeHint() const {
    return QSize(980, 680);
}

void CanvasWidget::paintEvent(QPaintEvent* event) {
    QWidget::paintEvent(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    DrawBackground(&painter);
    DrawLightAreas(&painter);
    DrawPolygons(&painter);
    DrawLightSources(&painter);
}

void CanvasWidget::mouseMoveEvent(QMouseEvent* event) {
    const QPointF scene_point = ClampToScene(event->position());
    if (mode_ == Mode::kLight) {
        controller_.SetLightSource(scene_point);
        update();
    } else if (drawing_polygon_) {
        controller_.UpdateLastPolygon(scene_point);
        update();
    }

    QWidget::mouseMoveEvent(event);
}

void CanvasWidget::mousePressEvent(QMouseEvent* event) {
    const QPointF scene_point = ClampToScene(event->position());

    if (mode_ == Mode::kLight && event->button() == Qt::LeftButton) {
        controller_.SetLightSource(scene_point);
        update();
    }

    if (mode_ == Mode::kPolygons && event->button() == Qt::LeftButton) {
        if (!drawing_polygon_) {
            controller_.AddPolygon(Polygon({scene_point, scene_point}));
            drawing_polygon_ = true;
        } else {
            controller_.AddVertexToLastPolygon(scene_point);
        }
        last_committed_vertex_ = scene_point;
        update();
    }

    if (mode_ == Mode::kPolygons && event->button() == Qt::RightButton && drawing_polygon_) {
        controller_.UpdateLastPolygon(last_committed_vertex_);
        controller_.FinishLastPolygon();
        drawing_polygon_ = false;
        update();
    }

    QWidget::mousePressEvent(event);
}

void CanvasWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    controller_.SetCanvasBounds(SceneBounds());
    if (drawing_polygon_) {
        controller_.UpdateLastPolygon(last_committed_vertex_);
    }
}

QRectF CanvasWidget::SceneBounds() const {
    return rect().adjusted(12, 12, -12, -12);
}

QPointF CanvasWidget::ClampToScene(const QPointF& point) const {
    const QRectF bounds = SceneBounds();
    return QPointF(
        std::clamp(point.x(), bounds.left(), bounds.right()),
        std::clamp(point.y(), bounds.top(), bounds.bottom()));
}

void CanvasWidget::DrawBackground(QPainter* painter) {
    const QRectF bounds = rect();
    QLinearGradient gradient(bounds.topLeft(), bounds.bottomRight());
    gradient.setColorAt(0.0, BackgroundTop());
    gradient.setColorAt(1.0, BackgroundBottom());
    painter->fillRect(bounds, gradient);

    painter->save();
    painter->setPen(QPen(PanelBorder(), 1.0));
    painter->setBrush(Qt::NoBrush);
    painter->drawRoundedRect(SceneBounds(), 14.0, 14.0);
    painter->restore();
}

void CanvasWidget::DrawLightAreas(QPainter* painter) {
    const std::vector<Polygon> light_areas = controller_.CreateLightAreas();

    painter->save();
    painter->setClipRect(SceneBounds());
    painter->setPen(Qt::NoPen);
    painter->setBrush(LightColor());

    for (const Polygon& area : light_areas) {
        const auto& vertices = area.GetVertices();
        if (vertices.size() >= 3) {
            painter->drawPolygon(vertices.data(), static_cast<int>(vertices.size()));
        }
    }

    painter->restore();
}

void CanvasWidget::DrawPolygons(QPainter* painter) {
    const auto& polygons = controller_.GetPolygons();

    painter->save();
    painter->setClipRect(SceneBounds());

    for (std::size_t index = 0; index < polygons.size(); ++index) {
        const bool is_draft = drawing_polygon_ && index + 1 == polygons.size();
        const auto& vertices = polygons[index].GetVertices();
        if (vertices.empty()) {
            continue;
        }

        if (!is_draft && vertices.size() >= 3) {
            painter->setPen(QPen(PolygonOutlineColor(), 1.5));
            painter->setBrush(PolygonFillColor());
            painter->drawPolygon(vertices.data(), static_cast<int>(vertices.size()));
        } else if (is_draft && vertices.size() >= 2) {
            QPen draft_pen(PolygonOutlineColor(), 1.5);
            draft_pen.setStyle(Qt::DashLine);
            painter->setPen(draft_pen);
            painter->setBrush(PolygonDraftFillColor());
            painter->drawPolyline(vertices.data(), static_cast<int>(vertices.size()));
        }

        painter->setPen(Qt::NoPen);
        painter->setBrush(GlowCoreColor());
        for (const QPointF& vertex : vertices) {
            painter->drawEllipse(vertex, 3.5, 3.5);
        }
    }

    painter->restore();
}

void CanvasWidget::DrawLightSources(QPainter* painter) {
    painter->save();
    painter->setClipRect(SceneBounds());

    for (const QPointF& source : controller_.GetLightSources()) {
        QRadialGradient glow(source, 24.0);
        glow.setColorAt(0.0, QColor(255, 248, 225, 170));
        glow.setColorAt(0.5, QColor(255, 226, 145, 70));
        glow.setColorAt(1.0, QColor(255, 226, 145, 0));
        painter->setPen(Qt::NoPen);
        painter->setBrush(glow);
        painter->drawEllipse(source, 24.0, 24.0);

        painter->setBrush(GlowCoreColor());
        painter->drawEllipse(source, 3.0, 3.0);
    }

    painter->restore();
}
