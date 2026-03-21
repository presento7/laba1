#ifndef APP_MAIN_WINDOW_H
#define APP_MAIN_WINDOW_H

#include <QColor>
#include <QMainWindow>
#include <QString>
#include <QVector>

QT_BEGIN_NAMESPACE
class QComboBox;
class QCloseEvent;
class QDialog;
class QGroupBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QSpinBox;
QT_END_NAMESPACE

enum class TicketStatus {
    Fresh,
    Review,
    Done,
};

struct Ticket {
    QString name;
    QString hint;
    TicketStatus status = TicketStatus::Fresh;
};

class MainWindow : public QMainWindow {
    Q_OBJECT

   public:
    MainWindow();

   protected:
    void closeEvent(QCloseEvent* event) override;

   private slots:
    void handleCountChanged(int value);
    void handleSelectionChanged();
    void handleItemDoubleClicked(QListWidgetItem* item);
    void handleNameSubmitted();
    void handleStatusChanged(int index);
    void handleHintChanged();
    void chooseNextQuestion();
    void choosePreviousQuestion();
    void openOverview();
    void openHelp();

   private:
    QString defaultTicketName(int index) const;
    QColor colorForStatus(TicketStatus status) const;
    int comboIndexForStatus(TicketStatus status) const;
    TicketStatus statusForComboIndex(int index) const;
    int eligibleTicketCount() const;
    void buildUi();
    void buildOverviewDialog();
    void buildHelpDialog();
    void applyStyle();
    void resetTickets(int count);
    void loadState();
    void saveState() const;
    void populateView();
    void refreshTicketItem(int index);
    void refreshQuestionView();
    void refreshProgress();
    void refreshButtons();
    void selectTicket(int index, bool pushToHistory);
    void updateTicketStatus(int index, TicketStatus status);

    QVector<Ticket> tickets_;
    QVector<int> history_;
    int historyCursor_ = -1;
    int currentIndex_ = 0;
    bool syncing_ = false;

    QDialog* overviewDialog_ = nullptr;
    QDialog* helpDialog_ = nullptr;
    QSpinBox* countSpin_ = nullptr;
    QListWidget* view_ = nullptr;
    QLabel* numberValue_ = nullptr;
    QLabel* nameValue_ = nullptr;
    QLineEdit* nameEdit_ = nullptr;
    QComboBox* statusCombo_ = nullptr;
    QPlainTextEdit* hintEdit_ = nullptr;
    QPushButton* helpButton_ = nullptr;
    QPushButton* overviewButton_ = nullptr;
    QPushButton* nextButton_ = nullptr;
    QPushButton* previousButton_ = nullptr;
    QProgressBar* totalProgress_ = nullptr;
    QProgressBar* greenProgress_ = nullptr;
};

#endif
