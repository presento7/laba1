#ifndef LABS_RAYCASTER_MAIN_WINDOW_H
#define LABS_RAYCASTER_MAIN_WINDOW_H

#include "widgets/canvas_widget.h"

#include <QtWidgets/QMainWindow>

class QComboBox;
class QLabel;

class MainWindow : public QMainWindow {
   public:
    MainWindow();

   private:
    void UpdateHelpText();

    CanvasWidget* canvas_;
    QComboBox* mode_selector_;
    QLabel* help_label_;
};

#endif
