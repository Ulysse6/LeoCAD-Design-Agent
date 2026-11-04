#pragma once

#include <QString>
#include <QByteArray>
#include <QImage>
#include <QMap>
#include <vector>
#include <cstdint>

enum class lcAIBackend
{
	NativeSynthesizer, // 3D Voxel and Parametric Structural Synthesizer (100% offline, novel builds)
	NeuralVLM          // Vision-Language Model via Ollama, local vLLM, or OpenAI/Claude API
};

enum class lcAITheme
{
	Auto,
	Classic,
	Modern,
	Monochrome,
	Neon,
	Earth,
	Pastel,
	Custom
};

enum class lcAISize
{
	Mini,
	Standard,
	Large
};

struct lcAIOptions
{
	lcAIBackend Backend = lcAIBackend::NativeSynthesizer;
	lcAITheme Theme = lcAITheme::Auto;
	lcAISize Size = lcAISize::Standard;
	bool StepByStep = true;
	bool ReplaceCurrent = true;
	int TargetPieceCount = 0; // 0 = Auto / Unconstrained
	QString ImagePath;
	QImage AttachedImage;
	QString VlmEndpoint = "http://localhost:11434/api/generate";
	QString VlmModel = "qwen2-vl";
	QString VlmApiKey;
};

struct lcAIModelPiece
{
	int ColorCode = 16;
	float X = 0.0f;
	float Y = 0.0f;
	float Z = 0.0f;
	// 3x3 rotation matrix
	float R[9] = { 1.0f, 0.0f, 0.0f,
	               0.0f, 1.0f, 0.0f,
	               0.0f, 0.0f, 1.0f };
	QString PartId;
	int Step = 1;
};

struct lcAIModelResult
{
	bool Success = false;
	QString Title;
	QString Explanation;
	int BrickCount = 0;
	int StepCount = 1;
	QByteArray LDrawContent;
	std::vector<lcAIModelPiece> Pieces;
	QMap<QString, int> PartCounts;
	QString ErrorMessage;
};

struct lcKBModelEntry;

class lcAIGenerator
{
public:
	static lcAIModelResult GenerateModel(const QString& Prompt, const lcAIOptions& Options);

	// Core Hybrid Engine:
	// 1. Native 3D Voxel & Parametric Structural Synthesizer
	static lcAIModelResult SynthesizeFromImage(const QImage& Image, const QString& Prompt, const lcAIOptions& Options);
	static lcAIModelResult SynthesizeParametricModel(const QString& Category, const QString& Prompt, const lcAIOptions& Options);

	// 2. Neural Vision-Language Model Bridge (Ollama / Local / OpenAI / Claude)
	static lcAIModelResult CallNeuralVLM(const QString& Prompt, const QImage& Image, const lcAIOptions& Options);

	// Helpers
	static uint64_t ComputeVisualHash(const QImage& Image);
	static int ExtractDominantColor(const QImage& Image);
	static int ExtractTargetPieces(const QString& Prompt, int OptionPieces = 0);
	static lcAIModelResult ParseLDrawString(const QString& Title, const QString& LDrawCode, int StepCount = 1);

	static void AddPiece(std::vector<lcAIModelPiece>& Pieces, int ColorCode, float X, float Y, float Z,
	                     const float Rot[9], const QString& PartId, int Step);
	static void AddPieceRotY(std::vector<lcAIModelPiece>& Pieces, int ColorCode, float X, float Y, float Z,
	                         float AngleDeg, const QString& PartId, int Step);
	static QByteArray SerializeLDraw(const QString& Title, const std::vector<lcAIModelPiece>& Pieces, bool IncludeSteps);
	static int ResolveColor(const QString& Prompt, int DefaultColor);
	static QString GetPartDescription(const QString& PartId);

private:
	// Knowledge base builder (used when specific reference models are requested)
	static lcAIModelResult GenerateFromKnowledgeBase(const lcKBModelEntry* Entry, const QString& Prompt, const lcAIOptions& Options);
};
