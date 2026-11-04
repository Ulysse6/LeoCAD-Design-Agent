#pragma once

#include <QString>
#include <QByteArray>
#include <QMap>
#include <vector>

enum class lcAITheme
{
	Auto,
	Classic,
	Modern,
	Monochrome,
	Neon,
	Earth,
	Pastel
};

enum class lcAISize
{
	Mini,
	Standard,
	Large
};

struct lcAIOptions
{
	lcAITheme Theme = lcAITheme::Auto;
	lcAISize Size = lcAISize::Standard;
	bool StepByStep = true;
	bool ReplaceCurrent = true;
	QString CustomApiKey;
	QString ApiEndpoint;
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

private:
	// Knowledge base builder
	static lcAIModelResult GenerateFromKnowledgeBase(const lcKBModelEntry* Entry, const QString& Prompt, const lcAIOptions& Options);

	// Domain builders
	static lcAIModelResult BuildHouse(const QString& Prompt, const lcAIOptions& Options);
	static lcAIModelResult BuildCar(const QString& Prompt, const lcAIOptions& Options);
	static lcAIModelResult BuildRocket(const QString& Prompt, const lcAIOptions& Options);
	static lcAIModelResult BuildSpaceship(const QString& Prompt, const lcAIOptions& Options);
	static lcAIModelResult BuildCastleTower(const QString& Prompt, const lcAIOptions& Options);
	static lcAIModelResult BuildPyramid(const QString& Prompt, const lcAIOptions& Options);
	static lcAIModelResult BuildTree(const QString& Prompt, const lcAIOptions& Options);
	static lcAIModelResult BuildBoat(const QString& Prompt, const lcAIOptions& Options);
	static lcAIModelResult BuildRobot(const QString& Prompt, const lcAIOptions& Options);
	static lcAIModelResult BuildAirplane(const QString& Prompt, const lcAIOptions& Options);
	static lcAIModelResult BuildFurniture(const QString& Prompt, const lcAIOptions& Options);
	static lcAIModelResult BuildBridge(const QString& Prompt, const lcAIOptions& Options);
	static lcAIModelResult BuildRainbowArch(const QString& Prompt, const lcAIOptions& Options);
	static lcAIModelResult BuildCustomSculpture(const QString& Prompt, const lcAIOptions& Options);

	// Helpers
	static void AddPiece(std::vector<lcAIModelPiece>& Pieces, int ColorCode, float X, float Y, float Z,
	                     const float Rot[9], const QString& PartId, int Step);
	static void AddPieceRotY(std::vector<lcAIModelPiece>& Pieces, int ColorCode, float X, float Y, float Z,
	                         float AngleDeg, const QString& PartId, int Step);
	static QByteArray SerializeLDraw(const QString& Title, const std::vector<lcAIModelPiece>& Pieces, bool IncludeSteps);
	static int ResolveColor(const QString& Prompt, int DefaultColor);
	static QString GetPartDescription(const QString& PartId);
};
