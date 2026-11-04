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
#include <QSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>

lcAIAssistantWidget::lcAIAssistantWidget(QWidget* Parent)
	: QWidget(Parent)
{
	setAcceptDrops(true);

	QVBoxLayout* MainLayout = new QVBoxLayout(this);
	MainLayout->setContentsMargins(8, 8, 8, 8);
	MainLayout->setSpacing(6);

	// Header banner
	QWidget* HeaderWidget = new QWidget(this);
	QHBoxLayout* HeaderLayout = new QHBoxLayout(HeaderWidget);
	HeaderLayout->setContentsMargins(4, 4, 4, 4);

	QLabel* TitleLabel = new QLabel(tr("🤖 <b>Multimodal AI LEGO Builder</b>"), HeaderWidget);
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
		{ "🛻", "Pickup", "Build a futuristic pickup truck" },
		{ "🏎️", "Racer", "Build a supersonic racing car" },
		{ "✈️", "Jet", "Build a stealth fighter jet" },
		{ "🚁", "Helicopter", "Build a rescue helicopter" },
		{ "🚀", "Spaceship", "Build an interstellar starfighter" },
		{ "🏰", "Castle", "Build a medieval castle tower" },
		{ "🏠", "Villa", "Build a modern hillside villa" },
		{ "🤖", "Robot", "Build an autonomous explorer mech" },
		{ "🚢", "Speedboat", "Build a hydrofoil speedboat" }
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

	// Generator Engine Selector (Native 3D vs Neural VLM)
	QHBoxLayout* OptRow0 = new QHBoxLayout();
	OptRow0->addWidget(new QLabel(tr("Generator:"), OptionsBox));
	mBackendCombo = new QComboBox(OptionsBox);
	mBackendCombo->addItem(tr("Native 3D Synthesizer (Instant / Offline)"), static_cast<int>(lcAIBackend::NativeSynthesizer));
	mBackendCombo->addItem(tr("Neural VLM (Ollama / Local)"), static_cast<int>(lcAIBackend::NeuralVLM));
	mBackendCombo->addItem(tr("Neural VLM (OpenAI / Cloud)"), static_cast<int>(lcAIBackend::NeuralVLM));
	mBackendCombo->setStyleSheet("font-size: 11px;");
	connect(mBackendCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &lcAIAssistantWidget::OnBackendChanged);
	OptRow0->addWidget(mBackendCombo, 1);
	OptionsLayout->addLayout(OptRow0);

	// VLM Settings (shown when Neural VLM selected)
	mVlmSettingsWidget = new QWidget(OptionsBox);
	mVlmSettingsWidget->setVisible(false);
	QHBoxLayout* VlmLayout = new QHBoxLayout(mVlmSettingsWidget);
	VlmLayout->setContentsMargins(0, 2, 0, 2);
	VlmLayout->setSpacing(4);

	mVlmEndpointInput = new QLineEdit(mVlmSettingsWidget);
	mVlmEndpointInput->setPlaceholderText("http://localhost:11434/api/generate");
	mVlmEndpointInput->setText("http://localhost:11434/api/generate");
	mVlmEndpointInput->setStyleSheet("font-size: 10px; padding: 2px 4px;");
	VlmLayout->addWidget(new QLabel(tr("URL:"), mVlmSettingsWidget));
	VlmLayout->addWidget(mVlmEndpointInput, 1);

	mVlmModelInput = new QLineEdit(mVlmSettingsWidget);
	mVlmModelInput->setPlaceholderText("qwen2-vl");
	mVlmModelInput->setText("qwen2-vl");
	mVlmModelInput->setStyleSheet("font-size: 10px; padding: 2px 4px;");
	VlmLayout->addWidget(new QLabel(tr("Model:"), mVlmSettingsWidget));
	VlmLayout->addWidget(mVlmModelInput);

	OptionsLayout->addWidget(mVlmSettingsWidget);

	// Build mode row
	QHBoxLayout* OptRow1 = new QHBoxLayout();
	mModeCombo = new QComboBox(OptionsBox);
	mModeCombo->addItem(tr("Replace current model"), true);
	mModeCombo->addItem(tr("Add to current model"), false);
	mModeCombo->setStyleSheet("font-size: 11px;");
	OptRow1->addWidget(new QLabel(tr("Mode:"), OptionsBox));
	OptRow1->addWidget(mModeCombo, 1);
	OptionsLayout->addLayout(OptRow1);

	// Animate & speed row
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

	// Piece Budget Conditioning
	QHBoxLayout* OptRow3 = new QHBoxLayout();
	OptRow3->addWidget(new QLabel(tr("Piece Budget:"), OptionsBox));

	mPiecePresetCombo = new QComboBox(OptionsBox);
	mPiecePresetCombo->addItem(tr("Auto (Natural)"), 0);
	mPiecePresetCombo->addItem(tr("Mini (~50 pcs)"), 50);
	mPiecePresetCombo->addItem(tr("Medium (~150 pcs)"), 150);
	mPiecePresetCombo->addItem(tr("Detailed (~300 pcs)"), 300);
	mPiecePresetCombo->addItem(tr("Complex (~500 pcs)"), 500);
	mPiecePresetCombo->addItem(tr("Custom"), -1);
	mPiecePresetCombo->setStyleSheet("font-size: 11px;");
	OptRow3->addWidget(mPiecePresetCombo);

	mPieceCountSpin = new QSpinBox(OptionsBox);
	mPieceCountSpin->setRange(0, 1000);
	mPieceCountSpin->setSpecialValueText(tr("Auto"));
	mPieceCountSpin->setSuffix(tr(" pcs"));
	mPieceCountSpin->setValue(0);
	mPieceCountSpin->setStyleSheet("font-size: 11px;");
	mPieceCountSpin->setToolTip(tr("Target number of pieces (0 for automatic model sizing)"));
	OptRow3->addWidget(mPieceCountSpin, 1);

	connect(mPiecePresetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &lcAIAssistantWidget::OnPiecePresetChanged);
	connect(mPieceCountSpin, QOverload<int>::of(&QSpinBox::valueChanged), [this](int val) {
		if (val == 0) mPiecePresetCombo->setCurrentIndex(0);
		else if (val == 50) mPiecePresetCombo->setCurrentIndex(1);
		else if (val == 150) mPiecePresetCombo->setCurrentIndex(2);
		else if (val == 300) mPiecePresetCombo->setCurrentIndex(3);
		else if (val == 500) mPiecePresetCombo->setCurrentIndex(4);
		else mPiecePresetCombo->setCurrentIndex(5);
	});

	OptionsLayout->addLayout(OptRow3);
	MainLayout->addWidget(OptionsBox);

	// Progress & Status Bar
	mProgressBar = new QProgressBar(this);
	mProgressBar->setRange(0, 100);
	mProgressBar->setValue(0);
	mProgressBar->setTextVisible(true);
	mProgressBar->setStyleSheet("QProgressBar { height: 14px; font-size: 10px; }");
	MainLayout->addWidget(mProgressBar);

	mStatusLabel = new QLabel(tr("Ready for your LEGO building requests (text or attached photo)"), this);
	mStatusLabel->setStyleSheet("font-size: 11px; color: #8899aa;");
	MainLayout->addWidget(mStatusLabel);

	// Image Attachment Preview Bar
	mAttachmentBar = new QWidget(this);
	mAttachmentBar->setVisible(false);
	mAttachmentBar->setStyleSheet("background-color: #24283b; border: 1px dashed #4a90e2; border-radius: 6px; padding: 4px; margin: 2px 0;");
	QHBoxLayout* AttachLayout = new QHBoxLayout(mAttachmentBar);
	AttachLayout->setContentsMargins(6, 4, 6, 4);
	AttachLayout->setSpacing(6);

	mThumbLabel = new QLabel(mAttachmentBar);
	mThumbLabel->setFixedSize(48, 48);
	mThumbLabel->setStyleSheet("border: 1px solid #444; border-radius: 4px; background: #111;");
	mThumbLabel->setScaledContents(true);
	AttachLayout->addWidget(mThumbLabel);

	mImageNameLabel = new QLabel(mAttachmentBar);
	mImageNameLabel->setStyleSheet("font-size: 11px; color: #79b8ff; font-weight: bold;");
	AttachLayout->addWidget(mImageNameLabel, 1);

	mRemoveImageBtn = new QPushButton(tr("✕"), mAttachmentBar);
	mRemoveImageBtn->setToolTip(tr("Remove reference image"));
	mRemoveImageBtn->setFixedSize(22, 22);
	mRemoveImageBtn->setStyleSheet("QPushButton { font-size: 11px; font-weight: bold; background: #442222; color: #ff8888; border: 1px solid #663333; border-radius: 11px; } QPushButton:hover { background: #662222; }");
	connect(mRemoveImageBtn, &QPushButton::clicked, this, &lcAIAssistantWidget::OnRemoveImageClicked);
	AttachLayout->addWidget(mRemoveImageBtn);

	MainLayout->addWidget(mAttachmentBar);

	// Input Row
	QHBoxLayout* InputLayout = new QHBoxLayout();

	mAttachBtn = new QPushButton(tr("📷"), this);
	mAttachBtn->setToolTip(tr("Attach reference image or photo (or drag & drop anywhere)"));
	mAttachBtn->setStyleSheet("font-size: 13px; padding: 6px 10px; background-color: #2a3342; color: #a5d6ff; border: 1px solid #3c495e; border-radius: 4px;");
	connect(mAttachBtn, &QPushButton::clicked, this, &lcAIAssistantWidget::OnAttachImageClicked);
	InputLayout->addWidget(mAttachBtn);

	mPromptInput = new QLineEdit(this);
	mPromptInput->setPlaceholderText(tr("Describe arbitrary model or attach reference photo..."));
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
	AppendSystemMessage(tr("<b>Welcome to the LeoCAD Multimodal AI Builder!</b><br>"
	                       "I synthesize completely novel, arbitrary 3D LEGO builds based on your text prompt and attached reference photos.<br>"
	                       "• <b>Arbitrary generation:</b> Describe anything (e.g. <i>'futuristic hovercraft'</i>, <i>'cyberpunk skyscraper'</i>, <i>'mech warrior'</i>) or attach any photo.<br>"
	                       "• <b>Piece Budget:</b> Choose a target budget (e.g. 50, 150, 300, 500 pcs) to control build density."));
}

void lcAIAssistantWidget::OnBackendChanged(int Index)
{
	bool isVlm = (Index > 0);
	mVlmSettingsWidget->setVisible(isVlm);
	if (Index == 1) // Ollama / Local
	{
		mVlmEndpointInput->setText("http://localhost:11434/api/generate");
		mVlmModelInput->setText("qwen2-vl");
	}
	else if (Index == 2) // OpenAI / Cloud
	{
		mVlmEndpointInput->setText("https://api.openai.com/v1/chat/completions");
		mVlmModelInput->setText("gpt-4o");
	}
}

void lcAIAssistantWidget::dragEnterEvent(QDragEnterEvent* Event)
{
	if (Event->mimeData()->hasUrls() || Event->mimeData()->hasImage())
		Event->acceptProposedAction();
}

void lcAIAssistantWidget::dropEvent(QDropEvent* Event)
{
	const QMimeData* Mime = Event->mimeData();
	if (Mime->hasUrls())
	{
		QList<QUrl> Urls = Mime->urls();
		for (const QUrl& Url : Urls)
		{
			QString LocalPath = Url.toLocalFile();
			if (!LocalPath.isEmpty())
			{
				QString Suffix = QFileInfo(LocalPath).suffix().toLower();
				if (Suffix == "png" || Suffix == "jpg" || Suffix == "jpeg" || Suffix == "webp" || Suffix == "bmp")
				{
					SetAttachedImage(LocalPath);
					Event->acceptProposedAction();
					return;
				}
			}
		}
	}
	else if (Mime->hasImage())
	{
		QImage Img = qvariant_cast<QImage>(Mime->imageData());
		if (!Img.isNull())
		{
			mAttachedImage = Img;
			mAttachedImagePath = "pasted_image.png";
			mThumbLabel->setPixmap(QPixmap::fromImage(Img.scaled(48, 48, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
			mImageNameLabel->setText(QString("Pasted Image (%1x%2)").arg(Img.width()).arg(Img.height()));
			mAttachmentBar->setVisible(true);
			mStatusLabel->setText(tr("Reference image pasted."));
			Event->acceptProposedAction();
		}
	}
}

void lcAIAssistantWidget::SetAttachedImage(const QString& Path)
{
	QImage Img(Path);
	if (Img.isNull()) return;

	mAttachedImagePath = Path;
	mAttachedImage = Img;
	QFileInfo Fi(Path);
	mImageNameLabel->setText(QString("%1 (%2x%3)").arg(Fi.fileName()).arg(Img.width()).arg(Img.height()));
	mThumbLabel->setPixmap(QPixmap::fromImage(Img.scaled(48, 48, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
	mAttachmentBar->setVisible(true);
	mStatusLabel->setText(tr("Attached reference image: %1").arg(Fi.fileName()));
}

void lcAIAssistantWidget::ClearAttachedImage()
{
	mAttachedImagePath.clear();
	mAttachedImage = QImage();
	mAttachmentBar->setVisible(false);
}

void lcAIAssistantWidget::OnAttachImageClicked()
{
	QString FilePath = QFileDialog::getOpenFileName(
		this,
		tr("Attach Reference Image"),
		QString(),
		tr("Images (*.png *.jpg *.jpeg *.webp *.bmp)")
	);
	if (!FilePath.isEmpty())
		SetAttachedImage(FilePath);
}

void lcAIAssistantWidget::OnRemoveImageClicked()
{
	ClearAttachedImage();
	mStatusLabel->setText(tr("Reference image removed."));
}

void lcAIAssistantWidget::OnPiecePresetChanged(int Index)
{
	int Pieces = mPiecePresetCombo->itemData(Index).toInt();
	if (Pieces >= 0)
	{
		mPieceCountSpin->blockSignals(true);
		mPieceCountSpin->setValue(Pieces);
		mPieceCountSpin->blockSignals(false);
	}
}

void lcAIAssistantWidget::AppendSystemMessage(const QString& Text)
{
	mChatBrowser->append(
		QString("<div style='background-color: #262933; border: 1px solid #3c4252; padding: 8px; border-radius: 6px; margin: 4px 0;'>"
		        "<span style='color: #4a90e2;'>🤖 <b>Agent:</b></span> %1</div>").arg(Text)
	);
}

void lcAIAssistantWidget::AppendUserMessage(const QString& Text, const QString& ImagePath, int TargetPieces)
{
	QString Html = "<div style='background-color: #2b394d; border-left: 4px solid #357abd; padding: 8px; border-radius: 6px; margin: 6px 0; text-align: left;'>";
	Html += "<div style='color: #79b8ff; font-weight: bold;'>👤 You:</div>";

	if (!mAttachedImage.isNull())
	{
		QByteArray ByteArray;
		QBuffer Buffer(&ByteArray);
		Buffer.open(QIODevice::WriteOnly);
		mAttachedImage.scaled(180, 180, Qt::KeepAspectRatio, Qt::SmoothTransformation).save(&Buffer, "PNG");
		QString Base64 = QString::fromLatin1(ByteArray.toBase64().data());
		Html += QString("<div style='margin: 6px 0;'><img src='data:image/png;base64,%1' style='border-radius: 6px; border: 1px solid #4a90e2; max-height: 120px;' /></div>").arg(Base64);
	}

	if (!Text.isEmpty())
		Html += QString("<div style='color: #e0e0e0; margin-top: 4px;'>%1</div>").arg(Text.toHtmlEscaped());

	if (TargetPieces > 0)
	{
		Html += QString("<div style='margin-top: 6px;'><span style='background-color: #1e3a5f; color: #a5d6ff; border: 1px solid #2f5a8a; padding: 2px 6px; border-radius: 4px; font-size: 10px; font-weight: bold;'>🎯 Budget: ~%1 pieces</span></div>").arg(TargetPieces);
	}

	Html += "</div>";
	mChatBrowser->append(Html);
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
	if (Prompt.isEmpty() && mAttachedImagePath.isEmpty() && mAttachedImage.isNull())
		return;
	if (Prompt.isEmpty() && (!mAttachedImagePath.isEmpty() || !mAttachedImage.isNull()))
		Prompt = "Build this reference model";
	BuildPrompt(Prompt);
}

void lcAIAssistantWidget::BuildPrompt(const QString& Prompt)
{
	if (Prompt.isEmpty()) return;

	mAnimTimer.stop();

	int TargetPieces = mPieceCountSpin->value();
	AppendUserMessage(Prompt, mAttachedImagePath, TargetPieces);

	mStatusLabel->setText(tr("Synthesizing novel 3D LEGO model..."));
	mProgressBar->setValue(10);
	mSendButton->setEnabled(false);
	mStopButton->setEnabled(true);

	lcAIOptions Opts;
	Opts.ReplaceCurrent = mModeCombo->currentData().toBool();
	Opts.StepByStep = true;
	Opts.TargetPieceCount = TargetPieces;
	Opts.ImagePath = mAttachedImagePath;
	Opts.AttachedImage = mAttachedImage;
	Opts.Backend = static_cast<lcAIBackend>(mBackendCombo->currentData().toInt());
	Opts.VlmEndpoint = mVlmEndpointInput->text().trimmed();
	Opts.VlmModel = mVlmModelInput->text().trimmed();

	// Reset input fields
	mPromptInput->clear();
	ClearAttachedImage();

	lcAIModelResult Res = lcAIGenerator::GenerateModel(Prompt, Opts);

	if (!Res.Success)
	{
		AppendSystemMessage(tr("❌ Sorry, I encountered an error synthesizing this model: %1").arg(Res.ErrorMessage));
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
