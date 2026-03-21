#include "main_window.h"

#include <QBrush>
#include <QCloseEvent>
#include <QComboBox>
#include <QDialog>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRandomGenerator>
#include <QScrollArea>
#include <QSettings>
#include <QSpinBox>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWidget>

namespace {

constexpr int kDefaultTicketCount = 24;
constexpr int kMinimumTicketCount = 1;
constexpr int kMaximumTicketCount = 200;

}

MainWindow::MainWindow() {
    buildUi();
    buildOverviewDialog();
    buildHelpDialog();
    applyStyle();
    loadState();
    statusBar()->showMessage("Можно начинать последний круг повторения");
    setWindowTitle("Quiz Helper");
    resize(760, 700);
    setMinimumSize(640, 560);
}

void MainWindow::closeEvent(QCloseEvent* event) {
    saveState();
    QMainWindow::closeEvent(event);
}

void MainWindow::handleCountChanged(int value) {
    resetTickets(value);
}

void MainWindow::handleSelectionChanged() {
    if (syncing_) {
        return;
    }

    const int row = view_->currentRow();
    if (row < 0 || row >= tickets_.size() || row == currentIndex_) {
        return;
    }

    selectTicket(row, true);
}

void MainWindow::handleItemDoubleClicked(QListWidgetItem* item) {
    if (!item) {
        return;
    }

    const int row = view_->row(item);
    if (row < 0 || row >= tickets_.size()) {
        return;
    }

    const TicketStatus currentStatus = tickets_[row].status;
    const TicketStatus nextStatus =
        currentStatus == TicketStatus::Done ? TicketStatus::Review : TicketStatus::Done;

    updateTicketStatus(row, nextStatus);
    selectTicket(row, false);
}

void MainWindow::handleNameSubmitted() {
    if (!nameEdit_->hasFocus() || currentIndex_ < 0 || currentIndex_ >= tickets_.size()) {
        return;
    }

    const QString newName = nameEdit_->text().trimmed();
    if (newName.isEmpty()) {
        return;
    }

    tickets_[currentIndex_].name = newName;
    refreshTicketItem(currentIndex_);
    refreshQuestionView();
    saveState();
    statusBar()->showMessage("Название вопроса обновлено", 2000);
}

void MainWindow::handleStatusChanged(int index) {
    if (syncing_ || currentIndex_ < 0 || currentIndex_ >= tickets_.size()) {
        return;
    }

    updateTicketStatus(currentIndex_, statusForComboIndex(index));
}

void MainWindow::handleHintChanged() {
    if (syncing_ || currentIndex_ < 0 || currentIndex_ >= tickets_.size()) {
        return;
    }

    tickets_[currentIndex_].hint = hintEdit_->toPlainText();
    saveState();
}

void MainWindow::chooseNextQuestion() {
    QVector<int> candidates;
    candidates.reserve(tickets_.size());

    for (int i = 0; i < tickets_.size(); ++i) {
        if (tickets_[i].status == TicketStatus::Fresh ||
            tickets_[i].status == TicketStatus::Review) {
            if (tickets_.size() == 1 || i != currentIndex_) {
                candidates.append(i);
            }
        }
    }

    if (candidates.isEmpty()) {
        for (int i = 0; i < tickets_.size(); ++i) {
            if (tickets_[i].status == TicketStatus::Fresh ||
                tickets_[i].status == TicketStatus::Review) {
                candidates.append(i);
            }
        }
    }

    if (candidates.isEmpty()) {
        QMessageBox::information(this, "Все повторено", "Не осталось красных или желтых вопросов.");
        return;
    }

    const int chosen = candidates.at(QRandomGenerator::global()->bounded(candidates.size()));
    selectTicket(chosen, true);
}

void MainWindow::choosePreviousQuestion() {
    if (historyCursor_ <= 0 || history_.isEmpty()) {
        return;
    }

    --historyCursor_;
    selectTicket(history_.at(historyCursor_), false);
}

void MainWindow::openOverview() {
    if (!overviewDialog_) {
        return;
    }

    overviewDialog_->show();
    overviewDialog_->raise();
    overviewDialog_->activateWindow();
}

void MainWindow::openHelp() {
    if (!helpDialog_) {
        return;
    }

    helpDialog_->show();
    helpDialog_->raise();
    helpDialog_->activateWindow();
}

QString MainWindow::defaultTicketName(int index) const {
    return QString("Вопрос %1").arg(index + 1);
}

QColor MainWindow::colorForStatus(TicketStatus status) const {
    switch (status) {
        case TicketStatus::Fresh:
            return QColor(227, 121, 127, 235);
        case TicketStatus::Review:
            return QColor(232, 196, 88, 236);
        case TicketStatus::Done:
            return QColor(107, 184, 113, 238);
    }

    return QColor(227, 121, 127, 235);
}

int MainWindow::comboIndexForStatus(TicketStatus status) const {
    switch (status) {
        case TicketStatus::Fresh:
            return 0;
        case TicketStatus::Review:
            return 1;
        case TicketStatus::Done:
            return 2;
    }

    return 0;
}

TicketStatus MainWindow::statusForComboIndex(int index) const {
    if (index == 1) {
        return TicketStatus::Review;
    }

    if (index == 2) {
        return TicketStatus::Done;
    }

    return TicketStatus::Fresh;
}

int MainWindow::eligibleTicketCount() const {
    int count = 0;
    for (const Ticket& ticket : tickets_) {
        if (ticket.status == TicketStatus::Fresh || ticket.status == TicketStatus::Review) {
            ++count;
        }
    }
    return count;
}

void MainWindow::buildUi() {
    auto* central = new QWidget(this);
    auto* rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(26, 22, 26, 22);
    rootLayout->setSpacing(18);

    auto* title = new QLabel("Quiz Helper", this);
    title->setObjectName("titleLabel");

    auto* topBar = new QFrame(this);
    topBar->setObjectName("topBar");
    auto* topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(20, 16, 20, 16);
    topLayout->setSpacing(14);

    helpButton_ = new QPushButton("?", this);
    helpButton_->setObjectName("helpButton");
    overviewButton_ = new QPushButton("Обзор и статистика", this);
    nextButton_ = new QPushButton("Случайный следующий", this);
    nextButton_->setObjectName("accentButton");
    overviewButton_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    nextButton_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    topLayout->addWidget(helpButton_, 0, Qt::AlignLeft);
    topLayout->addSpacing(10);
    topLayout->addWidget(overviewButton_, 1);
    topLayout->addWidget(nextButton_, 1);

    auto* questionBox = new QGroupBox("", this);
    auto* questionLayout = new QVBoxLayout(questionBox);
    questionLayout->setContentsMargins(20, 22, 20, 20);
    questionLayout->setSpacing(16);

    auto* infoCard = new QFrame(this);
    infoCard->setObjectName("infoCard");
    auto* infoLayout = new QGridLayout(infoCard);
    infoLayout->setContentsMargins(18, 18, 18, 18);
    infoLayout->setHorizontalSpacing(14);
    infoLayout->setVerticalSpacing(10);

    auto* numberLabel = new QLabel("Номер", this);
    numberLabel->setObjectName("sectionLabel");
    numberValue_ = new QLabel(this);
    numberValue_->setObjectName("valueLabel");

    auto* nameLabel = new QLabel("Название", this);
    nameLabel->setObjectName("sectionLabel");
    nameValue_ = new QLabel(this);
    nameValue_->setObjectName("nameValue");
    nameValue_->setWordWrap(true);

    infoLayout->addWidget(numberLabel, 0, 0);
    infoLayout->addWidget(numberValue_, 0, 1);
    infoLayout->addWidget(nameLabel, 1, 0);
    infoLayout->addWidget(nameValue_, 1, 1);
    infoLayout->setColumnStretch(1, 1);

    auto* editBox = new QFrame(this);
    editBox->setObjectName("editorCard");
    auto* editLayout = new QGridLayout(editBox);
    editLayout->setContentsMargins(18, 18, 18, 18);
    editLayout->setHorizontalSpacing(14);
    editLayout->setVerticalSpacing(12);

    auto* renameLabel = new QLabel("Переименовать", this);
    renameLabel->setObjectName("sectionLabel");
    nameEdit_ = new QLineEdit(this);
    nameEdit_->setPlaceholderText("Нажми Enter, чтобы сохранить название");

    auto* statusLabel = new QLabel("Статус", this);
    statusLabel->setObjectName("sectionLabel");
    statusCombo_ = new QComboBox(this);
    statusCombo_->addItems({"Не отвечен", "Нужно повторить", "Отвечен"});

    auto* hintLabel = new QLabel("Подсказка", this);
    hintLabel->setObjectName("sectionLabel");
    hintEdit_ = new QPlainTextEdit(this);
    hintEdit_->setPlaceholderText("Подсказка");
    hintEdit_->setMinimumHeight(170);

    editLayout->addWidget(renameLabel, 0, 0);
    editLayout->addWidget(nameEdit_, 0, 1);
    editLayout->addWidget(statusLabel, 1, 0);
    editLayout->addWidget(statusCombo_, 1, 1);
    editLayout->addWidget(hintLabel, 2, 0, Qt::AlignTop);
    editLayout->addWidget(hintEdit_, 2, 1);
    editLayout->setColumnStretch(1, 1);

    questionLayout->addWidget(infoCard);
    questionLayout->addWidget(editBox, 1);

    auto* questionScroll = new QScrollArea(this);
    questionScroll->setWidgetResizable(true);
    questionScroll->setFrameShape(QFrame::NoFrame);
    questionScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    questionScroll->setWidget(questionBox);

    rootLayout->addWidget(title);
    rootLayout->addWidget(topBar);
    rootLayout->addWidget(questionScroll, 1);

    setCentralWidget(central);

    connect(nameEdit_, &QLineEdit::returnPressed, this, &MainWindow::handleNameSubmitted);
    connect(
        statusCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &MainWindow::handleStatusChanged);
    connect(hintEdit_, &QPlainTextEdit::textChanged, this, &MainWindow::handleHintChanged);
    connect(helpButton_, &QPushButton::clicked, this, &MainWindow::openHelp);
    connect(overviewButton_, &QPushButton::clicked, this, &MainWindow::openOverview);
    connect(nextButton_, &QPushButton::clicked, this, &MainWindow::chooseNextQuestion);
}

void MainWindow::buildOverviewDialog() {
    overviewDialog_ = new QDialog(this);
    overviewDialog_->setWindowTitle("Обзор и статистика");
    overviewDialog_->resize(980, 720);
    overviewDialog_->setMinimumSize(760, 560);

    auto* dialogLayout = new QVBoxLayout(overviewDialog_);
    dialogLayout->setContentsMargins(22, 20, 22, 20);
    dialogLayout->setSpacing(16);

    auto* dialogTitle = new QLabel("Quiz Helper", overviewDialog_);
    dialogTitle->setObjectName("titleLabel");

    auto* controlBar = new QFrame(overviewDialog_);
    controlBar->setObjectName("topBar");
    auto* controlLayout = new QHBoxLayout(controlBar);
    controlLayout->setContentsMargins(18, 14, 18, 14);
    controlLayout->setSpacing(14);

    auto* countLabel = new QLabel("Количество вопросов", overviewDialog_);
    countLabel->setObjectName("sectionLabel");

    countSpin_ = new QSpinBox(overviewDialog_);
    countSpin_->setRange(kMinimumTicketCount, kMaximumTicketCount);
    countSpin_->setButtonSymbols(QAbstractSpinBox::PlusMinus);

    previousButton_ = new QPushButton("Предыдущий вопрос", overviewDialog_);

    controlLayout->addWidget(countLabel);
    controlLayout->addWidget(countSpin_);
    controlLayout->addStretch(1);
    controlLayout->addWidget(previousButton_);

    auto* viewBox = new QGroupBox("Общий прогресс", overviewDialog_);
    auto* viewLayout = new QVBoxLayout(viewBox);
    viewLayout->setContentsMargins(18, 22, 18, 18);
    viewLayout->setSpacing(14);

    view_ = new QListWidget(overviewDialog_);
    view_->setViewMode(QListView::IconMode);
    view_->setResizeMode(QListView::Adjust);
    view_->setMovement(QListView::Static);
    view_->setFlow(QListView::LeftToRight);
    view_->setWrapping(true);
    view_->setSpacing(10);
    view_->setUniformItemSizes(true);
    view_->setWordWrap(true);
    view_->setGridSize(QSize(168, 108));
    view_->setSelectionMode(QAbstractItemView::SingleSelection);
    view_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    auto* progressBox = new QFrame(overviewDialog_);
    progressBox->setObjectName("progressBox");
    auto* progressLayout = new QGridLayout(progressBox);
    progressLayout->setContentsMargins(16, 16, 16, 16);
    progressLayout->setHorizontalSpacing(12);
    progressLayout->setVerticalSpacing(10);

    auto* totalLabel = new QLabel("В процессе", overviewDialog_);
    totalLabel->setObjectName("sectionLabel");
    totalProgress_ = new QProgressBar(overviewDialog_);
    totalProgress_->setTextVisible(true);

    auto* greenLabel = new QLabel("Полностью закрыто", overviewDialog_);
    greenLabel->setObjectName("sectionLabel");
    greenProgress_ = new QProgressBar(overviewDialog_);
    greenProgress_->setTextVisible(true);
    greenProgress_->setObjectName("greenProgress");

    progressLayout->addWidget(totalLabel, 0, 0);
    progressLayout->addWidget(totalProgress_, 0, 1);
    progressLayout->addWidget(greenLabel, 1, 0);
    progressLayout->addWidget(greenProgress_, 1, 1);
    progressLayout->setColumnStretch(1, 1);

    viewLayout->addWidget(view_, 1);
    viewLayout->addWidget(progressBox);

    dialogLayout->addWidget(dialogTitle);
    dialogLayout->addWidget(controlBar);
    dialogLayout->addWidget(viewBox, 1);

    connect(countSpin_, &QSpinBox::valueChanged, this, &MainWindow::handleCountChanged);
    connect(view_, &QListWidget::itemSelectionChanged, this, &MainWindow::handleSelectionChanged);
    connect(view_, &QListWidget::itemDoubleClicked, this, &MainWindow::handleItemDoubleClicked);
    connect(previousButton_, &QPushButton::clicked, this, &MainWindow::choosePreviousQuestion);
}

void MainWindow::buildHelpDialog() {
    helpDialog_ = new QDialog(this);
    helpDialog_->setWindowTitle("Как пользоваться");
    helpDialog_->resize(540, 430);
    helpDialog_->setMinimumSize(480, 360);

    auto* layout = new QVBoxLayout(helpDialog_);
    layout->setContentsMargins(22, 20, 22, 20);
    layout->setSpacing(16);

    auto* title = new QLabel("Quiz Helper", helpDialog_);
    title->setObjectName("titleLabel");

    auto* scroll = new QScrollArea(helpDialog_);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* card = new QFrame;
    card->setObjectName("editorCard");
    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(18, 18, 18, 18);
    cardLayout->setSpacing(14);

    auto* text = new QLabel(
        "Как работает приложение:\n"
        "1. В главном окне открыт текущий вопрос.\n"
        "2. Кнопка \"Случайный следующий\" выбирает новый вопрос только среди неотвеченных и тех, "
        "которые стоит повторить.\n"
        "3. Название вопроса можно менять через поле редактирования, статус выбирается в "
        "выпадающем списке.\n"
        "4. В подсказку удобно записывать формулы, ключевые идеи и мини-шпаргалки.\n\n"
        "Как использовать обзор:\n"
        "1. Кнопка \"Обзор и статистика\" открывает все вопросы и прогресс.\n"
        "2. Нежно-красный цвет означает, что вопрос еще не отвечен.\n"
        "3. Нежно-желтый цвет означает, что вопрос надо повторить.\n"
        "4. Зеленый цвет означает, что вопрос уже закрыт.\n"
        "5. Двойной клик по карточке в обзоре быстро переключает желтый и зеленый статусы.\n"
        "6. Кнопка \"Предыдущий вопрос\" возвращает к вопросу, который был выбран до текущего.",
        helpDialog_);
    text->setObjectName("helpText");
    text->setWordWrap(true);
    text->setTextFormat(Qt::PlainText);

    auto* closeButton = new QPushButton("Понятно", helpDialog_);
    closeButton->setObjectName("accentButton");

    cardLayout->addWidget(text);
    scroll->setWidget(card);
    layout->addWidget(title);
    layout->addWidget(scroll, 1);
    layout->addWidget(closeButton, 0, Qt::AlignRight);

    connect(closeButton, &QPushButton::clicked, helpDialog_, &QDialog::close);
}

void MainWindow::applyStyle() {
    const QString style = R"(
        QMainWindow {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                                        stop:0 #c8e9f2, stop:0.5 #cfeecf, stop:1 #e1f0c7);
        }
        QDialog {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                                        stop:0 #d1edf5, stop:0.5 #d9f2d7, stop:1 #e8f3d1);
        }
        QLabel#titleLabel {
            color: #133f52;
            font-size: 28px;
            font-weight: 700;
        }
        QLabel#subtitleLabel {
            color: #4b6d53;
            font-size: 14px;
        }
        QLabel#sectionLabel {
            color: #284f60;
            font-size: 13px;
            font-weight: 600;
        }
        QLabel#valueLabel {
            color: #0f4758;
            font-size: 22px;
            font-weight: 700;
        }
        QLabel#nameValue {
            color: #113f50;
            font-size: 18px;
            font-weight: 600;
        }
        QLabel#helpText {
            color: #234d5d;
            font-size: 14px;
        }
        QGroupBox {
            background: rgba(232, 246, 240, 0.93);
            border: 1px solid #94cbb9;
            border-radius: 22px;
            color: #244e5e;
            font-size: 15px;
            font-weight: 700;
            margin-top: 10px;
        }
        QGroupBox::title {
            left: 18px;
            padding: 0 8px;
        }
        QFrame#topBar, QFrame#progressBox, QFrame#infoCard, QFrame#editorCard {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                                        stop:0 rgba(219, 242, 248, 0.98),
                                        stop:1 rgba(221, 241, 208, 0.96));
            border: 1px solid #8fc8b7;
            border-radius: 18px;
        }
        QScrollArea {
            background: transparent;
            border: none;
        }
        QScrollBar:vertical {
            background: rgba(191, 224, 216, 0.88);
            width: 12px;
            border-radius: 6px;
            margin: 4px 2px 0px 6px;
        }
        QScrollBar::handle:vertical {
            background: #5baea2;
            min-height: 36px;
            border-radius: 6px;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
        QListWidget {
            background: transparent;
            border: none;
            outline: none;
            padding: 2px;
        }
        QListWidget::item {
            border: none;
            background: transparent;
            padding: 0px;
            margin: 0px;
        }
        QListWidget::item:selected {
            border: none;
            background: transparent;
        }
        QSpinBox, QLineEdit, QComboBox, QPlainTextEdit {
            background: rgba(255, 255, 255, 0.95);
            color: #173f4d;
            border: 1px solid #82c6b5;
            border-radius: 12px;
            padding: 10px 12px;
            font-size: 14px;
        }
        QLineEdit:focus, QComboBox:focus, QPlainTextEdit:focus, QSpinBox:focus {
            border: 2px solid #3cab91;
        }
        QPlainTextEdit {
            padding-top: 12px;
        }
        QPushButton {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                                        stop:0 #86d8f0, stop:1 #a7ea64);
            color: #103542;
            border: 1px solid #4ca997;
            border-radius: 14px;
            font-size: 15px;
            font-weight: 600;
            padding: 12px 20px;
        }
        QPushButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                                        stop:0 #6ccfea, stop:1 #95e44d);
        }
        QPushButton:disabled {
            background: rgba(200, 220, 216, 0.95);
            color: #778f92;
            border: 1px solid rgba(111, 145, 142, 0.55);
        }
        QPushButton#accentButton {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                                        stop:0 #33bde9, stop:1 #84de37);
            color: #f7fffd;
            border: 1px solid #2d9d8d;
        }
        QPushButton#accentButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                                        stop:0 #18afe3, stop:1 #75cf2f);
        }
        QPushButton#helpButton {
            min-width: 44px;
            max-width: 44px;
            min-height: 44px;
            max-height: 44px;
            border-radius: 22px;
            font-size: 20px;
            font-weight: 700;
        }
        QProgressBar {
            background: rgba(196, 230, 220, 0.96);
            border: 1px solid rgba(103, 150, 143, 0.36);
            border-radius: 10px;
            min-height: 20px;
            color: #234651;
            text-align: center;
            font-weight: 600;
        }
        QProgressBar::chunk {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                                        stop:0 #80d8f4, stop:1 #f2ee95);
            border-radius: 10px;
        }
        QProgressBar#greenProgress::chunk {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                                        stop:0 #6fd3ba, stop:1 #9ce36c);
        }
        QStatusBar {
            background: rgba(202, 232, 224, 0.98);
            color: #3f6661;
        }
    )";

    setStyleSheet(style);
    if (overviewDialog_) {
        overviewDialog_->setStyleSheet(style);
    }
    if (helpDialog_) {
        helpDialog_->setStyleSheet(style);
    }
}

void MainWindow::resetTickets(int count) {
    tickets_.clear();
    tickets_.reserve(count);

    for (int i = 0; i < count; ++i) {
        Ticket ticket;
        ticket.name = defaultTicketName(i);
        tickets_.append(ticket);
    }

    history_.clear();
    currentIndex_ = 0;
    if (!tickets_.isEmpty()) {
        history_.append(0);
        historyCursor_ = 0;
    } else {
        historyCursor_ = -1;
    }

    populateView();
    refreshProgress();
    refreshButtons();
    refreshQuestionView();
    saveState();
    statusBar()->showMessage("Список вопросов сброшен к исходному состоянию", 2500);
}

void MainWindow::loadState() {
    QSettings settings;
    const int savedCount = settings.value("count", kDefaultTicketCount).toInt();
    const int safeCount = qBound(kMinimumTicketCount, savedCount, kMaximumTicketCount);

    countSpin_->blockSignals(true);
    countSpin_->setValue(safeCount);
    countSpin_->blockSignals(false);

    tickets_.clear();
    tickets_.reserve(safeCount);
    for (int i = 0; i < safeCount; ++i) {
        Ticket ticket;
        ticket.name = defaultTicketName(i);
        tickets_.append(ticket);
    }

    const int savedSize = settings.beginReadArray("tickets");
    for (int i = 0; i < savedSize && i < tickets_.size(); ++i) {
        settings.setArrayIndex(i);
        const QString savedName = settings.value("name").toString().trimmed();
        if (!savedName.isEmpty()) {
            tickets_[i].name = savedName;
        }
        tickets_[i].hint = settings.value("hint").toString();
        tickets_[i].status = statusForComboIndex(settings.value("status", 0).toInt());
    }
    settings.endArray();

    const int savedCurrent =
        qBound(0, settings.value("currentIndex", 0).toInt(), qMax(0, tickets_.size() - 1));

    history_.clear();
    if (!tickets_.isEmpty()) {
        history_.append(savedCurrent);
        historyCursor_ = 0;
        currentIndex_ = savedCurrent;
    } else {
        currentIndex_ = 0;
        historyCursor_ = -1;
    }

    populateView();
    refreshProgress();
    refreshButtons();
    refreshQuestionView();
}

void MainWindow::saveState() const {
    QSettings settings;
    settings.setValue("count", tickets_.size());
    settings.setValue("currentIndex", currentIndex_);
    settings.beginWriteArray("tickets");
    for (int i = 0; i < tickets_.size(); ++i) {
        settings.setArrayIndex(i);
        settings.setValue("name", tickets_[i].name);
        settings.setValue("hint", tickets_[i].hint);
        settings.setValue("status", comboIndexForStatus(tickets_[i].status));
    }
    settings.endArray();
}

void MainWindow::populateView() {
    syncing_ = true;
    view_->clear();

    for (int i = 0; i < tickets_.size(); ++i) {
        auto* item = new QListWidgetItem;
        item->setTextAlignment(Qt::AlignCenter);
        item->setSizeHint(QSize(148, 82));
        view_->addItem(item);
        refreshTicketItem(i);
    }

    if (!tickets_.isEmpty()) {
        view_->setCurrentRow(qBound(0, currentIndex_, tickets_.size() - 1));
    }

    for (int i = 0; i < tickets_.size(); ++i) {
        refreshTicketItem(i);
    }

    syncing_ = false;
}

void MainWindow::refreshTicketItem(int index) {
    if (index < 0 || index >= tickets_.size() || index >= view_->count()) {
        return;
    }

    QListWidgetItem* item = view_->item(index);
    const TicketStatus status = tickets_[index].status;
    QString background;
    QString foreground;
    QString border;
    if (status == TicketStatus::Fresh) {
        background = "#e67d84";
        foreground = "#5c1320";
        border = "#c75b67";
    } else if (status == TicketStatus::Review) {
        background = "#ebca61";
        foreground = "#66480a";
        border = "#d4ad34";
    } else {
        background = "#70bc74";
        foreground = "#1c4024";
        border = "#4c9955";
    }
    const QString highlight = index == currentIndex_ ? "#133f52" : border;
    item->setText(QString());
    item->setToolTip(tickets_[index].name);
    item->setBackground(Qt::transparent);
    item->setSizeHint(QSize(164, 100));

    if (QWidget* oldCard = view_->itemWidget(item)) {
        oldCard->deleteLater();
    }

    auto* card = new QFrame(view_);
    card->setObjectName("ticketCard");
    card->setStyleSheet(QString(
                            "QFrame#ticketCard {"
                            "background:%1;"
                            "border:2px solid %2;"
                            "border-radius:16px;"
                            "}"
                            "QLabel {"
                            "background:transparent;"
                            "border:none;"
                            "color:%3;"
                            "}"
                            "QLabel#ticketNumber {"
                            "font-size:20px;"
                            "font-weight:700;"
                            "}"
                            "QLabel#ticketName {"
                            "font-size:12px;"
                            "font-weight:600;"
                            "}")
                            .arg(background, highlight, foreground));
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(2);

    auto* numberLabel = new QLabel(QString("%1").arg(index + 1, 2, 10, QLatin1Char('0')), card);
    numberLabel->setObjectName("ticketNumber");
    numberLabel->setAlignment(Qt::AlignCenter);

    auto* nameLabel = new QLabel(tickets_[index].name, card);
    nameLabel->setObjectName("ticketName");
    nameLabel->setAlignment(Qt::AlignCenter);
    nameLabel->setWordWrap(true);

    layout->addWidget(numberLabel);
    layout->addWidget(nameLabel, 1);
    view_->setItemWidget(item, card);
}

void MainWindow::refreshQuestionView() {
    const bool hasCurrent = currentIndex_ >= 0 && currentIndex_ < tickets_.size();

    syncing_ = true;
    if (hasCurrent) {
        numberValue_->setText(QString::number(currentIndex_ + 1));
        nameValue_->setText(tickets_[currentIndex_].name);
        nameEdit_->setText(tickets_[currentIndex_].name);
        statusCombo_->setCurrentIndex(comboIndexForStatus(tickets_[currentIndex_].status));
        hintEdit_->setPlainText(tickets_[currentIndex_].hint);
    } else {
        numberValue_->setText("—");
        nameValue_->setText("Нет вопроса");
        nameEdit_->clear();
        statusCombo_->setCurrentIndex(0);
        hintEdit_->clear();
    }
    syncing_ = false;

    nameEdit_->setEnabled(hasCurrent);
    statusCombo_->setEnabled(hasCurrent);
    hintEdit_->setEnabled(hasCurrent);
}

void MainWindow::refreshProgress() {
    const int total = tickets_.size();
    int touched = 0;
    int completed = 0;

    for (const Ticket& ticket : tickets_) {
        if (ticket.status != TicketStatus::Fresh) {
            ++touched;
        }
        if (ticket.status == TicketStatus::Done) {
            ++completed;
        }
    }

    totalProgress_->setRange(0, qMax(1, total));
    totalProgress_->setValue(touched);
    totalProgress_->setFormat(QString("%1 из %2").arg(touched).arg(total));

    greenProgress_->setRange(0, qMax(1, total));
    greenProgress_->setValue(completed);
    greenProgress_->setFormat(QString("%1 из %2").arg(completed).arg(total));
}

void MainWindow::refreshButtons() {
    previousButton_->setEnabled(historyCursor_ > 0);
    nextButton_->setEnabled(eligibleTicketCount() > 0);
}

void MainWindow::selectTicket(int index, bool pushToHistory) {
    if (index < 0 || index >= tickets_.size()) {
        return;
    }

    currentIndex_ = index;

    if (pushToHistory) {
        if (historyCursor_ + 1 < history_.size()) {
            history_.resize(historyCursor_ + 1);
        }
        if (history_.isEmpty() || history_.last() != index) {
            history_.append(index);
            historyCursor_ = history_.size() - 1;
        } else {
            historyCursor_ = history_.size() - 1;
        }
    }

    syncing_ = true;
    view_->setCurrentRow(index);
    if (view_->item(index)) {
        view_->scrollToItem(view_->item(index), QAbstractItemView::PositionAtCenter);
    }
    syncing_ = false;

    for (int i = 0; i < tickets_.size(); ++i) {
        refreshTicketItem(i);
    }

    refreshQuestionView();
    refreshButtons();
    saveState();
}

void MainWindow::updateTicketStatus(int index, TicketStatus status) {
    if (index < 0 || index >= tickets_.size()) {
        return;
    }

    tickets_[index].status = status;
    refreshTicketItem(index);
    refreshQuestionView();
    refreshProgress();
    refreshButtons();
    saveState();
}
