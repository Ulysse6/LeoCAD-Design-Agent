#include "lc_global.h"
#include "lc_aiwidget.h"
#include "lc_mainwindow.h"
#include "lc_application.h"
#include "lc_model.h"
#include "lc_view.h"
#include "project.h"
#include "piece.h"
#include "lc_piecedata.h"
#include "lc_library.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QTextBrowser>
#include <QLineEdit>
#include <QPushButton>
#include <QProgressBar>
#include <QCheckBox>
#include <QComboBox>
#include <QSlider>
#include <QLabel>
#include <QGroupBox>
#include <QScrollArea>
#include <QBuffer>
#include <QDateTime>

lcAIAssistantWidget::lcAIAssistantWidget(QWidget* Parent)
	: QWidget(Parent)
{
	QVBoxLayout* MainLayout = new QVBoxLayout(this);
	MainLayout->setContentsMargins(8, 8, 8, 8);
	MainLayout->setSpacing(6);

	// Header banner
	QWidget* HeaderWidget = new QWidget(this);
	QHBoxLayout* HeaderLayout = new QHBoxLayout(HeaderWidget);
	HeaderLayout->setContentsMargins(4, 4, 4, 4);

	QLabel* TitleLabel = new QLabel(tr("🤖 <b>AI LEGO Builder</b>"), HeaderWidget);
	TitleLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: #4a90e2;");
	HeaderLayout->addWidget(TitleLabel);
	HeaderLayout->addStretch();

	mClearButton = new QPushButton(tr("Clear"), HeaderWidget);
	mClearButton->setToolTip(tr("Clear conversation history"));
	mClearButton->setStyleSheet("font-size: 11px; padding: 2px 8px;");
	connect(mClearButton, &QPushButton::clicked, this, &lcAIAssistantWidget::OnClearChat);
	HeaderLayout->addWidget(mClearButton);

	MainLayout->addWidget(HeaderWidget);

	// Chat conversation view
	mChatBrowser = new QTextBrowser(this);
	mChatBrowser->setOpenExternalLinks(true);
	mChatBrowser->setStyleSheet(
		"QTextBrowser {"
		"  background-color: #1e1e24;"
		"  color: #e0e0e0;"
		"  border: 1px solid #3c3c46;"
		"  border-radius: 6px;"
		"  padding: 6px;"
		"  font-family: sans-serif;"
		"  font-size: 12px;"
		"}"
	);
	MainLayout->addWidget(mChatBrowser, 1);

	// Quick Prompt Presets Header
	QLabel* PresetsLabel = new QLabel(tr("<b>Quick Suggestions:</b>"), this);
	PresetsLabel->setStyleSheet("font-size: 11px; color: #888899; margin-top: 2px;");
	MainLayout->addWidget(PresetsLabel);

	// Presets scrollable container
	QWidget* PresetsWidget = new QWidget(this);
	QGridLayout* PresetsGrid = new QGridLayout(PresetsWidget);
	PresetsGrid->setContentsMargins(0, 0, 0, 0);
	PresetsGrid->setSpacing(4);

	struct PresetInfo { QString Icon; QString Label; QString Prompt; };
	const PresetInfo Presets[] = {
		{ "🛻", "Pickup", "Build a racing pickup truck" },
		{ "🚜", "Tractor", "Build a loader tractor" },
		{ "🏎️", "Racer", "Build a rally racer" },
		{ "🚒", "Tow Truck", "Build a super tow truck" },
		{ "🚁", "Helicopter", "Build a rescue helicopter" },
		{ "🏁", "Dragster", "Build a racing dragster" },
		{ "🛸", "X-Wing", "Build an x-wing starfighter" },
		{ "🏠", "House", "Build a cozy house" },
		{ "🚤", "Speedboat", "Build a motor speedboat" },
		{ "🚂", "Train", "Build a freight train wagon" },
		{ "🤖", "Robot", "Build a mini robot" },
		{ "🗼", "Skyscraper", "Build the Empire State Building" }
	};

	int ColCount = 3;
	for (size_t i = 0; i < sizeof(Presets) / sizeof(Presets[0]); ++i)
	{
		QPushButton* Btn = new QPushButton(Presets[i].Icon + " " + Presets[i].Label, PresetsWidget);
		Btn->setStyleSheet("QPushButton { font-size: 11px; padding: 4px 6px; text-align: left; }");
		Btn->setProperty("promptText", Presets[i].Prompt);
		connect(Btn, &QPushButton::clicked, this, &lcAIAssistantWidget::OnPresetClicked);
		PresetsGrid->addWidget(Btn, static_cast<int>(i / ColCount), static_cast<int>(i % ColCount));
	}
	MainLayout->addWidget(PresetsWidget);

	// Settings & Options row
	QGroupBox* OptionsBox = new QGroupBox(tr("Build Options"), this);
	OptionsBox->setStyleSheet("QGroupBox { font-size: 11px; font-weight: bold; margin-top: 4px; }");
	QVBoxLayout* OptionsLayout = new QVBoxLayout(OptionsBox);
	OptionsLayout->setContentsMargins(6, 6, 6, 6);
	OptionsLayout->setSpacing(4);

	QHBoxLayout* OptRow1 = new QHBoxLayout();
	mModeCombo = new QComboBox(OptionsBox);
	mModeCombo->addItem(tr("Replace current model"), true);
	mModeCombo->addItem(tr("Add to current model"), false);
	mModeCombo->setStyleSheet("font-size: 11px;");
	OptRow1->addWidget(new QLabel(tr("Mode:"), OptionsBox));
	OptRow1->addWidget(mModeCombo, 1);
	OptionsLayout->addLayout(OptRow1);

	QHBoxLayout* OptRow2 = new QHBoxLayout();
	mAnimateCheckBox = new QCheckBox(tr("Animate build"), OptionsBox);
	mAnimateCheckBox->setChecked(true);
	mAnimateCheckBox->setToolTip(tr("Watch the LEGO model assemble brick by brick in real-time!"));
	OptRow2->addWidget(mAnimateCheckBox);

	OptRow2->addWidget(new QLabel(tr("Speed:"), OptionsBox));
	mSpeedSlider = new QSlider(Qt::Horizontal, OptionsBox);
	mSpeedSlider->setRange(10, 200);
	mSpeedSlider->setValue(40);
	mSpeedSlider->setToolTip(tr("Assembly animation speed"));
	OptRow2->addWidget(mSpeedSlider, 1);
	OptionsLayout->addLayout(OptRow2);

	MainLayout->addWidget(OptionsBox);

	// Progress & Status Bar
	mProgressBar = new QProgressBar(this);
	mProgressBar->setRange(0, 100);
	mProgressBar->setValue(0);
	mProgressBar->setTextVisible(true);
	mProgressBar->setStyleSheet("QProgressBar { height: 14px; font-size: 10px; }");
	MainLayout->addWidget(mProgressBar);

	mStatusLabel = new QLabel(tr("Ready for your LEGO building requests"), this);
	mStatusLabel->setStyleSheet("font-size: 11px; color: #8899aa;");
	MainLayout->addWidget(mStatusLabel);

	// Input Row
	QHBoxLayout* InputLayout = new QHBoxLayout();
	mPromptInput = new QLineEdit(this);
	mPromptInput->setPlaceholderText(tr("Ask the agent to build any LEGO model..."));
	mPromptInput->setStyleSheet("font-size: 12px; padding: 6px; border-radius: 4px;");
	connect(mPromptInput, &QLineEdit::returnPressed, this, &lcAIAssistantWidget::OnSendClicked);
	InputLayout->addWidget(mPromptInput, 1);

	mSendButton = new QPushButton(tr("Build 🔨"), this);
	mSendButton->setStyleSheet("font-weight: bold; background-color: #357abd; color: white; padding: 6px 14px;");
	connect(mSendButton, &QPushButton::clicked, this, &lcAIAssistantWidget::OnSendClicked);
	InputLayout->addWidget(mSendButton);

	mStopButton = new QPushButton(tr("Stop ⏹"), this);
	mStopButton->setEnabled(false);
	mStopButton->setStyleSheet("padding: 6px 10px;");
	connect(mStopButton, &QPushButton::clicked, this, &lcAIAssistantWidget::OnStopBuild);
	InputLayout->addWidget(mStopButton);

	MainLayout->addLayout(InputLayout);

	// Timer for piece-by-piece assembly
	connect(&mAnimTimer, &QTimer::timeout, this, &lcAIAssistantWidget::OnAnimationTick);

	// Initial welcome message
	AppendSystemMessage(tr("<b>Welcome to the LeoCAD AI Assistant!</b><br>"
	                       "I am your autonomous 3D LEGO builder agent, trained on over 100 authentic LEGO sets from the Official Model Repository (pickups, tractors, race cars, helicopters, tow trucks, spaceships, architecture) with procedural fallbacks. "
	                       "Tell me what you'd like to build, or pick one of the quick suggestions above!"));
}

void lcAIAssistantWidget::AppendSystemMessage(const QString& Text)
{
	mChatBrowser->append(
		QString("<div style='background-color: #262933; border: 1px solid #3c4252; padding: 8px; border-radius: 6px; margin: 4px 0;'>"
		        "<span style='color: #4a90e2;'>🤖 <b>Agent:</b></span> %1</div>").arg(Text)
	);
}

void lcAIAssistantWidget::AppendUserMessage(const QString& Text)
{
	mChatBrowser->append(
		QString("<div style='background-color: #2b394d; border-left: 4px solid #357abd; padding: 8px; border-radius: 6px; margin: 6px 0; text-align: left;'>"
		        "<span style='color: #79b8ff;'>👤 <b>You:</b></span> %1</div>").arg(Text.toHtmlEscaped())
	);
}

void lcAIAssistantWidget::AppendAgentMessage(const QString& Title, const QString& Explanation,
                                            int BrickCount, int StepCount, const QMap<QString, int>& PartCounts)
{
	QString PartsHtml;
	int Count = 0;
	for (auto it = PartCounts.constBegin(); it != PartCounts.constEnd(); ++it)
	{
		if (Count++ > 0) PartsHtml += ", ";
		PartsHtml += QString("<b>%1</b> (x%2)").arg(it.key(), QString::number(it.value()));
	}

	mChatBrowser->append(
		QString("<div style='background-color: #21262d; border: 1px solid #30363d; border-radius: 6px; padding: 10px; margin: 6px 0;'>"
		        "<div style='color: #58a6ff; font-weight: bold; font-size: 13px;'>✨ %1</div>"
		        "<div style='margin-top: 6px; color: #c9d1d9; line-height: 1.4;'>%2</div>"
		        "<div style='margin-top: 8px; padding-top: 6px; border-top: 1px solid #30363d; font-size: 11px; color: #8b949e;'>"
		        "🧱 <b>Bricks:</b> %3 | 📋 <b>Steps:</b> %4<br>"
		        "📦 <b>Parts:</b> %5"
		        "</div>"
		        "</div>")
		        .arg(Title.toHtmlEscaped(), Explanation.toHtmlEscaped(),
		             QString::number(BrickCount), QString::number(StepCount), PartsHtml)
	);
}

void lcAIAssistantWidget::OnPresetClicked()
{
	QPushButton* Btn = qobject_cast<QPushButton*>(sender());
	if (!Btn) return;
	QString Prompt = Btn->property("promptText").toString();
	mPromptInput->setText(Prompt);
	BuildPrompt(Prompt);
}

void lcAIAssistantWidget::OnClearChat()
{
	mChatBrowser->clear();
	AppendSystemMessage(tr("Conversation history cleared. What would you like to build next?"));
}

void lcAIAssistantWidget::OnSendClicked()
{
	QString Prompt = mPromptInput->text().trimmed();
	if (Prompt.isEmpty()) return;
	BuildPrompt(Prompt);
}

void lcAIAssistantWidget::BuildPrompt(const QString& Prompt)
{
	if (Prompt.isEmpty()) return;

	mAnimTimer.stop();
	AppendUserMessage(Prompt);
	mPromptInput->clear();

	mStatusLabel->setText(tr("Analyzing request and synthesizing 3D LEGO model..."));
	mProgressBar->setValue(10);
	mSendButton->setEnabled(false);
	mStopButton->setEnabled(true);

	lcAIOptions Opts;
	Opts.ReplaceCurrent = mModeCombo->currentData().toBool();
	Opts.StepByStep = true;

	lcAIModelResult Res = lcAIGenerator::GenerateModel(Prompt, Opts);

	if (!Res.Success)
	{
		AppendSystemMessage(tr("❌ Sorry, I encountered an error creating this model: %1").arg(Res.ErrorMessage));
		mStatusLabel->setText(tr("Error generating model"));
		mSendButton->setEnabled(true);
		mStopButton->setEnabled(false);
		mProgressBar->setValue(0);
		return;
	}

	AppendAgentMessage(Res.Title, Res.Explanation, Res.BrickCount, Res.StepCount, Res.PartCounts);
	ExecuteModelBuild(Res);
}

void lcAIAssistantWidget::ExecuteModelBuild(const lcAIModelResult& Result)
{
	mCurrentModelTitle = Result.Title;
	bool Replace = mModeCombo->currentData().toBool();
	bool Animate = mAnimateCheckBox->isChecked();

	if (Replace)
	{
		Project* NewProject = new Project();
		gApplication->SetProject(NewProject);
	}

	lcModel* ActiveModel = gMainWindow ? gMainWindow->GetActiveModel() : (lcGetActiveProject() ? lcGetActiveProject()->GetActiveModel() : nullptr);
	if (ActiveModel)
		ActiveModel->SetFileName(Result.Title + ".ldr");

	if (!Animate)
	{
		// Instant load: parse all pieces into project
		QBuffer Buffer;
		QByteArray Content = Result.LDrawContent;
		Buffer.setData(Content);
		Buffer.open(QIODevice::ReadOnly);

		if (ActiveModel)
		{
			ActiveModel->LoadLDraw(Buffer, lcGetActiveProject());
			ActiveModel->CalculateStep(Result.StepCount);
			ActiveModel->UpdateAllViews();
		}

		if (gMainWindow)
		{
			gMainWindow->UpdateTimeline(true, true);
			if (gMainWindow->GetActiveView())
				gMainWindow->GetActiveView()->ZoomExtents(lcGeometryBoundsMode::IncludeFallback, false);
		}

		mProgressBar->setValue(100);
		mStatusLabel->setText(tr("Built %1 with %2 pieces!").arg(Result.Title).arg(Result.BrickCount));
		mSendButton->setEnabled(true);
		mStopButton->setEnabled(false);
		return;
	}

	// Animated build: queue all pieces and fire timer
	mAnimQueue = Result.Pieces;
	mAnimIndex = 0;
	mProgressBar->setValue(10);
	mProgressBar->setMaximum(100);

	int Interval = mSpeedSlider->value();
	mAnimTimer.start(Interval);
}

void lcAIAssistantWidget::OnAnimationTick()
{
	if (mAnimIndex >= mAnimQueue.size())
	{
		mAnimTimer.stop();
		mProgressBar->setValue(100);
		mStatusLabel->setText(tr("Completed assembly of %1!").arg(mCurrentModelTitle));
		mSendButton->setEnabled(true);
		mStopButton->setEnabled(false);

		if (gMainWindow)
		{
			gMainWindow->UpdateTimeline(true, true);
			if (gMainWindow->GetActiveView())
				gMainWindow->GetActiveView()->ZoomExtents(lcGeometryBoundsMode::IncludeFallback, false);
		}
		return;
	}

	const lcAIModelPiece& PieceInfo = mAnimQueue[mAnimIndex];
	lcModel* Model = gMainWindow ? gMainWindow->GetActiveModel() : (lcGetActiveProject() ? lcGetActiveProject()->GetActiveModel() : nullptr);
	Project* CurrentProject = lcGetActiveProject();
	lcPiecesLibrary* Library = lcGetPiecesLibrary();

	if (Model && CurrentProject && Library)
	{
		lcPieceDataRef PieceData = Library->AcquirePieceData(PieceInfo.PartId.toLatin1().constData(), CurrentProject,
		                                                     lcAcquirePieceDataFlag::CreateMissing | lcAcquirePieceDataFlag::SearchProjectFolder);

		// Transform matrix computation matching LeoCAD LDraw conventions
		lcMatrix44 IncludeTransform(
			lcVector4(PieceInfo.R[0], PieceInfo.R[3], PieceInfo.R[6], 0.0f),
			lcVector4(PieceInfo.R[1], PieceInfo.R[4], PieceInfo.R[7], 0.0f),
			lcVector4(PieceInfo.R[2], PieceInfo.R[5], PieceInfo.R[8], 0.0f),
			lcVector4(PieceInfo.X, PieceInfo.Y, PieceInfo.Z, 1.0f)
		);
		const float* Matrix = IncludeTransform.GetFloats();
		const lcMatrix44 Transform(
			lcVector4(Matrix[0], Matrix[2], -Matrix[1], 0.0f),
			lcVector4(Matrix[8], Matrix[10], -Matrix[9], 0.0f),
			lcVector4(-Matrix[4], -Matrix[6], Matrix[5], 0.0f),
			lcVector4(Matrix[12], Matrix[14], -Matrix[13], 1.0f)
		);

		lcPiece* Piece = new lcPiece(nullptr);
		Piece->SetPieceData(std::move(PieceData), PieceInfo.PartId, false, false);
		Piece->Initialize(Transform, PieceInfo.Step);
		Piece->SetColorCode(PieceInfo.ColorCode);
		Model->AddPiece(std::unique_ptr<lcPiece>(Piece), Model->GetPieces().size());
		Model->CalculateStep(PieceInfo.Step);
		Model->UpdateAllViews();

		// Keep view centered if early in build
		if (mAnimIndex == 0 && gMainWindow && gMainWindow->GetActiveView())
			gMainWindow->GetActiveView()->ZoomExtents(lcGeometryBoundsMode::IncludeFallback, false);
	}

	mAnimIndex++;
	int Pct = static_cast<int>((mAnimIndex * 100) / mAnimQueue.size());
	mProgressBar->setValue(Pct);
	mStatusLabel->setText(tr("Placing piece %1 of %2...").arg(mAnimIndex).arg(mAnimQueue.size()));
}

void lcAIAssistantWidget::OnStopBuild()
{
	mAnimTimer.stop();
	mStatusLabel->setText(tr("Assembly stopped. %1 pieces placed.").arg(mAnimIndex));
	mSendButton->setEnabled(true);
	mStopButton->setEnabled(false);

	if (gMainWindow)
	{
		gMainWindow->UpdateTimeline(true, true);
		if (gMainWindow->GetActiveView())
			gMainWindow->GetActiveView()->ZoomExtents(lcGeometryBoundsMode::IncludeFallback, false);
	}
}
