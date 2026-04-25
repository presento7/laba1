#ifndef APP_MAIN_WINDOW_H
#define APP_MAIN_WINDOW_H

#include <QMainWindow>
#include <QString>
#include <QStringList>
#include <QVector>

QT_BEGIN_NAMESPACE
class QAction;
class QButtonGroup;
class QCloseEvent;
class QFrame;
class QKeyEvent;
class QLabel;
class QProgressBar;
class QPushButton;
class QRadioButton;
class QStackedWidget;
class QTextEdit;
class QTimer;
QT_END_NAMESPACE

enum class Difficulty {
    Easy,
    Normal,
    Hard,
};

enum class ExerciseType {
    None,
    Translation,
    Grammar,
};

struct TranslationTask {
    QString prompt;
    QString answer;
    QStringList variants;
    QString hint;
};

struct GrammarTask {
    QString prompt;
    QStringList options;
    int correctIndex = 0;
    QString hint;
};

class MainWindow : public QMainWindow {
    Q_OBJECT

   public:
    MainWindow();

   protected:
    void closeEvent(QCloseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

   private slots:
    void startTranslationExercise();
    void startGrammarExercise();
    void startSelectedExercise();
    void submitTranslationAnswer();
    void submitGrammarAnswer();
    void tickTimer();
    void openSettingsDialog();
    void resetCurrentExercise();
    void showCurrentHelp();

   private:
    void buildUi();
    void buildMenus();
    void buildTopBar(QFrame* topBar);
    void buildPages();
    void applyStyle();
    void seedTasks();
    void loadSettings();
    void saveSettings() const;

    void startExercise(ExerciseType type);
    void generateExerciseOrder(int sourceSize);
    void showCurrentTask();
    void showTranslationTask();
    void showGrammarTask();
    void advanceAfterAnswer(bool correct, const QString& detail);
    void finishExercise(bool success, const QString& title, const QString& detail);
    void stopExerciseTimer();
    void updateStatusPanel();
    void updateExerciseStatus();

    int taskCountForDifficulty() const;
    int timeLimitForDifficulty() const;
    int maxWrongAttemptsForDifficulty() const;
    int pointsForDifficulty() const;
    QString difficultyName() const;
    QString exerciseName() const;
    QString exerciseName(ExerciseType type) const;
    QString currentHint() const;

    bool isTranslationCorrect(const QString& userAnswer, const TranslationTask& task) const;
    QString normalizeForCompare(const QString& value) const;
    int levenshteinDistance(const QString& left, const QString& right) const;
    bool tokenSimilarityEnough(const QString& left, const QString& right) const;

    Difficulty difficulty_ = Difficulty::Normal;
    ExerciseType currentExercise_ = ExerciseType::None;
    ExerciseType selectedExercise_ = ExerciseType::Translation;
    QVector<TranslationTask> translationTasks_;
    QVector<GrammarTask> grammarTasks_;
    QVector<int> exerciseOrder_;

    int currentStep_ = 0;
    int correctAnswers_ = 0;
    int wrongAttempts_ = 0;
    int remainingSeconds_ = 0;
    int totalScore_ = 0;
    int streak_ = 0;
    int activeTaskCount_ = 0;
    int activeTimeLimit_ = 0;
    int activeMaxWrongAttempts_ = 0;
    int activePoints_ = 0;
    bool activeExercise_ = false;
    bool perfectExercise_ = true;

    QTimer* exerciseTimer_ = nullptr;
    QStackedWidget* exerciseStack_ = nullptr;
    QStackedWidget* pagesStack_ = nullptr;

    QLabel* difficultyValue_ = nullptr;
    QLabel* scoreValue_ = nullptr;
    QLabel* streakValue_ = nullptr;
    QLabel* modeValue_ = nullptr;
    QLabel* timerValue_ = nullptr;
    QLabel* attemptsValue_ = nullptr;
    QLabel* progressText_ = nullptr;
    QLabel* feedbackLabel_ = nullptr;
    QLabel* homeModeValue_ = nullptr;
    QLabel* translationPrompt_ = nullptr;
    QLabel* translationHint_ = nullptr;
    QLabel* grammarPrompt_ = nullptr;
    QLabel* grammarHint_ = nullptr;
    QLabel* resultTitle_ = nullptr;
    QLabel* resultText_ = nullptr;

    QProgressBar* progressBar_ = nullptr;
    QProgressBar* timerBar_ = nullptr;

    QPushButton* resetButton_ = nullptr;
    QPushButton* settingsButton_ = nullptr;
    QPushButton* startGameButton_ = nullptr;
    QPushButton* submitTranslationButton_ = nullptr;
    QPushButton* submitGrammarButton_ = nullptr;

    QTextEdit* translationAnswer_ = nullptr;
    QButtonGroup* grammarGroup_ = nullptr;
    QVector<QRadioButton*> grammarOptions_;
};

#endif
