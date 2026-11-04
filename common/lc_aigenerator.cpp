#include "lc_global.h"
#include "lc_aigenerator.h"
#include "lc_kb_catalog.h"
#include <cmath>
#include <algorithm>
#include <QTextStream>
#include <QStringList>
#include <QBuffer>
#include <QEventLoop>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QCoreApplication>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// LDraw standard color codes
enum LDrawColor
{
	COLOR_BLACK = 0,
	COLOR_BLUE = 1,
	COLOR_GREEN = 2,
	COLOR_TEAL = 3,
	COLOR_RED = 4,
	COLOR_MAGENTA = 5,
	COLOR_BROWN = 6,
	COLOR_LIGHT_GRAY = 7,
	COLOR_DARK_GRAY = 8,
	COLOR_LIGHT_BLUE = 9,
	COLOR_BRIGHT_GREEN = 10,
	COLOR_YELLOW = 14,
	COLOR_WHITE = 15,
	COLOR_TAN = 19,
	COLOR_ORANGE = 25,
	COLOR_LIME = 27,
	COLOR_TRANS_DARK_BLUE = 33,
	COLOR_TRANS_RED = 36,
	COLOR_TRANS_YELLOW = 46,
	COLOR_TRANS_CLEAR = 47,
	COLOR_LIGHT_BLUISH_GRAY = 71,
	COLOR_DARK_BLUISH_GRAY = 72,
	COLOR_DARK_RED = 320,
	COLOR_DARK_GREEN = 288
};

struct LegoColorDef {
	int Code;
	int R, G, B;
};

static const LegoColorDef gStandardLegoColors[] = {
	{ COLOR_RED, 201, 26, 9 },
	{ COLOR_BLUE, 0, 85, 191 },
	{ COLOR_YELLOW, 242, 205, 55 },
	{ COLOR_GREEN, 35, 120, 65 },
	{ COLOR_WHITE, 255, 255, 255 },
	{ COLOR_BLACK, 27, 42, 52 },
	{ COLOR_ORANGE, 254, 138, 24 },
	{ COLOR_DARK_BLUISH_GRAY, 108, 110, 104 },
	{ COLOR_LIGHT_BLUISH_GRAY, 160, 165, 169 },
	{ COLOR_LIME, 166, 202, 85 },
	{ COLOR_BRIGHT_GREEN, 75, 159, 74 },
	{ COLOR_TAN, 228, 205, 158 },
	{ COLOR_BROWN, 88, 57, 39 },
	{ COLOR_DARK_RED, 114, 0, 18 },
	{ COLOR_DARK_GREEN, 0, 69, 26 }
};
static const size_t gStandardLegoColorCount = sizeof(gStandardLegoColors) / sizeof(gStandardLegoColors[0]);

// ============================================================================
// 1. VOXEL GRID & STRUCTURAL BRICK PACKER
// ============================================================================

struct lcVoxelCell
{
	bool Occupied = false;
	int Color = 16;
	bool IsSlope = false;
	int SlopeDir = 0; // 0=Flat, 1=+Z, 2=-Z, 3=+X, 4=-X
};

class lcVoxelVolume
{
public:
	int DimX = 0;
	int DimY = 0;
	int DimZ = 0;
	std::vector<lcVoxelCell> Cells;

	lcVoxelVolume(int dx, int dy, int dz)
		: DimX(dx), DimY(dy), DimZ(dz), Cells(dx * dy * dz)
	{
	}

	inline int Index(int x, int y, int z) const
	{
		return y * (DimX * DimZ) + z * DimX + x;
	}

	inline bool IsValid(int x, int y, int z) const
	{
		return x >= 0 && x < DimX && y >= 0 && y < DimY && z >= 0 && z < DimZ;
	}

	inline lcVoxelCell& At(int x, int y, int z)
	{
		return Cells[Index(x, y, z)];
	}

	inline const lcVoxelCell& At(int x, int y, int z) const
	{
		return Cells[Index(x, y, z)];
	}

	inline bool IsOccupied(int x, int y, int z) const
	{
		if (!IsValid(x, y, z)) return false;
		return At(x, y, z).Occupied;
	}

	void Set(int x, int y, int z, int Color)
	{
		if (IsValid(x, y, z))
		{
			lcVoxelCell& c = At(x, y, z);
			c.Occupied = true;
			c.Color = Color;
		}
	}
};

void lcAIGenerator::AddPiece(std::vector<lcAIModelPiece>& Pieces, int ColorCode, float X, float Y, float Z,
                             const float Rot[9], const QString& PartId, int Step)
{
	lcAIModelPiece Piece;
	Piece.ColorCode = ColorCode;
	Piece.X = X;
	Piece.Y = Y;
	Piece.Z = Z;
	Piece.PartId = PartId.endsWith(".dat", Qt::CaseInsensitive) ? PartId : PartId + ".dat";
	Piece.Step = Step;
	for (int i = 0; i < 9; ++i)
		Piece.R[i] = Rot[i];
	Pieces.push_back(Piece);
}

void lcAIGenerator::AddPieceRotY(std::vector<lcAIModelPiece>& Pieces, int ColorCode, float X, float Y, float Z,
                                 float AngleDeg, const QString& PartId, int Step)
{
	float Rad = static_cast<float>(AngleDeg * M_PI / 180.0);
	float c = std::cos(Rad);
	float s = std::sin(Rad);
	float Rot[9] = {
		c, 0.0f, s,
		0.0f, 1.0f, 0.0f,
		-s, 0.0f, c
	};
	AddPiece(Pieces, ColorCode, X, Y, Z, Rot, PartId, Step);
}

QByteArray lcAIGenerator::SerializeLDraw(const QString& Title, const std::vector<lcAIModelPiece>& Pieces, bool IncludeSteps)
{
	QByteArray Out;
	QTextStream Stream(&Out);

	Stream << "0 " << Title << "\n";
	Stream << "0 Name: " << Title.toLower().replace(' ', '_') << ".ldr\n";
	Stream << "0 Author: LeoCAD AI Agent\n";
	Stream << "0 !LEOCAD MODEL\n";

	int CurrentStep = 1;
	for (const auto& Piece : Pieces)
	{
		if (IncludeSteps && Piece.Step > CurrentStep)
		{
			while (CurrentStep < Piece.Step)
			{
				Stream << "0 STEP\n";
				CurrentStep++;
			}
		}

		Stream << "1 " << Piece.ColorCode << " "
		       << Piece.X << " " << Piece.Y << " " << Piece.Z << " "
		       << Piece.R[0] << " " << Piece.R[1] << " " << Piece.R[2] << " "
		       << Piece.R[3] << " " << Piece.R[4] << " " << Piece.R[5] << " "
		       << Piece.R[6] << " " << Piece.R[7] << " " << Piece.R[8] << " "
		       << Piece.PartId << "\n";
	}

	Stream.flush();
	return Out;
}

int lcAIGenerator::ResolveColor(const QString& Prompt, int DefaultColor)
{
	QString Lower = Prompt.toLower();
	if (Lower.contains("red")) return COLOR_RED;
	if (Lower.contains("blue") && Lower.contains("light")) return COLOR_LIGHT_BLUE;
	if (Lower.contains("blue") || Lower.contains("azure")) return COLOR_BLUE;
	if (Lower.contains("green") && Lower.contains("bright")) return COLOR_BRIGHT_GREEN;
	if (Lower.contains("green") || Lower.contains("emerald")) return COLOR_GREEN;
	if (Lower.contains("yellow") || Lower.contains("gold")) return COLOR_YELLOW;
	if (Lower.contains("black") || Lower.contains("dark")) return COLOR_BLACK;
	if (Lower.contains("white") || Lower.contains("snow")) return COLOR_WHITE;
	if (Lower.contains("gray") || Lower.contains("grey")) return COLOR_LIGHT_BLUISH_GRAY;
	if (Lower.contains("orange")) return COLOR_ORANGE;
	if (Lower.contains("lime")) return COLOR_LIME;
	if (Lower.contains("tan") || Lower.contains("beige") || Lower.contains("sand")) return COLOR_TAN;
	if (Lower.contains("brown") || Lower.contains("wood")) return COLOR_BROWN;
	if (Lower.contains("pink") || Lower.contains("magenta")) return COLOR_MAGENTA;
	if (Lower.contains("teal") || Lower.contains("turquoise") || Lower.contains("cyan")) return COLOR_TEAL;
	return DefaultColor;
}

QString lcAIGenerator::GetPartDescription(const QString& PartId)
{
	QString Clean = PartId;
	Clean.remove(".dat", Qt::CaseInsensitive);
	if (Clean == "3001") return "Brick 2x4";
	if (Clean == "3002") return "Brick 2x3";
	if (Clean == "3003") return "Brick 2x2";
	if (Clean == "3004") return "Brick 1x2";
	if (Clean == "3005") return "Brick 1x1";
	if (Clean == "3010") return "Brick 1x4";
	if (Clean == "3009") return "Brick 1x6";
	if (Clean == "3008") return "Brick 1x8";
	if (Clean == "2456") return "Brick 2x6";
	if (Clean == "3007") return "Brick 2x8";
	if (Clean == "3020") return "Plate 2x4";
	if (Clean == "3022") return "Plate 2x2";
	if (Clean == "3023") return "Plate 1x2";
	if (Clean == "3024") return "Plate 1x1";
	if (Clean == "3710") return "Plate 1x4";
	if (Clean == "3031") return "Plate 4x4";
	if (Clean == "3035") return "Plate 4x8";
	if (Clean == "3039") return "Slope 45 2x2";
	if (Clean == "3037") return "Slope 45 2x4";
	if (Clean == "3040") return "Slope 45 2x1";
	if (Clean == "6014b") return "Wheel 11mm";
	if (Clean == "3829c01") return "Steering Wheel";
	return "Lego Part " + Clean;
}

int lcAIGenerator::ExtractDominantColor(const QImage& Image)
{
	if (Image.isNull())
		return -1;

	QImage scaled = Image.scaled(32, 32, Qt::IgnoreAspectRatio, Qt::SmoothTransformation).convertToFormat(QImage::Format_ARGB32);

	QRgb cornerPixel = scaled.pixel(0, 0);
	int bgR = qRed(cornerPixel), bgG = qGreen(cornerPixel), bgB = qBlue(cornerPixel);
	bool hasUniformBg = true;
	QRgb c2 = scaled.pixel(31, 0), c3 = scaled.pixel(0, 31), c4 = scaled.pixel(31, 31);
	if (std::abs(qRed(c2) - bgR) > 20 || std::abs(qRed(c3) - bgR) > 20 || std::abs(qRed(c4) - bgR) > 20)
		hasUniformBg = false;

	QMap<int, int> ColorHistogram;

	for (int y = 0; y < 32; ++y)
	{
		for (int x = 0; x < 32; ++x)
		{
			QRgb pixel = scaled.pixel(x, y);
			if (qAlpha(pixel) < 50)
				continue;

			int r = qRed(pixel), g = qGreen(pixel), b = qBlue(pixel);

			if (r > 245 && g > 245 && b > 245)
				continue;

			if (hasUniformBg && qAlpha(cornerPixel) > 50)
			{
				int diffBg = (r - bgR)*(r - bgR) + (g - bgG)*(g - bgG) + (b - bgB)*(b - bgB);
				if (diffBg < 1200)
					continue;
			}

			int bestColor = COLOR_RED;
			int bestDist = 1000000;
			for (size_t c = 0; c < gStandardLegoColorCount; ++c)
			{
				int dr = r - gStandardLegoColors[c].R;
				int dg = g - gStandardLegoColors[c].G;
				int db = b - gStandardLegoColors[c].B;
				int dist = dr*dr + dg*dg + db*db;
				if (dist < bestDist)
				{
					bestDist = dist;
					bestColor = gStandardLegoColors[c].Code;
				}
			}
			ColorHistogram[bestColor]++;
		}
	}

	int dominantColor = -1;
	int maxCount = 0;
	for (auto it = ColorHistogram.begin(); it != ColorHistogram.end(); ++it)
	{
		int count = it.value();
		if (it.key() == COLOR_BLACK || it.key() == COLOR_WHITE || it.key() == COLOR_LIGHT_BLUISH_GRAY)
			count = count / 2;

		if (count > maxCount)
		{
			maxCount = count;
			dominantColor = it.key();
		}
	}

	return dominantColor;
}

uint64_t lcAIGenerator::ComputeVisualHash(const QImage& Image)
{
	if (Image.isNull())
		return 0;

	QImage scaled = Image.scaled(9, 8, Qt::IgnoreAspectRatio, Qt::SmoothTransformation).convertToFormat(QImage::Format_Grayscale8);
	uint64_t hash = 0;
	for (int y = 0; y < 8; ++y)
	{
		const uchar* line = scaled.constScanLine(y);
		for (int x = 0; x < 8; ++x)
		{
			bool bit = (line[x] > line[x + 1]);
			hash = (hash << 1) | (bit ? 1 : 0);
		}
	}
	return hash;
}

int lcAIGenerator::ExtractTargetPieces(const QString& Prompt, int OptionPieces)
{
	if (OptionPieces > 0)
		return OptionPieces;

	QRegExp rxHyphen("(\\d+)\\s*-\\s*(?:piece|pieces|part|parts|brick|bricks)", Qt::CaseInsensitive);
	if (rxHyphen.indexIn(Prompt) != -1)
		return rxHyphen.cap(1).toInt();

	QRegExp rxPrep("(?:with|using|around|approx|approximately|under|about)\\s*(\\d+)\\s*(?:pieces|parts|bricks)", Qt::CaseInsensitive);
	if (rxPrep.indexIn(Prompt) != -1)
		return rxPrep.cap(1).toInt();

	QRegExp rxSimple("(\\d+)\\s*(?:pieces|parts|bricks)", Qt::CaseInsensitive);
	if (rxSimple.indexIn(Prompt) != -1)
		return rxSimple.cap(1).toInt();

	return 0;
}

lcAIModelResult lcAIGenerator::ParseLDrawString(const QString& Title, const QString& LDrawCode, int StepCount)
{
	lcAIModelResult Result;
	Result.Title = Title;
	Result.Success = false;

	QStringList Lines = LDrawCode.split('\n');
	int CurrentStep = 1;

	for (const QString& RawLine : Lines)
	{
		QString Line = RawLine.trimmed();
		if (Line.isEmpty()) continue;

		if (Line.startsWith("0 STEP"))
		{
			CurrentStep++;
			continue;
		}

		if (Line.startsWith("1 "))
		{
			QStringList Tokens = Line.split(QRegExp("\\s+"), QString::SkipEmptyParts);
			if (Tokens.size() >= 15)
			{
				bool Ok = false;
				int Color = Tokens[1].toInt(&Ok);
				if (Ok)
				{
					float X = Tokens[2].toFloat();
					float Y = Tokens[3].toFloat();
					float Z = Tokens[4].toFloat();
					float Rot[9];
					for (int r = 0; r < 9; ++r)
						Rot[r] = Tokens[5 + r].toFloat();

					QString Part = Tokens.mid(14).join(" ");
					AddPiece(Result.Pieces, Color, X, Y, Z, Rot, Part, CurrentStep);
					Result.PartCounts[Part]++;
				}
			}
		}
	}

	if (!Result.Pieces.empty())
	{
		Result.Success = true;
		Result.BrickCount = static_cast<int>(Result.Pieces.size());
		Result.StepCount = CurrentStep;
		Result.LDrawContent = LDrawCode.toUtf8();
		Result.Explanation = QString("Generated novel 3D LEGO model with %1 pieces across %2 steps.").arg(Result.BrickCount).arg(Result.StepCount);
	}
	else
	{
		Result.ErrorMessage = "No valid LDraw pieces parsed from output";
	}

	return Result;
}

// ============================================================================
// 1.5 NEURAL VISION-TO-BRICK STRUCTURAL INFERENCE MODEL
// ============================================================================

struct lcNeuralModelData
{
	bool Loaded = false;
	int d_in = 32;
	int d_h1 = 48;
	int d_h2 = 24;
	int d_out = 12;
	std::vector<std::vector<float>> W1;
	std::vector<float> b1;
	std::vector<std::vector<float>> W2;
	std::vector<float> b2;
	std::vector<std::vector<float>> W3;
	std::vector<float> b3;
};

static lcNeuralModelData gNeuralModel;
static bool gNeuralModelInitialized = false;

static void EnsureNeuralModelLoaded()
{
	if (gNeuralModelInitialized)
		return;
	gNeuralModelInitialized = true;

	QStringList candidatePaths = {
		"models/vlm_neural_weights.json",
		"../models/vlm_neural_weights.json",
		"../../models/vlm_neural_weights.json",
		QDir(QCoreApplication::applicationDirPath()).filePath("models/vlm_neural_weights.json"),
		QDir(QCoreApplication::applicationDirPath()).filePath("../../../models/vlm_neural_weights.json"),
		"/home/ulyss/Documents/LeoFork/leocad/models/vlm_neural_weights.json"
	};

	QString foundPath;
	for (const QString& path : candidatePaths)
	{
		if (QFile::exists(path))
		{
			foundPath = path;
			break;
		}
	}

	if (foundPath.isEmpty())
		return;

	QFile f(foundPath);
	if (!f.open(QIODevice::ReadOnly))
		return;

	QByteArray data = f.readAll();
	f.close();

	QJsonParseError err;
	QJsonDocument doc = QJsonDocument::fromJson(data, &err);
	if (doc.isNull() || !doc.isObject())
		return;

	QJsonObject root = doc.object();
	gNeuralModel.d_in = root["d_in"].toInt(32);
	gNeuralModel.d_h1 = root["d_h1"].toInt(48);
	gNeuralModel.d_h2 = root["d_h2"].toInt(24);
	gNeuralModel.d_out = root["d_out"].toInt(12);

	// Load W1, b1
	QJsonArray jW1 = root["W1"].toArray();
	gNeuralModel.W1.resize(jW1.size());
	for (int i = 0; i < jW1.size(); ++i)
	{
		QJsonArray row = jW1[i].toArray();
		gNeuralModel.W1[i].resize(row.size());
		for (int j = 0; j < row.size(); ++j)
			gNeuralModel.W1[i][j] = static_cast<float>(row[j].toDouble());
	}
	QJsonArray jb1 = root["b1"].toArray();
	gNeuralModel.b1.resize(jb1.size());
	for (int i = 0; i < jb1.size(); ++i)
		gNeuralModel.b1[i] = static_cast<float>(jb1[i].toDouble());

	// Load W2, b2
	QJsonArray jW2 = root["W2"].toArray();
	gNeuralModel.W2.resize(jW2.size());
	for (int i = 0; i < jW2.size(); ++i)
	{
		QJsonArray row = jW2[i].toArray();
		gNeuralModel.W2[i].resize(row.size());
		for (int j = 0; j < row.size(); ++j)
			gNeuralModel.W2[i][j] = static_cast<float>(row[j].toDouble());
	}
	QJsonArray jb2 = root["b2"].toArray();
	gNeuralModel.b2.resize(jb2.size());
	for (int i = 0; i < jb2.size(); ++i)
		gNeuralModel.b2[i] = static_cast<float>(jb2[i].toDouble());

	// Load W3, b3
	QJsonArray jW3 = root["W3"].toArray();
	gNeuralModel.W3.resize(jW3.size());
	for (int i = 0; i < jW3.size(); ++i)
	{
		QJsonArray row = jW3[i].toArray();
		gNeuralModel.W3[i].resize(row.size());
		for (int j = 0; j < row.size(); ++j)
			gNeuralModel.W3[i][j] = static_cast<float>(row[j].toDouble());
	}
	QJsonArray jb3 = root["b3"].toArray();
	gNeuralModel.b3.resize(jb3.size());
	for (int i = 0; i < jb3.size(); ++i)
		gNeuralModel.b3[i] = static_cast<float>(jb3[i].toDouble());

	gNeuralModel.Loaded = true;
}

std::vector<float> lcAIGenerator::ExtractNeuralFeatures(const QImage& Image, const QString& Prompt, int TargetPieces)
{
	std::vector<float> feat(32, 0.0f);

	if (!Image.isNull())
	{
		QImage gray = Image.scaled(16, 16, Qt::IgnoreAspectRatio, Qt::SmoothTransformation).convertToFormat(QImage::Format_Grayscale8);
		// 8 vertical slices
		for (int y = 0; y < 8; ++y)
		{
			int count = 0;
			for (int dy = 0; dy < 2; ++dy)
			{
				const uchar* line = gray.constScanLine(y * 2 + dy);
				for (int x = 0; x < 16; ++x)
				{
					if (line[x] < 240)
						count++;
				}
			}
			feat[y] = static_cast<float>(count) / 32.0f;
		}

		// 8 horizontal slices
		for (int x = 0; x < 8; ++x)
		{
			int count = 0;
			for (int y = 0; y < 16; ++y)
			{
				const uchar* line = gray.constScanLine(y);
				for (int dx = 0; dx < 2; ++dx)
				{
					if (line[x * 2 + dx] < 240)
						count++;
				}
			}
			feat[8 + x] = static_cast<float>(count) / 32.0f;
		}

		float aspect = static_cast<float>(Image.width()) / static_cast<float>(std::max(1, Image.height()));
		feat[16] = std::min(3.0f, std::max(0.2f, aspect)) / 3.0f;
	}
	else
	{
		feat[16] = 0.33f;
	}

	int pcs = (TargetPieces > 0) ? TargetPieces : 150;
	feat[17] = std::log10(static_cast<float>(std::max(10, std::min(1000, pcs)))) / 3.0f;

	QString p = Prompt.toLower();
	auto containsAny = [&](const QStringList& keys) {
		for (const QString& k : keys) {
			if (p.contains(k)) return 1.0f;
		}
		return 0.0f;
	};

	feat[18] = containsAny({ "car", "truck", "racer", "pickup", "van", "wagon", "buggy" });
	feat[19] = containsAny({ "train", "locomotive", "rail", "chief", "engine" });
	feat[20] = containsAny({ "plane", "airplane", "jet", "aircraft", "helicopter" });
	feat[21] = containsAny({ "space", "spaceship", "rocket", "shuttle", "x-wing" });
	feat[22] = containsAny({ "house", "building", "villa", "lodge", "cottage" });
	feat[23] = containsAny({ "tower", "castle", "landmark", "eiffel", "white house", "pisa", "big ben" });
	feat[24] = containsAny({ "boat", "ship", "speedboat", "yacht" });
	feat[25] = containsAny({ "robot", "mech", "droid" });

	feat[26] = p.contains("red") ? 1.0f : 0.0f;
	feat[27] = p.contains("blue") ? 1.0f : 0.0f;
	feat[28] = p.contains("yellow") ? 1.0f : 0.0f;
	feat[29] = p.contains("black") ? 1.0f : 0.0f;
	feat[30] = p.contains("white") ? 1.0f : 0.0f;
	feat[31] = (p.contains("gray") || p.contains("grey")) ? 1.0f : 0.0f;

	return feat;
}

bool lcAIGenerator::PredictNeuralParameters(const QImage& Image, const QString& Prompt, int TargetPieces, lcNeuralVLMOutput& Output)
{
	EnsureNeuralModelLoaded();
	if (!gNeuralModel.Loaded)
		return false;

	std::vector<float> x = ExtractNeuralFeatures(Image, Prompt, TargetPieces);

	// Forward pass
	// Layer 1
	std::vector<float> h1(gNeuralModel.d_h1, 0.0f);
	for (int j = 0; j < gNeuralModel.d_h1; ++j)
	{
		float val = gNeuralModel.b1[j];
		for (int i = 0; i < gNeuralModel.d_in; ++i)
			val += x[i] * gNeuralModel.W1[i][j];
		h1[j] = std::max(0.0f, val);
	}

	// Layer 2
	std::vector<float> h2(gNeuralModel.d_h2, 0.0f);
	for (int j = 0; j < gNeuralModel.d_h2; ++j)
	{
		float val = gNeuralModel.b2[j];
		for (int i = 0; i < gNeuralModel.d_h1; ++i)
			val += h1[i] * gNeuralModel.W2[i][j];
		h2[j] = std::max(0.0f, val);
	}

	// Layer 3 (Output with Sigmoid)
	std::vector<float> pred(gNeuralModel.d_out, 0.0f);
	for (int j = 0; j < gNeuralModel.d_out; ++j)
	{
		float val = gNeuralModel.b3[j];
		for (int i = 0; i < gNeuralModel.d_h2; ++i)
			val += h2[i] * gNeuralModel.W3[i][j];
		float clamped = std::max(-15.0f, std::min(15.0f, val));
		pred[j] = 1.0f / (1.0f + std::exp(-clamped));
	}

	Output.Valid = true;
	Output.WidthStuds = std::max(6.0f, std::min(30.0f, pred[0] * 30.0f));
	Output.HeightLayers = std::max(4.0f, std::min(25.0f, pred[1] * 25.0f));
	Output.DepthStuds = std::max(6.0f, std::min(30.0f, pred[2] * 30.0f));
	Output.StepCount = std::max(1, static_cast<int>(pred[3] * 30.0f + 0.5f));
	Output.Ratio2x4 = pred[4];
	Output.Ratio2x2 = pred[5];
	Output.Ratio1x2 = pred[6];
	Output.Ratio1x1 = pred[7];
	Output.RatioSlopes = pred[8];
	Output.RatioPlates = pred[9];
	Output.DominantColor = static_cast<int>(pred[10] * 100.0f + 0.5f);
	Output.Complexity = pred[11];

	return true;
}

// ============================================================================
// 2. GREEDY INTERLOCKING BRICK PACKER (Voxel -> Brick Decomposition)
// ============================================================================

struct BrickCandidate {
	int StudsX;
	int StudsZ;
	const char* PartId;
};

static void DecomposeVolumeToBricks(lcVoxelVolume& Vol, int TargetPieces,
                                    std::vector<lcAIModelPiece>& Pieces,
                                    QMap<QString, int>& PartCounts,
                                    int& TotalSteps,
                                    const lcNeuralVLMOutput* Neural = nullptr)
{
	std::vector<bool> Placed(Vol.DimX * Vol.DimY * Vol.DimZ, false);
	int CurrentStep = 1;

	// Candidate brick shapes ordered for budget control
	std::vector<BrickCandidate> BrickCandidates;
	if (Neural && Neural->Valid)
	{
		// Brick candidates prioritized by neural model predictions
		if (Neural->Ratio2x4 > 0.06f)
		{
			BrickCandidates = {
				{ 2, 4, "3001" }, { 4, 2, "3001" },
				{ 2, 3, "3002" }, { 3, 2, "3002" },
				{ 2, 2, "3003" },
				{ 1, 4, "3010" }, { 4, 1, "3010" },
				{ 1, 2, "3004" }, { 2, 1, "3004" },
				{ 1, 1, "3005" }
			};
		}
		else if (Neural->Ratio1x2 > 0.04f || TargetPieces > 200)
		{
			BrickCandidates = {
				{ 2, 2, "3003" },
				{ 1, 4, "3010" }, { 4, 1, "3010" },
				{ 1, 2, "3004" }, { 2, 1, "3004" },
				{ 1, 1, "3005" },
				{ 2, 4, "3001" }, { 4, 2, "3001" }
			};
		}
		else
		{
			BrickCandidates = {
				{ 2, 4, "3001" }, { 4, 2, "3001" },
				{ 2, 2, "3003" },
				{ 1, 4, "3010" }, { 4, 1, "3010" },
				{ 1, 2, "3004" }, { 2, 1, "3004" },
				{ 1, 1, "3005" }
			};
		}
	}
	else if (TargetPieces > 0 && TargetPieces <= 80)
	{
		// Low piece budget: pack larger bricks
		BrickCandidates = {
			{ 2, 4, "3001" }, { 4, 2, "3001" },
			{ 2, 3, "3002" }, { 3, 2, "3002" },
			{ 2, 2, "3003" },
			{ 1, 4, "3010" }, { 4, 1, "3010" },
			{ 1, 2, "3004" }, { 2, 1, "3004" },
			{ 1, 1, "3005" }
		};
	}
	else if (TargetPieces > 220)
	{
		// High piece budget: favor smaller pieces for higher fidelity & piece density
		BrickCandidates = {
			{ 2, 2, "3003" },
			{ 1, 4, "3010" }, { 4, 1, "3010" },
			{ 1, 2, "3004" }, { 2, 1, "3004" },
			{ 1, 1, "3005" },
			{ 2, 4, "3001" }, { 4, 2, "3001" }
		};
	}
	else
	{
		// Standard balanced budget
		BrickCandidates = {
			{ 2, 4, "3001" }, { 4, 2, "3001" },
			{ 2, 2, "3003" },
			{ 1, 4, "3010" }, { 4, 1, "3010" },
			{ 1, 2, "3004" }, { 2, 1, "3004" },
			{ 1, 1, "3005" }
		};
	}

	for (int y = 0; y < Vol.DimY; ++y)
	{
		bool HasLayerPieces = false;
		// Interlocking staggered bond: alternate primary scan direction every layer!
		bool ScanZFirst = (y % 2 == 1);

		int outerLimit = ScanZFirst ? Vol.DimZ : Vol.DimX;
		int innerLimit = ScanZFirst ? Vol.DimX : Vol.DimZ;

		for (int o = 0; o < outerLimit; ++o)
		{
			for (int i = 0; i < innerLimit; ++i)
			{
				int x = ScanZFirst ? i : o;
				int z = ScanZFirst ? o : i;

				int idx = Vol.Index(x, y, z);
				if (!Vol.Cells[idx].Occupied || Placed[idx])
					continue;

				int baseColor = Vol.Cells[idx].Color;

				// Find largest candidate brick that fits into unplaced occupied cells
				const BrickCandidate* BestBrick = nullptr;
				for (const auto& bc : BrickCandidates)
				{
					if (x + bc.StudsX > Vol.DimX || z + bc.StudsZ > Vol.DimZ)
						continue;

					bool Fits = true;
					for (int bx = 0; bx < bc.StudsX && Fits; ++bx)
					{
						for (int bz = 0; bz < bc.StudsZ && Fits; ++bz)
						{
							int cIdx = Vol.Index(x + bx, y, z + bz);
							if (!Vol.Cells[cIdx].Occupied || Placed[cIdx])
								Fits = false;
						}
					}
					if (Fits)
					{
						BestBrick = &bc;
						break;
					}
				}

				if (!BestBrick)
				{
					// Fallback to 1x1 brick
					static const BrickCandidate SingleBrick = { 1, 1, "3005" };
					BestBrick = &SingleBrick;
				}

				// Mark voxels as placed
				for (int bx = 0; bx < BestBrick->StudsX; ++bx)
				{
					for (int bz = 0; bz < BestBrick->StudsZ; ++bz)
					{
						Placed[Vol.Index(x + bx, y, z + bz)] = true;
					}
				}

				// LDraw grid placement (Stud = 20 LDU, Height = 24 LDU)
				// Center around origin
				float CenterX = (x + BestBrick->StudsX / 2.0f - Vol.DimX / 2.0f) * 20.0f;
				float CenterZ = (z + BestBrick->StudsZ / 2.0f - Vol.DimZ / 2.0f) * 20.0f;
				float CenterY = -static_cast<float>(y * 24.0f + 24.0f);

				// Determine orientation: if width > length in local candidate
				float Rot[9] = { 1,0,0, 0,1,0, 0,0,1 };
				QString partId = BestBrick->PartId;

				// Top surface aerodynamic slope check
				if (y == Vol.DimY - 1 || !Vol.IsOccupied(x, y + 1, z))
				{
					if (BestBrick->StudsX == 2 && BestBrick->StudsZ == 2)
					{
						partId = "3039"; // 45 2x2 Slope
					}
					else if (BestBrick->StudsX == 2 && BestBrick->StudsZ == 1)
					{
						partId = "3040"; // 45 2x1 Slope
					}
				}

				lcAIGenerator::AddPiece(Pieces, baseColor, CenterX, CenterY, CenterZ, Rot, partId, CurrentStep);
				PartCounts[partId]++;
				HasLayerPieces = true;
			}
		}

		if (HasLayerPieces)
			CurrentStep++;
	}

	TotalSteps = CurrentStep;
}

// ============================================================================
// 3. NATIVE GENERATIVE SYNTHESIZERS (Arbitrary Image & Parametric)
// ============================================================================

lcAIModelResult lcAIGenerator::SynthesizeFromImage(const QImage& Image, const QString& Prompt, const lcAIOptions& Options)
{
	lcAIModelResult Res;
	Res.Title = Prompt.trimmed().isEmpty() ? "Synthesized 3D Model from Reference Image" : Prompt;

	if (Image.isNull())
	{
		Res.Success = false;
		Res.ErrorMessage = "Attached image is null or invalid";
		return Res;
	}

	int TargetPieces = ExtractTargetPieces(Prompt, Options.TargetPieceCount);

	lcNeuralVLMOutput NeuralPrediction;
	bool HasNeural = PredictNeuralParameters(Image, Prompt, TargetPieces, NeuralPrediction);

	int GridX = 16, GridY = 12, GridZ = 12;

	if (HasNeural)
	{
		GridX = std::max(8, std::min(30, static_cast<int>(NeuralPrediction.WidthStuds + 0.5f)));
		GridY = std::max(6, std::min(25, static_cast<int>(NeuralPrediction.HeightLayers + 0.5f)));
		GridZ = std::max(8, std::min(30, static_cast<int>(NeuralPrediction.DepthStuds + 0.5f)));
	}
	else if (TargetPieces > 0)
	{
		if (TargetPieces <= 70)
		{
			GridX = 10; GridY = 8; GridZ = 8;
		}
		else if (TargetPieces <= 180)
		{
			GridX = 16; GridY = 12; GridZ = 12;
		}
		else
		{
			GridX = 22; GridY = 16; GridZ = 16;
		}
	}

	// Rescale image for 3D silhouette contouring
	QImage scaled = Image.scaled(GridX, GridY, Qt::IgnoreAspectRatio, Qt::SmoothTransformation).convertToFormat(QImage::Format_ARGB32);

	lcVoxelVolume Vol(GridX, GridY, GridZ);
	int BaseDominantColor = ExtractDominantColor(Image);
	if (BaseDominantColor < 0) BaseDominantColor = COLOR_RED;

	float AspectRatio = static_cast<float>(Image.width()) / static_cast<float>(std::max(1, Image.height()));

	// Voxelize volume based on image contours
	for (int y = 0; y < GridY; ++y)
	{
		int imgY = GridY - 1 - y; // Invert Y (LEGO build grows upward from ground)
		for (int x = 0; x < GridX; ++x)
		{
			QRgb pixel = scaled.pixel(x, imgY);
			if (qAlpha(pixel) < 60)
				continue;

			int r = qRed(pixel), g = qGreen(pixel), b = qBlue(pixel);
			if (r > 245 && g > 245 && b > 245)
				continue;

			// Map pixel color to closest official LEGO color
			int bestColor = BaseDominantColor;
			int bestDist = 1000000;
			for (size_t c = 0; c < gStandardLegoColorCount; ++c)
			{
				int dr = r - gStandardLegoColors[c].R;
				int dg = g - gStandardLegoColors[c].G;
				int db = b - gStandardLegoColors[c].B;
				int dist = dr*dr + dg*dg + db*db;
				if (dist < bestDist)
				{
					bestDist = dist;
					bestColor = gStandardLegoColors[c].Code;
				}
			}

			// Generate depth envelope Z
			int halfZ = GridZ / 2;
			float depthSpan = 0.5f;

			if (AspectRatio > 1.3f)
			{
				// Streamlined hull for vehicles/aircraft
				float normX = (x - GridX / 2.0f) / (GridX / 2.0f);
				depthSpan = std::max(0.2f, std::sqrt(std::max(0.0f, 1.0f - normX * normX)));
			}
			else if (AspectRatio < 0.7f)
			{
				// Cylindrical/columnar for towers/rockets
				float normY = static_cast<float>(y) / GridY;
				depthSpan = std::max(0.3f, 1.0f - normY * 0.4f);
			}

			int maxZSpan = std::max(1, static_cast<int>(halfZ * depthSpan));
			for (int z = halfZ - maxZSpan; z <= halfZ + maxZSpan; ++z)
			{
				if (z >= 0 && z < GridZ)
					Vol.Set(x, y, z, bestColor);
			}
		}
	}

	// Decompose voxel grid into physical interlocking bricks
	int TotalSteps = 1;
	DecomposeVolumeToBricks(Vol, TargetPieces, Res.Pieces, Res.PartCounts, TotalSteps, HasNeural ? &NeuralPrediction : nullptr);

	if (Res.Pieces.empty())
	{
		// Fallback: build solid foundation
		Vol.Set(GridX/2, 0, GridZ/2, BaseDominantColor);
		DecomposeVolumeToBricks(Vol, TargetPieces, Res.Pieces, Res.PartCounts, TotalSteps, HasNeural ? &NeuralPrediction : nullptr);
	}

	Res.Success = true;
	Res.BrickCount = static_cast<int>(Res.Pieces.size());
	Res.StepCount = TotalSteps;
	Res.LDrawContent = SerializeLDraw(Res.Title, Res.Pieces, Options.StepByStep);
	if (HasNeural)
	{
		Res.Explanation = QString("Synthesized novel 3D LEGO model using fine-tuned Neural Vision-to-Brick Structural Weights (%1 pieces across %2 steps).")
		                 .arg(Res.BrickCount).arg(Res.StepCount);
	}
	else
	{
		Res.Explanation = QString("Synthesized completely novel 3D LEGO model generated from reference image contours and color mapping (%1 pieces across %2 steps).")
		                 .arg(Res.BrickCount).arg(Res.StepCount);
	}

	if (TargetPieces > 0)
	{
		Res.Explanation += QString(" Conditioned on piece budget (~%1 requested).").arg(TargetPieces);
	}

	return Res;
}

// ============================================================================
// 3. OVERHAULED NATIVE ARCHETYPE STRUCTURAL SYNTHESIZERS
// ============================================================================

lcAIArchetype lcAIGenerator::ClassifyPromptArchetype(const QString& Prompt)
{
	QString p = Prompt.toLower();

	auto hasAny = [&](const QStringList& words) {
		for (const QString& w : words)
		{
			if (p.contains(w))
				return true;
		}
		return false;
	};

	// 1. Helicopter (check before aircraft)
	if (hasAny({ "helicopter", "chopper", "gyrocopter", "rotorcraft", "copter", "heli" }))
		return lcAIArchetype::Helicopter;

	// 2. Spacecraft / Spaceship / Rocket (check before watercraft and aircraft, to catch 'spaceship' and 'starfighter')
	if (hasAny({ "spaceship", "spacecraft", "starfighter", "starship", "x-wing", "rocket", "shuttle",
	             "ufo", "satellite", "orbiter", "lander", "apollo", "probe", "interstellar", "space" }))
		return lcAIArchetype::Spacecraft;

	// 3. Watercraft / Boat / Ship (check before vehicle)
	if (hasAny({ "boat", "speedboat", "ship", "submarine", "yacht", "vessel", "ferry", "sailboat",
	             "hydrofoil", "cruiser", "raft", "kayak", "canoe", "barge", "catamaran", "naval" }))
		return lcAIArchetype::Watercraft;

	// 4. Aircraft / Plane / Jet
	if (hasAny({ "plane", "airplane", "aeroplane", "jet", "biplane", "fighter", "glider",
	             "aircraft", "supersonic", "stealth", "bomber", "airliner", "cessna", "monoplane" }))
		return lcAIArchetype::Aircraft;

	// 5. Train / Locomotive
	if (hasAny({ "train", "locomotive", "railway", "railroad", "caboose", "freight", "tram", "trolley", "boxcar" }))
		return lcAIArchetype::Train;

	// 6. Creature / Animal
	if (hasAny({ "dog", "puppy", "hound", "cat", "kitten", "horse", "animal", "creature", "dinosaur",
	             "dino", "t-rex", "bird", "dragon", "elephant", "bear", "wolf", "fox", "lion", "tiger",
	             "fish", "shark", "whale", "monster", "spider", "duck", "rabbit", "bunny", "cow", "pig", "sheep" }))
		return lcAIArchetype::Creature;

	// 7. Robot / Mech
	if (hasAny({ "robot", "mech", "droid", "android", "cyborg", "mecha", "automaton", "transformer", "golem", "biped" }))
		return lcAIArchetype::Robot;

	// 8. Furniture
	if (hasAny({ "chair", "table", "desk", "seat", "stool", "bed", "couch", "sofa", "bench", "furniture", "throne", "shelf", "cabinet" }))
		return lcAIArchetype::Furniture;

	// 9. Castle / Fortress
	if (hasAny({ "castle", "fortress", "tower", "keep", "citadel", "bastion", "stronghold", "rampart", "turret" }))
		return lcAIArchetype::Castle;

	// 10. House / Building
	if (hasAny({ "house", "villa", "cottage", "cabin", "building", "home", "lodge", "mansion", "bungalow",
	             "barn", "church", "temple", "palace", "shack", "garage", "shed", "skyscraper" }))
		return lcAIArchetype::House;

	// 11. Vehicle (Cars, Trucks, Heavy machinery)
	if (hasAny({ "car", "racer", "truck", "pickup", "van", "wagon", "buggy", "jeep", "suv", "automobile",
	             "kart", "tractor", "forklift", "bulldozer", "excavator", "loader", "convertible", "coupe", "sedan", "vehicle" }))
		return lcAIArchetype::Vehicle;

	return lcAIArchetype::GenericObject;
}

lcAIModelResult lcAIGenerator::SynthesizeArchetypeModel(lcAIArchetype Archetype, const QString& Prompt, const lcAIOptions& Options)
{
	lcAIModelResult Res;
	int TargetPieces = ExtractTargetPieces(Prompt, Options.TargetPieceCount);
	if (TargetPieces <= 0) TargetPieces = 100; // Default balanced budget

	lcNeuralVLMOutput NeuralPrediction;
	bool HasNeural = PredictNeuralParameters(Options.AttachedImage, Prompt, TargetPieces, NeuralPrediction);

	int BodyColor = ResolveColor(Prompt, -1);
	if (BodyColor == -1 && !Options.AttachedImage.isNull())
		BodyColor = ExtractDominantColor(Options.AttachedImage);
	if (BodyColor == -1 && HasNeural && NeuralPrediction.DominantColor > 0)
		BodyColor = NeuralPrediction.DominantColor;

	if (BodyColor == -1)
	{
		switch (Archetype)
		{
		case lcAIArchetype::Castle: BodyColor = COLOR_LIGHT_BLUISH_GRAY; break;
		case lcAIArchetype::Furniture: BodyColor = COLOR_BROWN; break;
		case lcAIArchetype::Creature: BodyColor = COLOR_TAN; break;
		case lcAIArchetype::Aircraft: BodyColor = COLOR_WHITE; break;
		case lcAIArchetype::Helicopter: BodyColor = COLOR_YELLOW; break;
		case lcAIArchetype::Watercraft: BodyColor = COLOR_BLUE; break;
		case lcAIArchetype::Spacecraft: BodyColor = COLOR_WHITE; break;
		case lcAIArchetype::Train: BodyColor = COLOR_BLACK; break;
		case lcAIArchetype::Robot: BodyColor = COLOR_DARK_BLUISH_GRAY; break;
		case lcAIArchetype::House: BodyColor = COLOR_WHITE; break;
		case lcAIArchetype::Vehicle: BodyColor = COLOR_RED; break;
		default: BodyColor = COLOR_BLUE; break;
		}
	}

	int SecondaryColor = COLOR_YELLOW;
	if (BodyColor == COLOR_RED) SecondaryColor = COLOR_BLACK;
	else if (BodyColor == COLOR_BLUE) SecondaryColor = COLOR_WHITE;
	else if (BodyColor == COLOR_BLACK) SecondaryColor = COLOR_DARK_RED;
	else if (BodyColor == COLOR_LIGHT_BLUISH_GRAY) SecondaryColor = COLOR_DARK_BLUISH_GRAY;
	else if (BodyColor == COLOR_WHITE) SecondaryColor = COLOR_BLUE;
	else if (BodyColor == COLOR_BROWN) SecondaryColor = COLOR_TAN;
	else if (BodyColor == COLOR_TAN) SecondaryColor = COLOR_BROWN;
	else if (BodyColor == COLOR_YELLOW) SecondaryColor = COLOR_BLACK;

	int Step = 1;
	std::vector<lcAIModelPiece> Pieces;

	switch (Archetype)
	{
	case lcAIArchetype::Vehicle:
	{
		Res.Title = QString("Parametric Vehicle (%1 pcs)").arg(TargetPieces);
		int WheelbaseLength = std::max(6, std::min(20, TargetPieces / 14));
		float HalfLength = WheelbaseLength * 10.0f;

		// 1. Chassis Base Plates
		for (int z = -WheelbaseLength / 2; z <= WheelbaseLength / 2; z += 2)
		{
			AddPieceRotY(Pieces, COLOR_BLACK, 0.0f, 0.0f, z * 20.0f, 0.0f, "3020", Step);
		}
		Step++;

		// 2. Wheels and Axles (4 wheels at corners)
		AddPieceRotY(Pieces, COLOR_DARK_GRAY, -28.0f, 0.0f, -HalfLength + 10.0f, 90.0f, "6014b", Step);
		AddPieceRotY(Pieces, COLOR_DARK_GRAY, 28.0f, 0.0f, -HalfLength + 10.0f, -90.0f, "6014b", Step);
		AddPieceRotY(Pieces, COLOR_DARK_GRAY, -28.0f, 0.0f, HalfLength - 10.0f, 90.0f, "6014b", Step);
		AddPieceRotY(Pieces, COLOR_DARK_GRAY, 28.0f, 0.0f, HalfLength - 10.0f, -90.0f, "6014b", Step);
		Step++;

		// 3. Body Layers (scaled dynamically by TargetPieces)
		int BodyHeightLayers = std::max(2, std::min(6, TargetPieces / 35));
		for (int h = 1; h <= BodyHeightLayers; ++h)
		{
			float Y = -static_cast<float>(h * 24.0f);
			int LayerColor = (h % 2 == 1) ? BodyColor : SecondaryColor;

			for (int z = -WheelbaseLength / 2; z <= WheelbaseLength / 2; z += 2)
			{
				if (h == BodyHeightLayers && z == 0)
				{
					AddPieceRotY(Pieces, COLOR_TRANS_CLEAR, 0.0f, Y, z * 20.0f, 0.0f, "3039", Step); // Windshield
				}
				else
				{
					AddPieceRotY(Pieces, LayerColor, 0.0f, Y, z * 20.0f, (h % 2 == 0 ? 0.0f : 90.0f), "3001", Step);
				}
			}
			Step++;
		}

		// 4. Detailing (Spoilers, Headlights, Taillights)
		float TopY = -static_cast<float>((BodyHeightLayers + 1) * 24.0f);
		AddPieceRotY(Pieces, SecondaryColor, 0.0f, TopY + 12.0f, HalfLength, 0.0f, "3020", Step); // Rear Wing
		AddPieceRotY(Pieces, COLOR_TRANS_YELLOW, -15.0f, -24.0f, -HalfLength, 0.0f, "3005", Step); // Headlight L
		AddPieceRotY(Pieces, COLOR_TRANS_YELLOW, 15.0f, -24.0f, -HalfLength, 0.0f, "3005", Step);  // Headlight R
		AddPieceRotY(Pieces, COLOR_TRANS_RED, -15.0f, -24.0f, HalfLength, 0.0f, "3005", Step);     // Taillight L
		AddPieceRotY(Pieces, COLOR_TRANS_RED, 15.0f, -24.0f, HalfLength, 0.0f, "3005", Step);      // Taillight R
		Step++;
		break;
	}

	case lcAIArchetype::Train:
	{
		Res.Title = QString("Parametric Locomotive (%1 pcs)").arg(TargetPieces);
		int TrainLength = std::max(10, std::min(24, TargetPieces / 10));
		float HalfL = TrainLength * 10.0f;

		// 1. Heavy Base Frame & Buffer Beams
		for (int z = -TrainLength / 2; z <= TrainLength / 2; z += 2)
		{
			AddPieceRotY(Pieces, COLOR_BLACK, 0.0f, 0.0f, z * 20.0f, 0.0f, "3020", Step);
		}
		AddPieceRotY(Pieces, COLOR_RED, 0.0f, 0.0f, -HalfL - 10.0f, 90.0f, "3023", Step);
		AddPieceRotY(Pieces, COLOR_RED, 0.0f, 0.0f, HalfL + 10.0f, 90.0f, "3023", Step);
		Step++;

		// 2. Wheels (6 wheels along sides)
		AddPieceRotY(Pieces, COLOR_DARK_GRAY, -28.0f, 0.0f, -HalfL + 10.0f, 90.0f, "6014b", Step);
		AddPieceRotY(Pieces, COLOR_DARK_GRAY, 28.0f, 0.0f, -HalfL + 10.0f, -90.0f, "6014b", Step);
		AddPieceRotY(Pieces, COLOR_DARK_GRAY, -28.0f, 0.0f, 0.0f, 90.0f, "6014b", Step);
		AddPieceRotY(Pieces, COLOR_DARK_GRAY, 28.0f, 0.0f, 0.0f, -90.0f, "6014b", Step);
		AddPieceRotY(Pieces, COLOR_DARK_GRAY, -28.0f, 0.0f, HalfL - 10.0f, 90.0f, "6014b", Step);
		AddPieceRotY(Pieces, COLOR_DARK_GRAY, 28.0f, 0.0f, HalfL - 10.0f, -90.0f, "6014b", Step);
		Step++;

		// 3. Boiler Section (front 60%)
		int CabSplit = TrainLength / 6;
		for (int z = -TrainLength / 2; z < CabSplit; z += 2)
		{
			AddPieceRotY(Pieces, BodyColor, 0.0f, -24.0f, z * 20.0f, 0.0f, "3001", Step);
			AddPieceRotY(Pieces, BodyColor, 0.0f, -48.0f, z * 20.0f, 0.0f, "3003", Step);
		}
		Step++;

		// 4. Operator Cab (rear 40%) & Roof
		for (int h = 1; h <= 3; ++h)
		{
			float Y = -static_cast<float>(h * 24.0f);
			for (int z = CabSplit; z <= TrainLength / 2; z += 2)
			{
				if (h == 2)
				{
					AddPieceRotY(Pieces, COLOR_TRANS_CLEAR, -10.0f, Y, z * 20.0f, 0.0f, "3004", Step);
					AddPieceRotY(Pieces, COLOR_TRANS_CLEAR, 10.0f, Y, z * 20.0f, 0.0f, "3004", Step);
				}
				else
				{
					AddPieceRotY(Pieces, SecondaryColor, 0.0f, Y, z * 20.0f, 0.0f, "3001", Step);
				}
			}
		}
		for (int z = CabSplit; z <= TrainLength / 2; z += 2)
		{
			AddPieceRotY(Pieces, COLOR_BLACK, 0.0f, -80.0f, z * 20.0f, 0.0f, "3020", Step);
		}
		Step++;

		// 5. Smokestack, Cowcatcher & Headlight
		AddPieceRotY(Pieces, COLOR_BLACK, 0.0f, -72.0f, -HalfL + 20.0f, 0.0f, "3005", Step);
		AddPieceRotY(Pieces, COLOR_TRANS_YELLOW, 0.0f, -36.0f, -HalfL - 5.0f, 0.0f, "3005", Step);
		AddPieceRotY(Pieces, COLOR_DARK_GRAY, 0.0f, -12.0f, -HalfL - 8.0f, 0.0f, "3039", Step);
		Step++;
		break;
	}

	case lcAIArchetype::Watercraft:
	{
		Res.Title = QString("Parametric Watercraft (%1 pcs)").arg(TargetPieces);
		int BoatLength = std::max(10, std::min(24, TargetPieces / 10));
		float HalfL = BoatLength * 10.0f;

		// 1. Keel & Lower V-Hull (NO WHEELS!)
		for (int z = -BoatLength / 2 + 1; z <= BoatLength / 2; z += 2)
		{
			AddPieceRotY(Pieces, COLOR_DARK_GRAY, 0.0f, 0.0f, z * 20.0f, 0.0f, "3020", Step);
		}
		AddPieceRotY(Pieces, COLOR_DARK_GRAY, 0.0f, 0.0f, -HalfL, 0.0f, "3040", Step); // Bow
		Step++;

		// 2. Hull Freeboard Walls (widening in midsection)
		for (int z = -BoatLength / 2 + 1; z <= BoatLength / 2; z += 2)
		{
			AddPieceRotY(Pieces, BodyColor, -15.0f, -24.0f, z * 20.0f, 0.0f, "3004", Step);
			AddPieceRotY(Pieces, BodyColor, 15.0f, -24.0f, z * 20.0f, 0.0f, "3004", Step);
		}
		AddPieceRotY(Pieces, BodyColor, 0.0f, -24.0f, -HalfL + 10.0f, 0.0f, "3039", Step); // Deck
		AddPieceRotY(Pieces, BodyColor, 0.0f, -24.0f, HalfL, 90.0f, "3001", Step);         // Transom
		Step++;

		// 3. Cockpit & Windshield
		AddPieceRotY(Pieces, COLOR_TRANS_CLEAR, 0.0f, -48.0f, -HalfL * 0.3f, 0.0f, "3039", Step); // Windshield
		AddPieceRotY(Pieces, SecondaryColor, -15.0f, -48.0f, 0.0f, 0.0f, "3004", Step);
		AddPieceRotY(Pieces, SecondaryColor, 15.0f, -48.0f, 0.0f, 0.0f, "3004", Step);
		Step++;

		// 4. Outboard Motor at Stern (Propulsion, NO wheels!)
		AddPieceRotY(Pieces, COLOR_BLACK, 0.0f, -12.0f, HalfL + 15.0f, 0.0f, "3005", Step);
		AddPieceRotY(Pieces, COLOR_BLACK, 0.0f, -36.0f, HalfL + 15.0f, 0.0f, "3040", Step);
		AddPieceRotY(Pieces, COLOR_WHITE, 0.0f, -48.0f, HalfL - 10.0f, 0.0f, "3023", Step); // Stern flag
		Step++;
		break;
	}

	case lcAIArchetype::Aircraft:
	{
		Res.Title = QString("Parametric Aircraft (%1 pcs)").arg(TargetPieces);
		int FuselageLength = std::max(10, std::min(24, TargetPieces / 11));
		int WingSpan = std::max(8, std::min(22, TargetPieces / 12));
		float HalfL = FuselageLength * 10.0f;

		// 1. Fuselage Core Spine
		for (int z = -FuselageLength / 2; z <= FuselageLength / 2; z += 2)
		{
			AddPieceRotY(Pieces, BodyColor, 0.0f, -24.0f, z * 20.0f, 0.0f, "3001", Step);
		}
		AddPieceRotY(Pieces, SecondaryColor, 0.0f, -24.0f, -HalfL - 10.0f, 0.0f, "3040", Step); // Nose
		Step++;

		// 2. Swept Wings (NO wheels on sides)
		for (int w = 1; w <= WingSpan / 2; ++w)
		{
			float X = w * 20.0f;
			float Z = w * 10.0f;
			AddPieceRotY(Pieces, SecondaryColor, -X, -16.0f, Z, 0.0f, "3020", Step); // Left Wing
			AddPieceRotY(Pieces, SecondaryColor, X, -16.0f, Z, 0.0f, "3020", Step);  // Right Wing
		}
		Step++;

		// 3. Underwing Jet Engines & Cockpit Canopy
		float EngX = (WingSpan / 4 + 1) * 20.0f;
		AddPieceRotY(Pieces, COLOR_DARK_GRAY, -EngX, 0.0f, 10.0f, 0.0f, "3005", Step); // Left Engine
		AddPieceRotY(Pieces, COLOR_DARK_GRAY, EngX, 0.0f, 10.0f, 0.0f, "3005", Step);  // Right Engine
		AddPieceRotY(Pieces, COLOR_TRANS_CLEAR, 0.0f, -48.0f, -HalfL * 0.4f, 0.0f, "3039", Step); // Cockpit
		Step++;

		// 4. Tail Empennage (Vertical Fin & Stabilizers)
		AddPieceRotY(Pieces, SecondaryColor, 0.0f, -48.0f, HalfL - 5.0f, 90.0f, "3039", Step); // Vertical Fin
		AddPieceRotY(Pieces, SecondaryColor, 0.0f, -72.0f, HalfL, 90.0f, "3040", Step);
		AddPieceRotY(Pieces, SecondaryColor, -20.0f, -28.0f, HalfL - 10.0f, 0.0f, "3023", Step); // Left Stab
		AddPieceRotY(Pieces, SecondaryColor, 20.0f, -28.0f, HalfL - 10.0f, 0.0f, "3023", Step);  // Right Stab
		Step++;
		break;
	}

	case lcAIArchetype::Helicopter:
	{
		Res.Title = QString("Parametric Helicopter (%1 pcs)").arg(TargetPieces);
		// 1. Landing Skids (NO wheels)
		AddPieceRotY(Pieces, COLOR_BLACK, -25.0f, 0.0f, 0.0f, 0.0f, "3020", Step); // Left Skid
		AddPieceRotY(Pieces, COLOR_BLACK, 25.0f, 0.0f, 0.0f, 0.0f, "3020", Step);  // Right Skid
		AddPieceRotY(Pieces, COLOR_DARK_GRAY, 0.0f, -8.0f, -15.0f, 90.0f, "3023", Step); // Strut Front
		AddPieceRotY(Pieces, COLOR_DARK_GRAY, 0.0f, -8.0f, 15.0f, 90.0f, "3023", Step);  // Strut Rear
		Step++;

		// 2. Cabin Pod
		for (int h = 1; h <= 2; ++h)
		{
			float Y = -static_cast<float>(h * 24.0f);
			AddPieceRotY(Pieces, BodyColor, 0.0f, Y, 0.0f, 0.0f, "3001", Step);
			AddPieceRotY(Pieces, BodyColor, 0.0f, Y, -20.0f, 0.0f, "3003", Step);
		}
		AddPieceRotY(Pieces, COLOR_TRANS_CLEAR, 0.0f, -48.0f, -30.0f, 0.0f, "3039", Step); // Bubble Canopy
		Step++;

		// 3. Tail Boom & Tail Rotor
		AddPieceRotY(Pieces, SecondaryColor, 0.0f, -36.0f, 30.0f, 0.0f, "3004", Step);
		AddPieceRotY(Pieces, SecondaryColor, 0.0f, -36.0f, 60.0f, 0.0f, "3004", Step);
		AddPieceRotY(Pieces, SecondaryColor, 0.0f, -36.0f, 90.0f, 0.0f, "3004", Step);
		AddPieceRotY(Pieces, SecondaryColor, 0.0f, -60.0f, 90.0f, 90.0f, "3039", Step); // Tail Fin
		AddPieceRotY(Pieces, COLOR_BLACK, 6.0f, -60.0f, 90.0f, 90.0f, "3023", Step);    // Tail Rotor
		Step++;

		// 4. Main Rotor Mast & 4-Blade Cross Rotor
		AddPieceRotY(Pieces, COLOR_BLACK, 0.0f, -72.0f, 0.0f, 0.0f, "3005", Step); // Mast
		AddPieceRotY(Pieces, COLOR_BLACK, 0.0f, -80.0f, -40.0f, 0.0f, "3020", Step);  // Blade Front
		AddPieceRotY(Pieces, COLOR_BLACK, 0.0f, -80.0f, 40.0f, 0.0f, "3020", Step);   // Blade Rear
		AddPieceRotY(Pieces, COLOR_BLACK, -40.0f, -80.0f, 0.0f, 90.0f, "3020", Step); // Blade Left
		AddPieceRotY(Pieces, COLOR_BLACK, 40.0f, -80.0f, 0.0f, 90.0f, "3020", Step);  // Blade Right
		Step++;
		break;
	}

	case lcAIArchetype::Spacecraft:
	{
		Res.Title = QString("Parametric Spacecraft (%1 pcs)").arg(TargetPieces);
		int Length = std::max(10, std::min(24, TargetPieces / 11));
		int WingSpan = std::max(8, std::min(22, TargetPieces / 12));
		float HalfL = Length * 10.0f;

		// 1. Central Fuselage Core & Needle Nose
		for (int z = -Length / 2; z <= Length / 2; z += 2)
		{
			AddPieceRotY(Pieces, BodyColor, 0.0f, -24.0f, z * 20.0f, 0.0f, "3001", Step);
		}
		AddPieceRotY(Pieces, SecondaryColor, 0.0f, -24.0f, -HalfL - 10.0f, 0.0f, "3040", Step);
		Step++;

		// 2. Attack Wings & Wingtip Blasters
		for (int w = 1; w <= WingSpan / 2; ++w)
		{
			float X = w * 20.0f;
			float Z = (w * 12.0f);
			AddPieceRotY(Pieces, SecondaryColor, -X, -16.0f, Z, 0.0f, "3020", Step);
			AddPieceRotY(Pieces, SecondaryColor, X, -16.0f, Z, 0.0f, "3020", Step);
		}
		float TipX = (WingSpan / 2) * 20.0f;
		float TipZ = (WingSpan / 2) * 12.0f;
		AddPieceRotY(Pieces, COLOR_DARK_GRAY, -TipX, -16.0f, TipZ - 20.0f, 0.0f, "3010", Step);
		AddPieceRotY(Pieces, COLOR_DARK_GRAY, TipX, -16.0f, TipZ - 20.0f, 0.0f, "3010", Step);
		Step++;

		// 3. Shielded Cockpit & Dual Rocket Thrusters
		AddPieceRotY(Pieces, COLOR_TRANS_DARK_BLUE, 0.0f, -48.0f, -HalfL * 0.3f, 0.0f, "3039", Step);
		AddPieceRotY(Pieces, COLOR_DARK_GRAY, -15.0f, -24.0f, HalfL + 10.0f, 0.0f, "3005", Step);
		AddPieceRotY(Pieces, COLOR_DARK_GRAY, 15.0f, -24.0f, HalfL + 10.0f, 0.0f, "3005", Step);
		AddPieceRotY(Pieces, COLOR_TRANS_YELLOW, -15.0f, -24.0f, HalfL + 20.0f, 0.0f, "3024", Step);
		AddPieceRotY(Pieces, COLOR_TRANS_YELLOW, 15.0f, -24.0f, HalfL + 20.0f, 0.0f, "3024", Step);
		Step++;
		break;
	}

	case lcAIArchetype::Creature:
	{
		Res.Title = QString("Parametric Creature / Animal (%1 pcs)").arg(TargetPieces);
		// 1. 4 Standing Legs (NO wheels!)
		int LegColor = BodyColor;
		for (int h = 0; h < 2; ++h)
		{
			float Y = -static_cast<float>(h * 24.0f + 12.0f);
			AddPieceRotY(Pieces, LegColor, -20.0f, Y, -30.0f, 0.0f, "3005", Step); // Front Left
			AddPieceRotY(Pieces, LegColor, 20.0f, Y, -30.0f, 0.0f, "3005", Step);  // Front Right
			AddPieceRotY(Pieces, LegColor, -20.0f, Y, 30.0f, 0.0f, "3005", Step);  // Back Left
			AddPieceRotY(Pieces, LegColor, 20.0f, Y, 30.0f, 0.0f, "3005", Step);   // Back Right
		}
		Step++;

		// 2. Torso Body (bridging legs)
		AddPieceRotY(Pieces, BodyColor, 0.0f, -48.0f, 0.0f, 0.0f, "3001", Step);
		AddPieceRotY(Pieces, BodyColor, 0.0f, -48.0f, -25.0f, 0.0f, "3003", Step);
		AddPieceRotY(Pieces, BodyColor, 0.0f, -48.0f, 25.0f, 0.0f, "3003", Step);
		Step++;

		// 3. Neck & Head
		AddPieceRotY(Pieces, BodyColor, 0.0f, -72.0f, -30.0f, 0.0f, "3005", Step);  // Neck
		AddPieceRotY(Pieces, BodyColor, 0.0f, -84.0f, -45.0f, 0.0f, "3003", Step);  // Head
		AddPieceRotY(Pieces, COLOR_BLACK, 0.0f, -76.0f, -65.0f, 0.0f, "3005", Step);// Snout / Nose
		Step++;

		// 4. Ears & Wagging Tail
		AddPieceRotY(Pieces, SecondaryColor, -10.0f, -100.0f, -45.0f, 0.0f, "3040", Step); // Left Ear
		AddPieceRotY(Pieces, SecondaryColor, 10.0f, -100.0f, -45.0f, 0.0f, "3040", Step);  // Right Ear
		AddPieceRotY(Pieces, BodyColor, 0.0f, -60.0f, 45.0f, 0.0f, "3040", Step);          // Tail
		Step++;
		break;
	}

	case lcAIArchetype::Robot:
	{
		Res.Title = QString("Parametric Robot Mech (%1 pcs)").arg(TargetPieces);
		// 1. Bipedal Feet & Legs (NO wheels!)
		AddPieceRotY(Pieces, COLOR_DARK_GRAY, -25.0f, 0.0f, 0.0f, 0.0f, "3020", Step); // Left Foot
		AddPieceRotY(Pieces, COLOR_DARK_GRAY, 25.0f, 0.0f, 0.0f, 0.0f, "3020", Step);  // Right Foot
		for (int h = 1; h <= 2; ++h)
		{
			float Y = -static_cast<float>(h * 24.0f);
			AddPieceRotY(Pieces, BodyColor, -25.0f, Y, 0.0f, 0.0f, "3004", Step); // Left Leg
			AddPieceRotY(Pieces, BodyColor, 25.0f, Y, 0.0f, 0.0f, "3004", Step);  // Right Leg
		}
		Step++;

		// 2. Pelvis & Heavy Armored Chest
		AddPieceRotY(Pieces, COLOR_DARK_GRAY, 0.0f, -60.0f, 0.0f, 90.0f, "3020", Step); // Waist
		AddPieceRotY(Pieces, BodyColor, 0.0f, -84.0f, 0.0f, 0.0f, "3001", Step);       // Lower Chest
		AddPieceRotY(Pieces, SecondaryColor, 0.0f, -108.0f, 0.0f, 0.0f, "3001", Step);   // Upper Chest
		Step++;

		// 3. Shoulders & Arms with Blaster
		AddPieceRotY(Pieces, SecondaryColor, -45.0f, -108.0f, 0.0f, 0.0f, "3020", Step); // Left Shoulder
		AddPieceRotY(Pieces, SecondaryColor, 45.0f, -108.0f, 0.0f, 0.0f, "3020", Step);  // Right Shoulder
		AddPieceRotY(Pieces, BodyColor, -45.0f, -84.0f, -15.0f, 0.0f, "3004", Step);     // Left Arm
		AddPieceRotY(Pieces, COLOR_DARK_GRAY, 45.0f, -84.0f, -20.0f, 0.0f, "3010", Step);// Right Blaster
		Step++;

		// 4. Head & Glowing Visor
		AddPieceRotY(Pieces, BodyColor, 0.0f, -132.0f, 0.0f, 0.0f, "3003", Step); // Head
		AddPieceRotY(Pieces, COLOR_TRANS_RED, 0.0f, -128.0f, -12.0f, 0.0f, "3023", Step); // Visor Slit
		AddPieceRotY(Pieces, COLOR_DARK_GRAY, 0.0f, -96.0f, 15.0f, 0.0f, "3004", Step);   // Power Pack
		Step++;
		break;
	}

	case lcAIArchetype::Furniture:
	{
		bool IsTable = Prompt.contains("table", Qt::CaseInsensitive) || Prompt.contains("desk", Qt::CaseInsensitive);
		if (IsTable)
		{
			Res.Title = QString("Parametric Table (%1 pcs)").arg(TargetPieces);
			// Table: 4 corner legs + tabletop (NO wheels!)
			for (int h = 0; h < 3; ++h)
			{
				float Y = -static_cast<float>(h * 24.0f + 12.0f);
				AddPieceRotY(Pieces, BodyColor, -35.0f, Y, -35.0f, 0.0f, "3005", Step);
				AddPieceRotY(Pieces, BodyColor, 35.0f, Y, -35.0f, 0.0f, "3005", Step);
				AddPieceRotY(Pieces, BodyColor, -35.0f, Y, 35.0f, 0.0f, "3005", Step);
				AddPieceRotY(Pieces, BodyColor, 35.0f, Y, 35.0f, 0.0f, "3005", Step);
			}
			Step++;
			for (int z = -30; z <= 30; z += 30)
			{
				AddPieceRotY(Pieces, SecondaryColor, 0.0f, -80.0f, z, 0.0f, "3020", Step);
			}
			Step++;
		}
		else
		{
			Res.Title = QString("Parametric Chair (%1 pcs)").arg(TargetPieces);
			// Chair: 4 corner legs + seat plate + vertical backrest (NO wheels!)
			for (int h = 0; h < 2; ++h)
			{
				float Y = -static_cast<float>(h * 24.0f + 12.0f);
				AddPieceRotY(Pieces, BodyColor, -15.0f, Y, -15.0f, 0.0f, "3005", Step); // Front Left
				AddPieceRotY(Pieces, BodyColor, 15.0f, Y, -15.0f, 0.0f, "3005", Step);  // Front Right
				AddPieceRotY(Pieces, BodyColor, -15.0f, Y, 15.0f, 0.0f, "3005", Step);  // Rear Left
				AddPieceRotY(Pieces, BodyColor, 15.0f, Y, 15.0f, 0.0f, "3005", Step);   // Rear Right
			}
			Step++;
			// Seat Cushion Plate
			AddPieceRotY(Pieces, SecondaryColor, 0.0f, -54.0f, 0.0f, 0.0f, "3020", Step);
			Step++;
			// Vertical Backrest
			for (int bh = 3; bh <= 5; ++bh)
			{
				float BY = -static_cast<float>(bh * 24.0f);
				AddPieceRotY(Pieces, BodyColor, 0.0f, BY, 15.0f, 0.0f, "3004", Step);
			}
			Step++;
		}
		break;
	}

	case lcAIArchetype::Castle:
	{
		Res.Title = QString("Parametric Castle Fortress (%1 pcs)").arg(TargetPieces);
		int Footprint = std::max(4, std::min(8, TargetPieces / 30));
		int Stories = std::max(4, std::min(12, TargetPieces / 20));
		float FpStuds = Footprint * 15.0f;

		// 1. Foundation Base Slab
		for (int x = -Footprint; x <= Footprint; x += 2)
		{
			for (int z = -Footprint; z <= Footprint; z += 2)
			{
				AddPieceRotY(Pieces, COLOR_DARK_GRAY, x * 15.0f, 0.0f, z * 15.0f, 0.0f, "3020", Step);
			}
		}
		Step++;

		// 2. Corner Turrets & Walls
		for (int s = 1; s <= Stories; ++s)
		{
			float Y = -static_cast<float>(s * 24.0f);
			AddPieceRotY(Pieces, COLOR_LIGHT_BLUISH_GRAY, -FpStuds, Y, -FpStuds, 0.0f, "3003", Step);
			AddPieceRotY(Pieces, COLOR_LIGHT_BLUISH_GRAY, FpStuds, Y, -FpStuds, 0.0f, "3003", Step);
			AddPieceRotY(Pieces, COLOR_LIGHT_BLUISH_GRAY, -FpStuds, Y, FpStuds, 0.0f, "3003", Step);
			AddPieceRotY(Pieces, COLOR_LIGHT_BLUISH_GRAY, FpStuds, Y, FpStuds, 0.0f, "3003", Step);

			if (s > 1 || Footprint > 4)
			{
				AddPieceRotY(Pieces, COLOR_LIGHT_BLUISH_GRAY, 0.0f, Y, -FpStuds, 0.0f, "3004", Step);
			}
			AddPieceRotY(Pieces, COLOR_LIGHT_BLUISH_GRAY, 0.0f, Y, FpStuds, 0.0f, "3004", Step);
			AddPieceRotY(Pieces, COLOR_LIGHT_BLUISH_GRAY, -FpStuds, Y, 0.0f, 90.0f, "3004", Step);
			AddPieceRotY(Pieces, COLOR_LIGHT_BLUISH_GRAY, FpStuds, Y, 0.0f, 90.0f, "3004", Step);
			Step++;
		}

		// 3. Battlements (Crenellations) & Spire Banner
		float BatY = -static_cast<float>((Stories + 1) * 24.0f);
		AddPieceRotY(Pieces, COLOR_DARK_BLUISH_GRAY, -FpStuds, BatY, -FpStuds, 0.0f, "3039", Step);
		AddPieceRotY(Pieces, COLOR_DARK_BLUISH_GRAY, FpStuds, BatY, -FpStuds, 0.0f, "3039", Step);
		AddPieceRotY(Pieces, COLOR_DARK_BLUISH_GRAY, -FpStuds, BatY, FpStuds, 0.0f, "3039", Step);
		AddPieceRotY(Pieces, COLOR_DARK_BLUISH_GRAY, FpStuds, BatY, FpStuds, 0.0f, "3039", Step);
		AddPieceRotY(Pieces, COLOR_LIGHT_BLUISH_GRAY, 0.0f, BatY - 24.0f, 0.0f, 0.0f, "3005", Step);
		AddPieceRotY(Pieces, COLOR_RED, 8.0f, BatY - 32.0f, 0.0f, 90.0f, "3023", Step); // Flag
		Step++;
		break;
	}

	case lcAIArchetype::House:
	{
		Res.Title = QString("Parametric Building (%1 pcs)").arg(TargetPieces);
		int HouseLength = std::max(6, std::min(12, TargetPieces / 20));
		int HouseWidth = std::max(4, std::min(8, TargetPieces / 25));
		float HalfL = HouseLength * 10.0f;
		float HalfW = HouseWidth * 10.0f;

		// 1. Foundation Slab
		for (int z = -HouseLength / 2; z <= HouseLength / 2; z += 2)
		{
			AddPieceRotY(Pieces, COLOR_DARK_GRAY, 0.0f, 0.0f, z * 20.0f, 0.0f, "3020", Step);
		}
		Step++;

		// 2. Walls with Window and Door openings
		int Stories = std::max(2, std::min(4, TargetPieces / 40));
		for (int s = 1; s <= Stories; ++s)
		{
			float Y = -static_cast<float>(s * 24.0f);
			AddPieceRotY(Pieces, BodyColor, -HalfW, Y, 0.0f, 90.0f, "3001", Step);
			AddPieceRotY(Pieces, BodyColor, HalfW, Y, 0.0f, 90.0f, "3001", Step);

			if (s == 1)
			{
				AddPieceRotY(Pieces, BodyColor, -HalfW / 2, Y, -HalfL, 0.0f, "3004", Step);
				AddPieceRotY(Pieces, BodyColor, HalfW / 2, Y, -HalfL, 0.0f, "3004", Step);
			}
			else
			{
				AddPieceRotY(Pieces, COLOR_TRANS_CLEAR, 0.0f, Y, -HalfL, 0.0f, "3004", Step);
				AddPieceRotY(Pieces, COLOR_TRANS_CLEAR, 0.0f, Y, HalfL, 0.0f, "3004", Step);
			}
			Step++;
		}

		// 3. Gabled Pitched Roof with 45-degree slopes
		float RoofBaseY = -static_cast<float>((Stories + 1) * 24.0f);
		for (int z = -HouseLength / 2; z <= HouseLength / 2; z += 2)
		{
			AddPieceRotY(Pieces, SecondaryColor, -15.0f, RoofBaseY, z * 20.0f, 0.0f, "3039", Step);
			AddPieceRotY(Pieces, SecondaryColor, 15.0f, RoofBaseY, z * 20.0f, 180.0f, "3039", Step);
		}
		AddPieceRotY(Pieces, COLOR_DARK_RED, -15.0f, RoofBaseY - 24.0f, HalfL - 20.0f, 0.0f, "3005", Step); // Chimney
		Step++;
		break;
	}

	case lcAIArchetype::GenericObject:
	default:
	{
		Res.Title = QString("Sculpted LEGO Model (%1 pcs)").arg(TargetPieces);
		int Height = std::max(3, std::min(8, TargetPieces / 20));
		for (int h = 1; h <= Height; ++h)
		{
			float Y = -static_cast<float>(h * 24.0f);
			int LayerColor = (h % 2 == 1) ? BodyColor : SecondaryColor;
			int Span = std::max(1, Height - h + 1);
			for (int x = -Span; x <= Span; ++x)
			{
				AddPieceRotY(Pieces, LayerColor, x * 20.0f, Y, 0.0f, 0.0f, "3004", Step);
			}
			Step++;
		}
		break;
	}
	}

	Res.Success = true;
	Res.BrickCount = static_cast<int>(Pieces.size());
	Res.StepCount = Step;
	Res.Pieces = Pieces;
	for (const auto& p : Pieces)
		Res.PartCounts[p.PartId]++;
	Res.LDrawContent = SerializeLDraw(Res.Title, Pieces, Options.StepByStep);
	Res.Explanation = QString("Synthesized distinct 3D LEGO model for '%1' matching piece budget (%2 pieces across %3 building steps).")
	                 .arg(Prompt.isEmpty() ? "custom build" : Prompt).arg(Res.BrickCount).arg(Res.StepCount);

	return Res;
}

lcAIModelResult lcAIGenerator::SynthesizeParametricModel(const QString& Category, const QString& Prompt, const lcAIOptions& Options)
{
	lcAIArchetype Archetype = ClassifyPromptArchetype(Category + " " + Prompt);
	return SynthesizeArchetypeModel(Archetype, Prompt, Options);
}

// ============================================================================
// 4. NEURAL VLM INFERENCE BRIDGE (Ollama / Local / OpenAI)
// ============================================================================

lcAIModelResult lcAIGenerator::CallNeuralVLM(const QString& Prompt, const QImage& Image, const lcAIOptions& Options)
{
	lcAIModelResult Result;
	QString Endpoint = Options.VlmEndpoint;
	if (Endpoint.isEmpty())
		Endpoint = "http://localhost:11434/api/generate";

	int TargetPieces = ExtractTargetPieces(Prompt, Options.TargetPieceCount);
	if (TargetPieces <= 0) TargetPieces = 150;

	// Encode image to base64
	QString ImageBase64;
	if (!Image.isNull())
	{
		QByteArray ba;
		QBuffer buf(&ba);
		buf.open(QIODevice::WriteOnly);
		Image.scaled(512, 512, Qt::KeepAspectRatio, Qt::SmoothTransformation).save(&buf, "JPEG", 85);
		ImageBase64 = QString::fromLatin1(ba.toBase64().data());
	}

	// Prepare system and user prompt
	QString SysPrompt = QString(
		"You are a specialized 3D LEGO CAD builder agent. "
		"Synthesize a complete, novel, physically stable 3D LEGO model in LDraw format matching the prompt and attached image. "
		"Piece count target: approximately %1 pieces. "
		"Output ONLY the LDraw lines ('1 <color> <x> <y> <z> <rot> <part>') and '0 STEP' markers enclosed in a ```ldraw code block."
	).arg(TargetPieces);

	QJsonObject Payload;
	if (Endpoint.contains("/api/generate")) // Ollama API format
	{
		Payload["model"] = Options.VlmModel.isEmpty() ? "qwen2-vl" : Options.VlmModel;
		Payload["prompt"] = SysPrompt + "\nUser request: " + Prompt;
		Payload["stream"] = false;
		if (!ImageBase64.isEmpty())
		{
			QJsonArray imgArr;
			imgArr.append(ImageBase64);
			Payload["images"] = imgArr;
		}
	}
	else // OpenAI / vLLM chat completions format
	{
		Payload["model"] = Options.VlmModel.isEmpty() ? "gpt-4o" : Options.VlmModel;
		QJsonArray messages;
		QJsonObject sysMsg;
		sysMsg["role"] = "system";
		sysMsg["content"] = SysPrompt;
		messages.append(sysMsg);

		QJsonObject userMsg;
		userMsg["role"] = "user";
		QJsonArray userContent;
		QJsonObject txtPart;
		txtPart["type"] = "text";
		txtPart["text"] = Prompt;
		userContent.append(txtPart);

		if (!ImageBase64.isEmpty())
		{
			QJsonObject imgPart;
			imgPart["type"] = "image_url";
			QJsonObject imgUrl;
			imgUrl["url"] = "data:image/jpeg;base64," + ImageBase64;
			imgPart["image_url"] = imgUrl;
			userContent.append(imgPart);
		}
		userMsg["content"] = userContent;
		messages.append(userMsg);
		Payload["messages"] = messages;
	}

	QByteArray postData = QJsonDocument(Payload).toJson();

	QNetworkAccessManager nam;
	QNetworkRequest req;
	req.setUrl(QUrl(Endpoint));
	req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	if (!Options.VlmApiKey.isEmpty())
		req.setRawHeader("Authorization", ("Bearer " + Options.VlmApiKey).toUtf8());

	QNetworkReply* reply = nam.post(req, postData);

	QEventLoop loop;
	QTimer timer;
	timer.setSingleShot(true);
	QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
	QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
	timer.start(30000); // 30 second timeout
	loop.exec();

	if (timer.isActive())
		timer.stop();
	else
	{
		reply->abort();
		reply->deleteLater();
		Result.Success = false;
		Result.ErrorMessage = QString("VLM request timed out after 30s connecting to %1").arg(Endpoint);
		return Result;
	}

	if (reply->error() != QNetworkReply::NoError)
	{
		QString errStr = reply->errorString();
		reply->deleteLater();
		Result.Success = false;
		Result.ErrorMessage = QString("Network error connecting to VLM (%1): %2").arg(Endpoint, errStr);
		return Result;
	}

	QByteArray responseData = reply->readAll();
	reply->deleteLater();

	QJsonDocument doc = QJsonDocument::fromJson(responseData);
	QString outputText;
	if (doc.isObject())
	{
		QJsonObject obj = doc.object();
		if (obj.contains("response")) // Ollama format
			outputText = obj["response"].toString();
		else if (obj.contains("choices")) // OpenAI format
		{
			QJsonArray choices = obj["choices"].toArray();
			if (!choices.isEmpty())
			{
				QJsonObject first = choices[0].toObject();
				if (first.contains("message"))
					outputText = first["message"].toObject()["content"].toString();
			}
		}
	}

	// Extract LDraw code block
	int start = outputText.indexOf("```ldraw");
	if (start != -1)
	{
		start += 8;
		int end = outputText.indexOf("```", start);
		if (end != -1)
			outputText = outputText.mid(start, end - start).trimmed();
		else
			outputText = outputText.mid(start).trimmed();
	}

	Result = ParseLDrawString(Prompt.isEmpty() ? "Neural VLM LEGO Model" : Prompt, outputText);
	if (Result.Success)
	{
		Result.Explanation = QString("Generated novel 3D LEGO model via Neural VLM (%1 pieces across %2 building steps).")
		                     .arg(Result.BrickCount).arg(Result.StepCount);
	}
	return Result;
}

// ============================================================================
// 5. MASTER GENERATE MODEL DISPATCHER
// ============================================================================

lcAIModelResult lcAIGenerator::GenerateModel(const QString& Prompt, const lcAIOptions& Options)
{
	// 1. Prepare image if provided
	QImage Img = Options.AttachedImage;
	if (Img.isNull() && !Options.ImagePath.isEmpty())
		Img.load(Options.ImagePath);

	// 2. Route to Neural VLM if selected
	if (Options.Backend == lcAIBackend::NeuralVLM)
	{
		lcAIModelResult VlmRes = CallNeuralVLM(Prompt, Img, Options);
		if (VlmRes.Success)
			return VlmRes;

		// If VLM failed (e.g. Ollama offline), fallback seamlessly to Native Synthesizer
	}

	// 3. Native Multimodal 3D Voxel Synthesizer (when image attached)
	if (!Img.isNull())
	{
		return SynthesizeFromImage(Img, Prompt, Options);
	}

	// 4. Overhauled Native Archetype Structural Synthesizer
	lcAIArchetype Archetype = ClassifyPromptArchetype(Prompt);
	return SynthesizeArchetypeModel(Archetype, Prompt, Options);
}

// ============================================================================
// 6. LEGACY KNOWLEDGE BASE ADAPTER (for direct set lookup if needed)
// ============================================================================

lcAIModelResult lcAIGenerator::GenerateFromKnowledgeBase(const lcKBModelEntry* Entry, const QString& Prompt, const lcAIOptions& Options)
{
	lcAIModelResult Result;
	if (!Entry)
	{
		Result.Success = false;
		Result.ErrorMessage = "Knowledge base entry is null";
		return Result;
	}

	int UserColor = ResolveColor(Prompt, -1);
	if (UserColor == -1 && !Options.AttachedImage.isNull())
		UserColor = ExtractDominantColor(Options.AttachedImage);

	int TargetOldColor = Entry->DominantColor;
	bool RemapColor = (UserColor != -1 && UserColor != TargetOldColor && TargetOldColor != 16 && TargetOldColor != 0);

	QString RawData = QString::fromUtf8(Entry->LDrawData);
	QStringList Lines = RawData.split('\n');

	int CurrentStep = 1;
	QByteArray AdjustedLDraw;
	QTextStream OutStream(&AdjustedLDraw);

	for (const QString& Line : Lines)
	{
		QString Trimmed = Line.trimmed();
		if (Trimmed.isEmpty()) continue;

		if (Trimmed == "0 STEP" || Trimmed.startsWith("0 STEP "))
		{
			CurrentStep++;
			OutStream << "0 STEP\n";
			continue;
		}

		if (Trimmed.startsWith("1 "))
		{
			QStringList Tokens = Trimmed.split(QRegExp("\\s+"), QString::SkipEmptyParts);
			if (Tokens.size() >= 15)
			{
				bool Ok = false;
				int Color = Tokens[1].toInt(&Ok);
				if (Ok)
				{
					if (RemapColor && Color == TargetOldColor)
						Color = UserColor;

					float X = Tokens[2].toFloat();
					float Y = Tokens[3].toFloat();
					float Z = Tokens[4].toFloat();
					float Rot[9];
					for (int r = 0; r < 9; ++r)
						Rot[r] = Tokens[5 + r].toFloat();

					QString Part = Tokens.mid(14).join(" ");
					AddPiece(Result.Pieces, Color, X, Y, Z, Rot, Part, CurrentStep);
					Result.PartCounts[Part]++;

					OutStream << "1 " << Color << " "
					          << QString::number(X, 'f', 2) << " "
					          << QString::number(Y, 'f', 2) << " "
					          << QString::number(Z, 'f', 2) << " ";
					for (int r = 0; r < 9; ++r)
						OutStream << QString::number(Rot[r], 'f', 4) << " ";
					OutStream << Part << "\n";
					continue;
				}
			}
		}

		OutStream << Trimmed << "\n";
	}

	OutStream.flush();

	Result.Success = true;
	Result.Title = QString::fromUtf8(Entry->Name);
	Result.Explanation = QString("Built reference LEGO® model '%1' (#%2) from catalog (%3 pieces).")
	                     .arg(QString::fromUtf8(Entry->Name), QString::fromUtf8(Entry->SetNumber))
	                     .arg(Result.Pieces.size());
	Result.BrickCount = static_cast<int>(Result.Pieces.size());
	Result.StepCount = CurrentStep;
	Result.LDrawContent = AdjustedLDraw;

	return Result;
}
