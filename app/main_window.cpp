#include "main_window.h"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QButtonGroup>
#include <QCloseEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QRandomGenerator>
#include <QSettings>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace {

constexpr int kWelcomePage = 0;
constexpr int kTranslationPage = 1;
constexpr int kGrammarPage = 2;
constexpr int kResultPage = 3;

QLabel* makeCaption(const QString& text, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setObjectName("captionLabel");
    label->setWordWrap(true);
    return label;
}

QLabel* makeStatValue(QWidget* parent) {
    auto* label = new QLabel(parent);
    label->setObjectName("statValue");
    label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return label;
}

void addStatRow(QGridLayout* layout, int row, const QString& title, QLabel* value, QWidget* parent) {
    auto* label = new QLabel(title, parent);
    label->setObjectName("statLabel");
    layout->addWidget(label, row, 0);
    layout->addWidget(value, row, 1);
}

}  // namespace

MainWindow::MainWindow() {
    seedTasks();
    buildUi();
    buildMenus();
    applyStyle();
    loadSettings();
    updateStatusPanel();

    setWindowTitle("LangUp");
    resize(1040, 680);
    setMinimumSize(820, 560);
}

void MainWindow::closeEvent(QCloseEvent* event) {
    saveSettings();
    QMainWindow::closeEvent(event);
}

void MainWindow::keyPressEvent(QKeyEvent* event) {
    const bool plainH = event->key() == Qt::Key_H && event->modifiers() == Qt::NoModifier;
    const bool typingAnswer = qobject_cast<QTextEdit*>(QApplication::focusWidget()) != nullptr;

    if (plainH && !typingAnswer) {
        showCurrentHelp();
        event->accept();
        return;
    }

    QMainWindow::keyPressEvent(event);
}

void MainWindow::startTranslationExercise() {
    startExercise(ExerciseType::Translation);
}

void MainWindow::startGrammarExercise() {
    startExercise(ExerciseType::Grammar);
}

void MainWindow::startSelectedExercise() {
    startExercise(selectedExercise_);
}

void MainWindow::submitTranslationAnswer() {
    if (!activeExercise_ || currentExercise_ != ExerciseType::Translation ||
        currentStep_ >= exerciseOrder_.size()) {
        return;
    }

    const TranslationTask& task = translationTasks_.at(exerciseOrder_.at(currentStep_));
    const QString answer = translationAnswer_->toPlainText();
    const bool correct = isTranslationCorrect(answer, task);
    const QString detail = correct
                               ? "Перевод принят. Продолжаем."
                               : QString("Ожидалось: %1").arg(task.answer);

    advanceAfterAnswer(correct, detail);
}

void MainWindow::submitGrammarAnswer() {
    if (!activeExercise_ || currentExercise_ != ExerciseType::Grammar ||
        currentStep_ >= exerciseOrder_.size()) {
        return;
    }

    const GrammarTask& task = grammarTasks_.at(exerciseOrder_.at(currentStep_));
    const int selectedId = grammarGroup_->checkedId();
    const bool correct = selectedId == task.correctIndex;
    const QString detail = correct
                               ? "Верно. Форма выбрана правильно."
                               : QString("Правильный вариант: %1").arg(task.options.at(task.correctIndex));

    advanceAfterAnswer(correct, detail);
}

void MainWindow::tickTimer() {
    if (!activeExercise_) {
        return;
    }

    --remainingSeconds_;
    updateStatusPanel();

    if (remainingSeconds_ <= 0) {
        finishExercise(
            false, "Время вышло",
            "Таймер дошел до нуля, поэтому упражнение остановлено без начисления баллов.");
    }
}

void MainWindow::openSettingsDialog() {
    QDialog dialog(this);
    dialog.setWindowTitle("Настройки");
    dialog.setModal(true);
    dialog.setMinimumWidth(520);

    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(22, 20, 22, 20);
    layout->setSpacing(14);

    auto* title = new QLabel("Настройки", &dialog);
    title->setObjectName("dialogTitle");

    auto* stats = new QFrame(&dialog);
    stats->setObjectName("settingsPanel");
    auto* statsLayout = new QGridLayout(stats);
    statsLayout->setContentsMargins(14, 14, 14, 14);
    statsLayout->setHorizontalSpacing(12);
    statsLayout->setVerticalSpacing(9);

    auto* settingsDifficultyValue = makeStatValue(stats);
    auto* settingsModeValue = makeStatValue(stats);
    auto* settingsTimerValue = makeStatValue(stats);
    auto* settingsAttemptsValue = makeStatValue(stats);

    const int minutes = qMax(0, remainingSeconds_) / 60;
    const int seconds = qMax(0, remainingSeconds_) % 60;
    const int shownMaxWrong =
        activeExercise_ || activeMaxWrongAttempts_ > 0 ? activeMaxWrongAttempts_ : maxWrongAttemptsForDifficulty();

    settingsDifficultyValue->setText(difficultyName());
    settingsModeValue->setText(activeExercise_ ? exerciseName() : exerciseName(selectedExercise_));
    settingsTimerValue->setText(
        activeExercise_ ? QString("%1:%2").arg(minutes).arg(seconds, 2, 10, QLatin1Char('0')) : "--:--");
    settingsAttemptsValue->setText(QString("%1 / %2").arg(wrongAttempts_).arg(shownMaxWrong));

    addStatRow(statsLayout, 0, "Сложность", settingsDifficultyValue, stats);
    addStatRow(statsLayout, 1, "Режим", settingsModeValue, stats);
    addStatRow(statsLayout, 2, "Таймер", settingsTimerValue, stats);
    addStatRow(statsLayout, 3, "Ошибки", settingsAttemptsValue, stats);

    auto* group = new QButtonGroup(&dialog);

    auto* easy = new QRadioButton("Easy: 4 задания, 3 ошибки, 3 минуты", &dialog);
    auto* normal = new QRadioButton("Normal: 6 заданий, 2 ошибки, 2:30", &dialog);
    auto* hard = new QRadioButton("Hard: 8 заданий, 1 ошибка, 2 минуты", &dialog);

    easy->setObjectName("choiceButton");
    normal->setObjectName("choiceButton");
    hard->setObjectName("choiceButton");

    group->addButton(easy, static_cast<int>(Difficulty::Easy));
    group->addButton(normal, static_cast<int>(Difficulty::Normal));
    group->addButton(hard, static_cast<int>(Difficulty::Hard));

    group->button(static_cast<int>(difficulty_))->setChecked(true);

    auto* trainingPanel = new QFrame(&dialog);
    trainingPanel->setObjectName("settingsPanel");
    auto* trainingLayout = new QHBoxLayout(trainingPanel);
    trainingLayout->setContentsMargins(14, 14, 14, 14);
    trainingLayout->setSpacing(10);

    auto* translationButton = new QPushButton("Translation", trainingPanel);
    translationButton->setObjectName("primaryButton");
    auto* grammarButton = new QPushButton("Grammar", trainingPanel);
    auto* helpButton = new QPushButton("Подсказка", trainingPanel);

    trainingLayout->addWidget(translationButton);
    trainingLayout->addWidget(grammarButton);
    trainingLayout->addWidget(helpButton);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    const auto applySettings = [&]() {
        difficulty_ = static_cast<Difficulty>(group->checkedId());
        updateStatusPanel();
        saveSettings();
    };

    connect(translationButton, &QPushButton::clicked, &dialog, [&]() {
        selectedExercise_ = ExerciseType::Translation;
        applySettings();
        dialog.accept();
        startTranslationExercise();
    });
    connect(grammarButton, &QPushButton::clicked, &dialog, [&]() {
        selectedExercise_ = ExerciseType::Grammar;
        applySettings();
        dialog.accept();
        startGrammarExercise();
    });
    connect(helpButton, &QPushButton::clicked, this, &MainWindow::showCurrentHelp);

    layout->addWidget(title);
    layout->addWidget(stats);
    layout->addWidget(easy);
    layout->addWidget(normal);
    layout->addWidget(hard);
    layout->addWidget(trainingPanel);
    layout->addSpacing(8);
    layout->addWidget(buttons);

    dialog.setStyleSheet(styleSheet());

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    applySettings();
    statusBar()->showMessage("Настройки сохранены", 2500);
}

void MainWindow::resetCurrentExercise() {
    stopExerciseTimer();
    activeExercise_ = false;
    currentExercise_ = ExerciseType::None;
    exerciseOrder_.clear();
    currentStep_ = 0;
    correctAnswers_ = 0;
    wrongAttempts_ = 0;
    remainingSeconds_ = 0;
    activeTaskCount_ = 0;
    activeTimeLimit_ = 0;
    activeMaxWrongAttempts_ = 0;
    activePoints_ = 0;
    perfectExercise_ = true;

    if (translationAnswer_) {
        translationAnswer_->clear();
    }
    if (grammarGroup_) {
        grammarGroup_->setExclusive(false);
        for (QAbstractButton* button : grammarGroup_->buttons()) {
            button->setChecked(false);
        }
        grammarGroup_->setExclusive(true);
    }
    if (pagesStack_) {
        pagesStack_->setCurrentIndex(kWelcomePage);
    }

    feedbackLabel_->setText("Ожидание...");
    updateStatusPanel();
    statusBar()->showMessage("Тест сброшен", 2000);
}

void MainWindow::showCurrentHelp() {
    QMessageBox::information(this, "Подсказка", currentHint());
}

void MainWindow::buildUi() {
    auto* central = new QWidget(this);
    auto* rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(22, 18, 22, 18);
    rootLayout->setSpacing(14);

    auto* topBar = new QFrame(central);
    topBar->setObjectName("topBar");
    buildTopBar(topBar);

    buildPages();

    rootLayout->addWidget(topBar);
    rootLayout->addWidget(exerciseStack_, 1);
    setCentralWidget(central);

    exerciseTimer_ = new QTimer(this);
    exerciseTimer_->setInterval(1000);
    connect(exerciseTimer_, &QTimer::timeout, this, &MainWindow::tickTimer);
}

void MainWindow::buildMenus() {
    auto* settingsMenu = menuBar()->addMenu("Настройки");
    auto* settingsAction = settingsMenu->addAction("Открыть настройки");
    connect(settingsAction, &QAction::triggered, this, &MainWindow::openSettingsDialog);

    auto* helpMenu = menuBar()->addMenu("Help");
    auto* hintAction = helpMenu->addAction("Подсказка к текущему упражнению (H)");
    connect(hintAction, &QAction::triggered, this, &MainWindow::showCurrentHelp);
}

void MainWindow::buildTopBar(QFrame* topBar) {
    auto* layout = new QHBoxLayout(topBar);
    layout->setContentsMargins(12, 10, 10, 10);
    layout->setSpacing(8);

    auto* title = new QLabel("LangUp", topBar);
    title->setObjectName("appTitle");

    modeValue_ = new QLabel(topBar);
    modeValue_->setObjectName("statLabel");
    modeValue_->setMaximumWidth(135);

    auto* scoreLabel = new QLabel("Баллы", topBar);
    scoreLabel->setObjectName("statLabel");
    scoreValue_ = makeStatValue(topBar);
    scoreValue_->setMinimumWidth(34);

    auto* streakLabel = new QLabel("Серия", topBar);
    streakLabel->setObjectName("statLabel");
    streakValue_ = makeStatValue(topBar);
    streakValue_->setMinimumWidth(30);

    progressText_ = new QLabel("0 из 0", topBar);
    progressText_->setObjectName("statLabel");

    progressBar_ = new QProgressBar(topBar);
    progressBar_->setTextVisible(false);
    progressBar_->setMinimumWidth(120);

    timerValue_ = makeStatValue(topBar);
    timerValue_->setMinimumWidth(54);

    timerBar_ = new QProgressBar(topBar);
    timerBar_->setObjectName("timerBar");
    timerBar_->setTextVisible(false);
    timerBar_->setMaximumWidth(82);

    resetButton_ = new QPushButton("Сброс", topBar);
    resetButton_->setObjectName("resetButton");
    settingsButton_ = new QPushButton(QString::fromUtf8("⚙"), topBar);
    settingsButton_->setObjectName("gearButton");
    settingsButton_->setToolTip("Настройки");

    connect(resetButton_, &QPushButton::clicked, this, &MainWindow::resetCurrentExercise);
    connect(settingsButton_, &QPushButton::clicked, this, &MainWindow::openSettingsDialog);

    layout->addWidget(title);
    layout->addStretch(1);
    layout->addWidget(modeValue_);
    layout->addWidget(scoreLabel);
    layout->addWidget(scoreValue_);
    layout->addWidget(streakLabel);
    layout->addWidget(streakValue_);
    layout->addWidget(progressText_);
    layout->addWidget(progressBar_, 2);
    layout->addWidget(timerValue_);
    layout->addWidget(timerBar_);
    layout->addWidget(resetButton_);
    layout->addWidget(settingsButton_);
}

void MainWindow::buildPages() {
    pagesStack_ = new QStackedWidget(this);
    pagesStack_->setObjectName("exerciseStack");

    auto* welcomePage = new QWidget(pagesStack_);
    auto* welcomeLayout = new QVBoxLayout(welcomePage);
    welcomeLayout->setContentsMargins(28, 26, 28, 26);
    welcomeLayout->setSpacing(18);

    auto* welcomeCard = new QFrame(welcomePage);
    welcomeCard->setObjectName("exercisePanel");
    auto* welcomeCardLayout = new QVBoxLayout(welcomeCard);
    welcomeCardLayout->setContentsMargins(28, 28, 28, 28);
    welcomeCardLayout->setSpacing(16);

    auto* welcomeTitle = new QLabel("Английский без спешки", welcomeCard);
    welcomeTitle->setObjectName("pageTitle");
    auto* welcomeText = makeCaption("Учимся потихонечку, без лишней спешки", welcomeCard);

    homeModeValue_ = new QLabel(welcomeCard);
    homeModeValue_->setObjectName("modeBadge");
    homeModeValue_->setAlignment(Qt::AlignCenter);

    startGameButton_ = new QPushButton("Начать игру", welcomeCard);
    startGameButton_->setObjectName("primaryButton");
    connect(startGameButton_, &QPushButton::clicked, this, &MainWindow::startSelectedExercise);

    welcomeCardLayout->addWidget(welcomeTitle);
    welcomeCardLayout->addWidget(welcomeText);
    welcomeCardLayout->addSpacing(8);
    welcomeCardLayout->addWidget(homeModeValue_, 0, Qt::AlignLeft);
    welcomeCardLayout->addWidget(startGameButton_, 0, Qt::AlignLeft);
    welcomeCardLayout->addStretch(1);
    welcomeLayout->addWidget(welcomeCard, 1);

    auto* translationPage = new QWidget(pagesStack_);
    auto* translationLayout = new QVBoxLayout(translationPage);
    translationLayout->setContentsMargins(28, 26, 28, 26);
    translationLayout->setSpacing(16);

    auto* translationPanel = new QFrame(translationPage);
    translationPanel->setObjectName("exercisePanel");
    auto* translationPanelLayout = new QVBoxLayout(translationPanel);
    translationPanelLayout->setContentsMargins(24, 24, 24, 24);
    translationPanelLayout->setSpacing(14);

    auto* translationTitle = new QLabel("Translation", translationPanel);
    translationTitle->setObjectName("pageTitle");
    translationHint_ = makeCaption("Переведите фразу на русский язык.", translationPanel);
    translationPrompt_ = new QLabel(translationPanel);
    translationPrompt_->setObjectName("promptLabel");
    translationPrompt_->setWordWrap(true);

    translationAnswer_ = new QTextEdit(translationPanel);
    translationAnswer_->setPlaceholderText("Введите перевод...");
    translationAnswer_->setMinimumHeight(130);

    submitTranslationButton_ = new QPushButton("Submit", translationPanel);
    submitTranslationButton_->setObjectName("primaryButton");
    connect(
        submitTranslationButton_, &QPushButton::clicked, this,
        &MainWindow::submitTranslationAnswer);

    translationPanelLayout->addWidget(translationTitle);
    translationPanelLayout->addWidget(translationHint_);
    translationPanelLayout->addWidget(translationPrompt_);
    translationPanelLayout->addWidget(translationAnswer_);
    translationPanelLayout->addWidget(submitTranslationButton_, 0, Qt::AlignRight);
    translationPanelLayout->addStretch(1);
    translationLayout->addWidget(translationPanel, 1);

    auto* grammarPage = new QWidget(pagesStack_);
    auto* grammarLayout = new QVBoxLayout(grammarPage);
    grammarLayout->setContentsMargins(28, 26, 28, 26);
    grammarLayout->setSpacing(16);

    auto* grammarPanel = new QFrame(grammarPage);
    grammarPanel->setObjectName("exercisePanel");
    auto* grammarPanelLayout = new QVBoxLayout(grammarPanel);
    grammarPanelLayout->setContentsMargins(24, 24, 24, 24);
    grammarPanelLayout->setSpacing(14);

    auto* grammarTitle = new QLabel("Grammar", grammarPanel);
    grammarTitle->setObjectName("pageTitle");
    grammarHint_ = makeCaption("Выберите один правильный вариант.", grammarPanel);
    grammarPrompt_ = new QLabel(grammarPanel);
    grammarPrompt_->setObjectName("promptLabel");
    grammarPrompt_->setWordWrap(true);

    auto* optionsPanel = new QFrame(grammarPanel);
    optionsPanel->setObjectName("optionsPanel");
    auto* optionsLayout = new QVBoxLayout(optionsPanel);
    optionsLayout->setContentsMargins(14, 14, 14, 14);
    optionsLayout->setSpacing(10);

    grammarGroup_ = new QButtonGroup(this);
    grammarGroup_->setExclusive(true);
    for (int i = 0; i < 4; ++i) {
        auto* option = new QRadioButton(optionsPanel);
        option->setObjectName("choiceButton");
        grammarGroup_->addButton(option, i);
        grammarOptions_.append(option);
        optionsLayout->addWidget(option);
    }

    submitGrammarButton_ = new QPushButton("Submit", grammarPanel);
    submitGrammarButton_->setObjectName("primaryButton");
    connect(submitGrammarButton_, &QPushButton::clicked, this, &MainWindow::submitGrammarAnswer);

    grammarPanelLayout->addWidget(grammarTitle);
    grammarPanelLayout->addWidget(grammarHint_);
    grammarPanelLayout->addWidget(grammarPrompt_);
    grammarPanelLayout->addWidget(optionsPanel);
    grammarPanelLayout->addWidget(submitGrammarButton_, 0, Qt::AlignRight);
    grammarPanelLayout->addStretch(1);
    grammarLayout->addWidget(grammarPanel, 1);

    auto* resultPage = new QWidget(pagesStack_);
    auto* resultLayout = new QVBoxLayout(resultPage);
    resultLayout->setContentsMargins(28, 26, 28, 26);
    resultLayout->setSpacing(16);

    auto* resultPanel = new QFrame(resultPage);
    resultPanel->setObjectName("exercisePanel");
    auto* resultPanelLayout = new QVBoxLayout(resultPanel);
    resultPanelLayout->setContentsMargins(28, 28, 28, 28);
    resultPanelLayout->setSpacing(16);

    resultTitle_ = new QLabel(resultPanel);
    resultTitle_->setObjectName("pageTitle");
    resultText_ = makeCaption("", resultPanel);

    auto* playAgainButton = new QPushButton("Сыграть ещё раз", resultPanel);
    playAgainButton->setObjectName("primaryButton");
    connect(playAgainButton, &QPushButton::clicked, this, &MainWindow::startSelectedExercise);

    resultPanelLayout->addWidget(resultTitle_);
    resultPanelLayout->addWidget(resultText_);
    resultPanelLayout->addWidget(playAgainButton, 0, Qt::AlignLeft);
    resultPanelLayout->addStretch(1);
    resultLayout->addWidget(resultPanel, 1);

    feedbackLabel_ = new QLabel(this);
    feedbackLabel_->setObjectName("feedbackLabel");
    feedbackLabel_->setWordWrap(true);
    feedbackLabel_->setText("Ожидание...");

    auto* shell = new QWidget(this);
    auto* shellLayout = new QVBoxLayout(shell);
    shellLayout->setContentsMargins(0, 0, 0, 0);
    shellLayout->setSpacing(10);
    shellLayout->addWidget(pagesStack_, 1);
    shellLayout->addWidget(feedbackLabel_);

    exerciseStack_ = new QStackedWidget(this);
    exerciseStack_->setObjectName("outerStack");
    exerciseStack_->addWidget(shell);
    pagesStack_->addWidget(welcomePage);
    pagesStack_->addWidget(translationPage);
    pagesStack_->addWidget(grammarPage);
    pagesStack_->addWidget(resultPage);
    pagesStack_->setCurrentIndex(kWelcomePage);
}

void MainWindow::applyStyle() {
    const QString style = R"(
        QMainWindow {
            background: #f6f7f8;
        }
        QMenuBar {
            background: #f6f7f8;
            color: #263238;
            padding: 4px 8px;
        }
        QMenuBar::item {
            background: transparent;
            padding: 6px 10px;
            border-radius: 6px;
        }
        QMenuBar::item:selected {
            background: #e8ecef;
        }
        QMenu {
            background: #ffffff;
            color: #263238;
            border: 1px solid #d9dee2;
            padding: 6px;
        }
        QMenu::item {
            padding: 7px 24px;
            border-radius: 6px;
        }
        QMenu::item:selected {
            background: #e7f1ef;
        }
        QFrame#topBar, QFrame#sidebar, QFrame#exercisePanel, QFrame#panel, QFrame#settingsPanel,
        QFrame#optionsPanel {
            background: #ffffff;
            border: 1px solid #d9dee2;
            border-radius: 8px;
        }
        QFrame#sidebar {
            background: #fbfbfc;
        }
        QLabel#appTitle {
            color: #1f2a30;
            font-size: 24px;
            font-weight: 700;
        }
        QLabel#pageTitle, QLabel#dialogTitle {
            color: #1f2a30;
            font-size: 24px;
            font-weight: 700;
        }
        QLabel#captionLabel {
            color: #5c6870;
            font-size: 14px;
            line-height: 1.35;
        }
        QLabel#promptLabel {
            color: #25323a;
            font-size: 22px;
            font-weight: 650;
            padding: 16px 0;
        }
        QLabel#statLabel {
            color: #65717a;
            font-size: 12px;
            font-weight: 600;
        }
        QLabel#statValue {
            color: #263238;
            font-size: 13px;
            font-weight: 700;
        }
        QLabel#modeBadge {
            background: #eef3f1;
            border: 1px solid #d0dcda;
            border-radius: 8px;
            color: #31433f;
            padding: 9px 14px;
            font-size: 14px;
            font-weight: 700;
        }
        QLabel#feedbackLabel {
            background: #ffffff;
            border: 1px solid #d9dee2;
            border-radius: 8px;
            color: #40515b;
            padding: 12px 14px;
            font-size: 14px;
        }
        QPushButton {
            background: #edf0f2;
            color: #23313a;
            border: 1px solid #cfd7dc;
            border-radius: 8px;
            padding: 9px 12px;
            font-size: 14px;
            font-weight: 650;
        }
        QPushButton:hover {
            background: #e4e9eb;
            border-color: #bdc8ce;
        }
        QPushButton:pressed {
            background: #d8e0e3;
        }
        QPushButton#primaryButton {
            background: #587b73;
            color: #ffffff;
            border-color: #45655f;
        }
        QPushButton#primaryButton:hover {
            background: #4d7169;
        }
        QPushButton#resetButton {
            background: #f2eeee;
            border-color: #d8caca;
            color: #3a2929;
            padding-left: 12px;
            padding-right: 12px;
        }
        QPushButton#resetButton:hover {
            background: #eadfdf;
        }
        QPushButton#gearButton {
            min-width: 36px;
            max-width: 36px;
            min-height: 36px;
            max-height: 36px;
            border-radius: 18px;
            padding: 0px;
            font-size: 20px;
        }
        QTextEdit {
            background: #fbfcfc;
            color: #23313a;
            border: 1px solid #cfd7dc;
            border-radius: 8px;
            padding: 10px;
            font-size: 15px;
        }
        QTextEdit:focus {
            border: 2px solid #73948d;
        }
        QRadioButton#choiceButton {
            color: #28363e;
            font-size: 15px;
            padding: 8px 6px;
        }
        QRadioButton#choiceButton::indicator {
            width: 20px;
            height: 20px;
            border: 2px solid #7c8a91;
            border-radius: 10px;
            background: #ffffff;
        }
        QRadioButton#choiceButton::indicator:hover {
            border-color: #587b73;
        }
        QRadioButton#choiceButton::indicator:checked {
            border: 2px solid #587b73;
            background: qradialgradient(cx:0.5, cy:0.5, fx:0.5, fy:0.5, radius:0.5,
                                        stop:0 #587b73, stop:0.48 #587b73,
                                        stop:0.52 #ffffff, stop:1 #ffffff);
        }
        QProgressBar {
            background: #e9edf0;
            border: 1px solid #d3dade;
            border-radius: 7px;
            min-height: 14px;
        }
        QProgressBar::chunk {
            background: #6f968d;
            border-radius: 6px;
        }
        QProgressBar#timerBar::chunk {
            background: #8b9aa3;
        }
        QDialog {
            background: #f6f7f8;
        }
        QMessageBox {
            background: #f6f7f8;
        }
        QMessageBox QLabel {
            color: #111111;
            background: transparent;
        }
        QDialogButtonBox QPushButton {
            min-width: 86px;
        }
        QStatusBar {
            background: #f6f7f8;
            color: #5c6870;
        }
    )";

    setStyleSheet(style);
}

void MainWindow::seedTasks() {
    translationTasks_ = {
        {"I drink green tea every morning.",
         "Я пью зеленый чай каждое утро.",
         {"Каждое утро я пью зеленый чай.", "Я каждое утро пью зеленый чай."},
         "Present Simple описывает привычку: I drink, he drinks. В русском порядке слов можно быть гибче."},
        {"She is reading a book near the window.",
         "Она читает книгу у окна.",
         {"Она читает книгу возле окна.", "У окна она читает книгу."},
         "Present Continuous: am/is/are + verb-ing. Здесь действие происходит прямо сейчас."},
        {"We visited the museum last Sunday.",
         "Мы посетили музей в прошлое воскресенье.",
         {"В прошлое воскресенье мы посетили музей.", "Мы были в музее в прошлое воскресенье."},
         "Last Sunday указывает на Past Simple. Для правильных глаголов часто добавляется -ed."},
        {"Could you open the door, please?",
         "Не могли бы вы открыть дверь, пожалуйста?",
         {"Могли бы вы открыть дверь, пожалуйста?", "Откройте дверь, пожалуйста."},
         "Could you ... ? звучит вежливее, чем прямой приказ."},
        {"The weather is getting colder.",
         "Погода становится холоднее.",
         {"Становится холоднее.", "Погода делается холоднее."},
         "Getting + adjective часто показывает постепенное изменение состояния."},
        {"My friend has already finished the project.",
         "Мой друг уже закончил проект.",
         {"Моя подруга уже закончила проект.", "Мой друг уже завершил проект."},
         "Already часто используется с Present Perfect: has finished."},
        {"They are planning a trip to London.",
         "Они планируют поездку в Лондон.",
         {"Они планируют путешествие в Лондон.", "Они собираются в поездку в Лондон."},
         "Planning a trip переводится как планировать поездку или путешествие."},
        {"This song sounds familiar.",
         "Эта песня звучит знакомо.",
         {"Эта песня кажется знакомой.", "Эта песня мне знакома."},
         "Sound может означать не только звук, но и впечатление: sounds familiar."},
        {"I have never tried sushi before.",
         "Я никогда раньше не пробовал суши.",
         {"Я никогда раньше не пробовала суши.", "Раньше я никогда не пробовал суши."},
         "Never before усиливает опыт в Present Perfect: have tried."},
        {"Let's meet after the lesson.",
         "Давай встретимся после занятия.",
         {"Давайте встретимся после занятия.", "Встретимся после урока."},
         "Let's + verb предлагает совместное действие."},
    };

    grammarTasks_ = {
        {"She ___ to music every evening.", {"listen", "listens", "is listen", "listening"}, 1,
         "В Present Simple с he/she/it к глаголу обычно добавляется -s."},
        {"They ___ football right now.", {"play", "plays", "are playing", "played"}, 2,
         "Right now требует Present Continuous: are + verb-ing."},
        {"I ___ my keys yesterday.", {"lose", "lost", "have lost", "am losing"}, 1,
         "Yesterday указывает на Past Simple: lose -> lost."},
        {"There ___ two notebooks on the table.", {"is", "are", "was", "be"}, 1,
         "Для множественного числа используется there are."},
        {"We have lived here ___ 2021.", {"for", "since", "during", "after"}, 1,
         "Since используется с точкой во времени: since 2021."},
        {"This is the ___ movie I have ever seen.", {"interesting", "more interesting", "most interesting", "interestinger"}, 2,
         "The most + adjective образует превосходную степень для длинных прилагательных."},
        {"If it rains, we ___ at home.", {"stay", "will stay", "stayed", "staying"}, 1,
         "Первый conditional: If + Present Simple, will + verb."},
        {"He is good ___ solving problems.", {"in", "at", "on", "for"}, 1,
         "Good at + noun/gerund: good at solving."},
        {"I am looking forward ___ you.", {"to see", "seeing", "to seeing", "see"}, 2,
         "Look forward to требует to + gerund: to seeing."},
        {"The report ___ by Anna yesterday.", {"wrote", "was written", "has written", "is writing"}, 1,
         "Passive voice в Past Simple: was/were + past participle."},
    };
}

void MainWindow::loadSettings() {
    QSettings settings;
    difficulty_ = static_cast<Difficulty>(settings.value("difficulty", static_cast<int>(Difficulty::Normal)).toInt());
    selectedExercise_ =
        static_cast<ExerciseType>(settings.value("selectedExercise", static_cast<int>(ExerciseType::Translation)).toInt());
    if (selectedExercise_ == ExerciseType::None) {
        selectedExercise_ = ExerciseType::Translation;
    }
    totalScore_ = settings.value("totalScore", 0).toInt();
    streak_ = settings.value("streak", 0).toInt();
}

void MainWindow::saveSettings() const {
    QSettings settings;
    settings.setValue("difficulty", static_cast<int>(difficulty_));
    settings.setValue("selectedExercise", static_cast<int>(selectedExercise_));
    settings.setValue("totalScore", totalScore_);
    settings.setValue("streak", streak_);
}

void MainWindow::startExercise(ExerciseType type) {
    if (type == ExerciseType::None) {
        type = ExerciseType::Translation;
    }

    if (activeExercise_) {
        const auto answer = QMessageBox::question(
            this, "Новая тренировка",
            "Текущее упражнение еще идет. Начать новое и сбросить текущий прогресс?");
        if (answer != QMessageBox::Yes) {
            return;
        }
    }

    selectedExercise_ = type;
    currentExercise_ = type;
    currentStep_ = 0;
    correctAnswers_ = 0;
    wrongAttempts_ = 0;
    activeTaskCount_ = taskCountForDifficulty();
    activeTimeLimit_ = timeLimitForDifficulty();
    activeMaxWrongAttempts_ = maxWrongAttemptsForDifficulty();
    activePoints_ = pointsForDifficulty();
    remainingSeconds_ = activeTimeLimit_;
    activeExercise_ = true;
    perfectExercise_ = true;

    generateExerciseOrder(type == ExerciseType::Translation ? translationTasks_.size() : grammarTasks_.size());

    feedbackLabel_->setText(QString("%1: новый набор из %2 заданий.").arg(exerciseName()).arg(exerciseOrder_.size()));
    showCurrentTask();
    updateStatusPanel();

    exerciseTimer_->start();
}

void MainWindow::generateExerciseOrder(int sourceSize) {
    exerciseOrder_.clear();
    exerciseOrder_.reserve(sourceSize);

    for (int i = 0; i < sourceSize; ++i) {
        exerciseOrder_.append(i);
    }

    for (int i = exerciseOrder_.size() - 1; i > 0; --i) {
        const int j = QRandomGenerator::global()->bounded(i + 1);
        exerciseOrder_.swapItemsAt(i, j);
    }

    const int needed = qMin(activeTaskCount_, exerciseOrder_.size());
    exerciseOrder_.resize(needed);
}

void MainWindow::showCurrentTask() {
    if (!activeExercise_ || currentStep_ < 0 || currentStep_ >= exerciseOrder_.size()) {
        return;
    }

    if (currentExercise_ == ExerciseType::Translation) {
        showTranslationTask();
    } else if (currentExercise_ == ExerciseType::Grammar) {
        showGrammarTask();
    }

    updateExerciseStatus();
}

void MainWindow::showTranslationTask() {
    const TranslationTask& task = translationTasks_.at(exerciseOrder_.at(currentStep_));
    translationPrompt_->setText(task.prompt);
    translationAnswer_->clear();
    translationAnswer_->setFocus();
    pagesStack_->setCurrentIndex(kTranslationPage);
}

void MainWindow::showGrammarTask() {
    const GrammarTask& task = grammarTasks_.at(exerciseOrder_.at(currentStep_));
    grammarPrompt_->setText(task.prompt);

    grammarGroup_->setExclusive(false);
    for (QAbstractButton* button : grammarGroup_->buttons()) {
        button->setChecked(false);
    }
    grammarGroup_->setExclusive(true);

    for (int i = 0; i < grammarOptions_.size(); ++i) {
        const bool visible = i < task.options.size();
        grammarOptions_[i]->setVisible(visible);
        if (visible) {
            grammarOptions_[i]->setText(task.options.at(i));
        }
    }

    pagesStack_->setCurrentIndex(kGrammarPage);
}

void MainWindow::advanceAfterAnswer(bool correct, const QString& detail) {
    if (correct) {
        ++correctAnswers_;
        feedbackLabel_->setText(detail);
        statusBar()->showMessage("Ответ засчитан", 1800);
    } else {
        ++wrongAttempts_;
        perfectExercise_ = false;
        QApplication::beep();
        feedbackLabel_->setText(QString("%1 Осталось ошибок: %2.")
                                    .arg(detail)
                                    .arg(qMax(0, activeMaxWrongAttempts_ - wrongAttempts_)));
        statusBar()->showMessage("Ответ не засчитан", 2200);
    }

    if (wrongAttempts_ >= activeMaxWrongAttempts_) {
        finishExercise(
            false, "Попытки закончились",
            QString("Неверных ответов: %1 из %2. %3")
                .arg(wrongAttempts_)
                .arg(activeMaxWrongAttempts_)
                .arg(detail));
        return;
    }

    ++currentStep_;
    if (currentStep_ >= exerciseOrder_.size()) {
        if (perfectExercise_ && correctAnswers_ == exerciseOrder_.size()) {
            finishExercise(
                true, "Идеальный проход",
                QString("Все %1 заданий выполнены без ошибок. Начислено %2 баллов.")
                    .arg(exerciseOrder_.size())
                    .arg(activePoints_));
        } else {
            finishExercise(
                false, "Упражнение завершено",
                QString("Верных ответов: %1 из %2. Баллы начисляются только за полностью "
                        "безошибочный проход.")
                    .arg(correctAnswers_)
                    .arg(exerciseOrder_.size()));
        }
        return;
    }

    showCurrentTask();
}

void MainWindow::finishExercise(bool success, const QString& title, const QString& detail) {
    if (!activeExercise_) {
        return;
    }

    stopExerciseTimer();
    activeExercise_ = false;

    if (success) {
        totalScore_ += activePoints_;
        ++streak_;
        QApplication::beep();
    } else {
        streak_ = 0;
    }

    resultTitle_->setText(title);
    resultText_->setText(detail);
    pagesStack_->setCurrentIndex(kResultPage);
    feedbackLabel_->setText(detail);

    updateStatusPanel();
    saveSettings();
    QMessageBox::information(this, title, detail);
}

void MainWindow::stopExerciseTimer() {
    if (exerciseTimer_) {
        exerciseTimer_->stop();
    }
}

void MainWindow::updateStatusPanel() {
    if (difficultyValue_) {
        difficultyValue_->setText(difficultyName());
    }
    if (scoreValue_) {
        scoreValue_->setText(QString::number(totalScore_));
    }
    if (streakValue_) {
        streakValue_->setText(QString::number(streak_));
    }
    const QString currentMode = activeExercise_ ? exerciseName() : exerciseName(selectedExercise_);
    if (modeValue_) {
        modeValue_->setText(QString("Режим: %1").arg(currentMode));
    }
    if (homeModeValue_) {
        homeModeValue_->setText(QString("Текущий режим: %1").arg(currentMode));
    }

    const int minutes = qMax(0, remainingSeconds_) / 60;
    const int seconds = qMax(0, remainingSeconds_) % 60;
    if (timerValue_) {
        timerValue_->setText(
            activeExercise_ ? QString("%1:%2").arg(minutes).arg(seconds, 2, 10, QLatin1Char('0')) : "--:--");
    }
    const bool hasPreviousRun = !exerciseOrder_.isEmpty();
    const int shownWrongAttempts = activeExercise_ || hasPreviousRun ? wrongAttempts_ : 0;
    const int shownMaxWrong =
        activeExercise_ || activeMaxWrongAttempts_ > 0 ? activeMaxWrongAttempts_ : maxWrongAttemptsForDifficulty();
    if (attemptsValue_) {
        attemptsValue_->setText(QString("%1 / %2").arg(shownWrongAttempts).arg(shownMaxWrong));
    }

    updateExerciseStatus();
}

void MainWindow::updateExerciseStatus() {
    const bool hasPreviousRun = !exerciseOrder_.isEmpty();
    const int total = activeExercise_ || hasPreviousRun ? exerciseOrder_.size() : taskCountForDifficulty();
    const int current = activeExercise_ || hasPreviousRun ? qMin(currentStep_, total) : 0;

    progressText_->setText(QString("%1 из %2").arg(current).arg(total));
    progressBar_->setRange(0, qMax(1, total));
    progressBar_->setValue(qBound(0, current, total));

    const int timeLimit = activeExercise_ || activeTimeLimit_ > 0 ? activeTimeLimit_ : timeLimitForDifficulty();
    timerBar_->setRange(0, qMax(1, timeLimit));
    timerBar_->setValue(qBound(0, remainingSeconds_, timeLimit));
}

int MainWindow::taskCountForDifficulty() const {
    switch (difficulty_) {
        case Difficulty::Easy:
            return 4;
        case Difficulty::Normal:
            return 6;
        case Difficulty::Hard:
            return 8;
    }

    return 6;
}

int MainWindow::timeLimitForDifficulty() const {
    switch (difficulty_) {
        case Difficulty::Easy:
            return 180;
        case Difficulty::Normal:
            return 150;
        case Difficulty::Hard:
            return 120;
    }

    return 150;
}

int MainWindow::maxWrongAttemptsForDifficulty() const {
    switch (difficulty_) {
        case Difficulty::Easy:
            return 3;
        case Difficulty::Normal:
            return 2;
        case Difficulty::Hard:
            return 1;
    }

    return 2;
}

int MainWindow::pointsForDifficulty() const {
    switch (difficulty_) {
        case Difficulty::Easy:
            return 50;
        case Difficulty::Normal:
            return 100;
        case Difficulty::Hard:
            return 160;
    }

    return 100;
}

QString MainWindow::difficultyName() const {
    switch (difficulty_) {
        case Difficulty::Easy:
            return "Easy";
        case Difficulty::Normal:
            return "Normal";
        case Difficulty::Hard:
            return "Hard";
    }

    return "Normal";
}

QString MainWindow::exerciseName() const {
    return exerciseName(currentExercise_);
}

QString MainWindow::exerciseName(ExerciseType type) const {
    switch (type) {
        case ExerciseType::Translation:
            return "Translation";
        case ExerciseType::Grammar:
            return "Grammar";
        case ExerciseType::None:
            return "Ожидание";
    }

    return "Ожидание";
}

QString MainWindow::currentHint() const {
    if (!activeExercise_ || currentStep_ < 0 || currentStep_ >= exerciseOrder_.size()) {
        return "Запустите Translation или Grammar в окне настроек. Сложность выбирается там же.";
    }

    if (currentExercise_ == ExerciseType::Translation) {
        return translationTasks_.at(exerciseOrder_.at(currentStep_)).hint;
    }

    if (currentExercise_ == ExerciseType::Grammar) {
        return grammarTasks_.at(exerciseOrder_.at(currentStep_)).hint;
    }

    return "Подсказка появится после запуска упражнения.";
}

bool MainWindow::isTranslationCorrect(const QString& userAnswer, const TranslationTask& task) const {
    const QString normalizedUser = normalizeForCompare(userAnswer);
    if (normalizedUser.isEmpty()) {
        return false;
    }

    QStringList expected;
    expected << task.answer << task.variants;

    for (const QString& candidate : expected) {
        const QString normalizedExpected = normalizeForCompare(candidate);
        if (normalizedExpected.isEmpty()) {
            continue;
        }

        if (normalizedUser == normalizedExpected) {
            return true;
        }

        const int longest = qMax(normalizedUser.size(), normalizedExpected.size());
        const int distance = levenshteinDistance(normalizedUser, normalizedExpected);
        const int allowedDistance = longest < 24 ? 2 : qMax(3, longest / 8);
        if (distance <= allowedDistance) {
            return true;
        }

        if (tokenSimilarityEnough(normalizedUser, normalizedExpected)) {
            return true;
        }
    }

    return false;
}

QString MainWindow::normalizeForCompare(const QString& value) const {
    QString prepared = value.toLower();
    prepared.replace("ё", "е");
    prepared = prepared.normalized(QString::NormalizationForm_D);

    QString result;
    result.reserve(prepared.size());

    for (const QChar ch : prepared) {
        const QChar::Category category = ch.category();
        if (category == QChar::Mark_NonSpacing || category == QChar::Mark_SpacingCombining ||
            category == QChar::Mark_Enclosing) {
            continue;
        }

        if (ch.isLetterOrNumber()) {
            result.append(ch);
        } else {
            result.append(' ');
        }
    }

    return result.simplified();
}

int MainWindow::levenshteinDistance(const QString& left, const QString& right) const {
    if (left == right) {
        return 0;
    }

    if (left.isEmpty()) {
        return right.size();
    }

    if (right.isEmpty()) {
        return left.size();
    }

    QVector<int> previous(right.size() + 1);
    QVector<int> current(right.size() + 1);

    for (int j = 0; j <= right.size(); ++j) {
        previous[j] = j;
    }

    for (int i = 1; i <= left.size(); ++i) {
        current[0] = i;
        for (int j = 1; j <= right.size(); ++j) {
            const int cost = left.at(i - 1) == right.at(j - 1) ? 0 : 1;
            current[j] = qMin(qMin(current[j - 1] + 1, previous[j] + 1), previous[j - 1] + cost);
        }
        previous.swap(current);
    }

    return previous[right.size()];
}

bool MainWindow::tokenSimilarityEnough(const QString& left, const QString& right) const {
    const QStringList leftTokens = left.split(' ', Qt::SkipEmptyParts);
    const QStringList rightTokens = right.split(' ', Qt::SkipEmptyParts);

    if (leftTokens.isEmpty() || rightTokens.isEmpty()) {
        return false;
    }

    QVector<bool> used(rightTokens.size(), false);
    int matches = 0;

    for (const QString& leftToken : leftTokens) {
        for (int i = 0; i < rightTokens.size(); ++i) {
            if (used[i]) {
                continue;
            }

            const QString& rightToken = rightTokens.at(i);
            const bool same = leftToken == rightToken;
            const bool typo = leftToken.size() > 4 && rightToken.size() > 4 &&
                              levenshteinDistance(leftToken, rightToken) <= 1;

            if (same || typo) {
                used[i] = true;
                ++matches;
                break;
            }
        }
    }

    const double ratio = static_cast<double>(matches) / qMax(leftTokens.size(), rightTokens.size());
    return ratio >= 0.82;
}
