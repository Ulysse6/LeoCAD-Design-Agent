#pragma once

#include <QWidget>
#include <QTimer>
#include <vector>
#include "lc_aigenerator.h"

class QTextBrowser;
class QLineEdit;
class QPushButton;
class QProgressBar;
class QCheckBox;
class QComboBox;
class QSlider;
class QLabel;

class lcAIAssistantWidget : public QWidget
{
	Q_OBJECT

public:
	explicit lcAIAssistantWidget(QWidget* Parent = nullptr);
	~lcAIAssistantWidget() override = default;

public slots:
	void BuildPrompt(const QString& Prompt);

private slots:
	void OnSendClicked();
	void OnPresetClicked();
	void OnClearChat();
	void OnStopBuild();
	void OnAnimationTick();

private:
	void AppendUserMessage(const QString& Text);
	void AppendAgentMessage(const QString& Title, const QString& Explanation,
	                        int BrickCount, int StepCount, const QMap<QString, int>& PartCounts);
	void AppendSystemMessage(const QString& Text);
	void ExecuteModelBuild(const lcAIModelResult& Result);

	// UI Controls
	QTextBrowser* mChatBrowser = nullptr;
	QLineEdit* mPromptInput = nullptr;
	QPushButton* mSendButton = nullptr;
	QPushButton* mStopButton = nullptr;
	QPushButton* mClearButton = nullptr;
	QProgressBar* mProgressBar = nullptr;
	QLabel* mStatusLabel = nullptr;

	// Options
	QCheckBox* mAnimateCheckBox = nullptr;
	QComboBox* mModeCombo = nullptr;
	QSlider* mSpeedSlider = nullptr;

	// Animation State
	QTimer mAnimTimer;
	std::vector<lcAIModelPiece> mAnimQueue;
	size_t mAnimIndex = 0;
	int mCurrentStep = 1;
	QString mCurrentModelTitle;
};
