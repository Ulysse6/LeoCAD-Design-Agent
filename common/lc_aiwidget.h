#pragma once

#include <QWidget>
#include <QTimer>
#include <QImage>
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
class QSpinBox;
class QDragEnterEvent;
class QDropEvent;

class lcAIAssistantWidget : public QWidget
{
	Q_OBJECT

public:
	explicit lcAIAssistantWidget(QWidget* Parent = nullptr);
	~lcAIAssistantWidget() override = default;

public slots:
	void BuildPrompt(const QString& Prompt);

protected:
	void dragEnterEvent(QDragEnterEvent* Event) override;
	void dropEvent(QDropEvent* Event) override;

private slots:
	void OnSendClicked();
	void OnPresetClicked();
	void OnClearChat();
	void OnStopBuild();
	void OnAnimationTick();
	void OnAttachImageClicked();
	void OnRemoveImageClicked();
	void OnPiecePresetChanged(int Index);

private:
	void AppendUserMessage(const QString& Text, const QString& ImagePath = QString(), int TargetPieces = 0);
	void AppendAgentMessage(const QString& Title, const QString& Explanation,
	                        int BrickCount, int StepCount, const QMap<QString, int>& PartCounts);
	void AppendSystemMessage(const QString& Text);
	void ExecuteModelBuild(const lcAIModelResult& Result);
	void SetAttachedImage(const QString& Path);
	void ClearAttachedImage();

	// UI Controls
	QTextBrowser* mChatBrowser = nullptr;
	QLineEdit* mPromptInput = nullptr;
	QPushButton* mSendButton = nullptr;
	QPushButton* mStopButton = nullptr;
	QPushButton* mClearButton = nullptr;
	QProgressBar* mProgressBar = nullptr;
	QLabel* mStatusLabel = nullptr;

	// Image Attachment
	QWidget* mAttachmentBar = nullptr;
	QLabel* mThumbLabel = nullptr;
	QLabel* mImageNameLabel = nullptr;
	QPushButton* mAttachBtn = nullptr;
	QPushButton* mRemoveImageBtn = nullptr;
	QString mAttachedImagePath;
	QImage mAttachedImage;

	// Options
	QCheckBox* mAnimateCheckBox = nullptr;
	QComboBox* mModeCombo = nullptr;
	QSlider* mSpeedSlider = nullptr;
	QSpinBox* mPieceCountSpin = nullptr;
	QComboBox* mPiecePresetCombo = nullptr;

	// Animation State
	QTimer mAnimTimer;
	std::vector<lcAIModelPiece> mAnimQueue;
	size_t mAnimIndex = 0;
	int mCurrentStep = 1;
	QString mCurrentModelTitle;
};
