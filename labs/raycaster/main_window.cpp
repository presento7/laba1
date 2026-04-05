#include "main_window.h"

#include <QtWidgets/QComboBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

MainWindow::MainWindow() : canvas_(new CanvasWidget(this)) {
    QWidget* central = new QWidget(this);
    setCentralWidget(central);

    QLabel* mode_label = new QLabel("Mode", this);
    mode_label->setObjectName("modeLabel");

    mode_selector_ = new QComboBox(this);
    mode_selector_->addItem("light");
    mode_selector_->addItem("polygons");

    help_label_ = new QLabel(this);
    help_label_->setWordWrap(true);
    help_label_->setObjectName("helpLabel");

    QHBoxLayout* controls_layout = new QHBoxLayout();
    controls_layout->setContentsMargins(0, 0, 0, 0);
    controls_layout->addWidget(mode_label);
    controls_layout->addWidget(mode_selector_);
    controls_layout->addStretch();

    QVBoxLayout* layout = new QVBoxLayout();
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(12);
    layout->addLayout(controls_layout);
    layout->addWidget(help_label_);
    layout->addWidget(canvas_, 1);
    central->setLayout(layout);

    connect(
        mode_selector_, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, [this](int index) {
            if (index == 0) {
                canvas_->SetMode(CanvasWidget::Mode::kLight);
            } else {
                canvas_->SetMode(CanvasWidget::Mode::kPolygons);
            }
            UpdateHelpText();
        });

    UpdateHelpText();

    setWindowTitle("Lab 2: Raycaster");
    resize(1080, 780);
    setMinimumSize(920, 680);

    setStyleSheet(
        "QMainWindow { background: #101728; }"
        "QLabel { color: #edf1ff; }"
        "QLabel#helpLabel { color: #bcc7ea; }"
        "QLabel#modeLabel { font-weight: 600; }"
        "QComboBox {"
        "  color: #edf1ff;"
        "  background: #1d263b;"
        "  border: 1px solid rgba(188, 199, 234, 0.35);"
        "  border-radius: 8px;"
        "  padding: 6px 28px 6px 10px;"
        "}"
        "QComboBox QAbstractItemView {"
        "  color: #edf1ff;"
        "  background: #1d263b;"
        "  selection-background-color: #33435f;"
        "}");
}

void MainWindow::UpdateHelpText() {
    if (mode_selector_->currentIndex() == 0) {
        help_label_->setText(
            "Move the cursor inside the canvas to steer the light source cluster and inspect "
            "penumbras.");
    } else {
        help_label_->setText(
            "Create obstacles with left clicks. Right click closes the current polygon and starts "
            "a new one.");
    }
}
