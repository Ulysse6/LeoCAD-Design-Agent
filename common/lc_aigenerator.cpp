#include "lc_global.h"
#include "lc_aigenerator.h"
#include "lc_kb_catalog.h"
#include <cmath>
#include <QTextStream>
#include <QStringList>

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
	// Rotation around Y axis
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
	if (Clean == "3045") return "Slope 45 2x2 Double Convex";
	if (Clean == "3941") return "Round Brick 2x2";
	if (Clean == "3062b") return "Round Brick 1x1";
	if (Clean == "4589" || Clean == "4589b") return "Cone 1x1";
	if (Clean == "3942") return "Cone 2x2";
	if (Clean == "6014b") return "Wheel 11mm";
	if (Clean == "3829c01") return "Steering Wheel";
	if (Clean == "3070b") return "Tile 1x1";
	if (Clean == "3069b") return "Tile 1x2";
	if (Clean == "3068b") return "Tile 2x2";
	return "Lego Part " + Clean;
}

lcAIModelResult lcAIGenerator::GenerateFromKnowledgeBase(const lcKBModelEntry* Entry, const QString& Prompt, const lcAIOptions& Options)
{
	lcAIModelResult Result;
	if (!Entry)
	{
		Result.Success = false;
		Result.ErrorMessage = "Knowledge base entry is null";
		return Result;
	}

	// Determine if user requested a custom color
	int UserColor = ResolveColor(Prompt, -1);
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
		if (Trimmed.isEmpty())
			continue;

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
	Result.Explanation = QString("Built authentic LEGO® set '%1' (#%2) from the LDraw Official Model Repository catalog, featuring %3 pieces.")
	                     .arg(QString::fromUtf8(Entry->Name), QString::fromUtf8(Entry->SetNumber))
	                     .arg(Result.Pieces.size());
	Result.BrickCount = static_cast<int>(Result.Pieces.size());
	Result.StepCount = CurrentStep;
	Result.LDrawContent = AdjustedLDraw;

	return Result;
}

lcAIModelResult lcAIGenerator::GenerateModel(const QString& Prompt, const lcAIOptions& Options)
{
	// 1. Check knowledge base catalog for best matching authentic LEGO set
	int MatchScore = 0;
	const lcKBModelEntry* KBEntry = lcKBCatalog::FindBestMatch(Prompt, &MatchScore);
	if (KBEntry && MatchScore >= 25)
	{
		return GenerateFromKnowledgeBase(KBEntry, Prompt, Options);
	}

	// 2. Fall back to procedural procedural generators
	QString Lower = Prompt.toLower().trimmed();

	if (Lower.contains("house") || Lower.contains("cottage") || Lower.contains("cabin") ||
	    Lower.contains("home") || Lower.contains("villa") || Lower.contains("mansion") ||
	    Lower.contains("building") || Lower.contains("shed"))
	{
		return BuildHouse(Prompt, Options);
	}
	if (Lower.contains("car") || Lower.contains("automobile") || Lower.contains("truck") ||
	    Lower.contains("racer") || Lower.contains("vehicle") || Lower.contains("kart") ||
	    Lower.contains("van") || Lower.contains("pickup") || Lower.contains("taxi"))
	{
		return BuildCar(Prompt, Options);
	}
	if (Lower.contains("rocket") || Lower.contains("missile") || Lower.contains("saturn"))
	{
		return BuildRocket(Prompt, Options);
	}
	if (Lower.contains("space") || Lower.contains("ship") || Lower.contains("shuttle") ||
	    Lower.contains("ufo") || Lower.contains("alien") || Lower.contains("starfighter") ||
	    Lower.contains("fighter") || Lower.contains("interceptor"))
	{
		return BuildSpaceship(Prompt, Options);
	}
	if (Lower.contains("castle") || Lower.contains("tower") || Lower.contains("fort") ||
	    Lower.contains("fortress") || Lower.contains("bastion") || Lower.contains("dungeon"))
	{
		return BuildCastleTower(Prompt, Options);
	}
	if (Lower.contains("pyramid") || Lower.contains("egypt") || Lower.contains("pharaoh") ||
	    Lower.contains("monument") || Lower.contains("ziggurat"))
	{
		return BuildPyramid(Prompt, Options);
	}
	if (Lower.contains("tree") || Lower.contains("pine") || Lower.contains("plant") ||
	    Lower.contains("flower") || Lower.contains("forest") || Lower.contains("bush"))
	{
		return BuildTree(Prompt, Options);
	}
	if (Lower.contains("boat") || Lower.contains("vessel") || Lower.contains("sail") ||
	    Lower.contains("yacht") || Lower.contains("canoe"))
	{
		return BuildBoat(Prompt, Options);
	}
	if (Lower.contains("robot") || Lower.contains("droid") || Lower.contains("mech") ||
	    Lower.contains("android") || Lower.contains("cyborg"))
	{
		return BuildRobot(Prompt, Options);
	}
	if (Lower.contains("plane") || Lower.contains("airplane") || Lower.contains("jet") ||
	    Lower.contains("aircraft") || Lower.contains("biplane") || Lower.contains("fly"))
	{
		return BuildAirplane(Prompt, Options);
	}
	if (Lower.contains("chair") || Lower.contains("table") || Lower.contains("furniture") ||
	    Lower.contains("sofa") || Lower.contains("desk") || Lower.contains("stool"))
	{
		return BuildFurniture(Prompt, Options);
	}
	if (Lower.contains("bridge") || Lower.contains("viaduct") || Lower.contains("overpass"))
	{
		return BuildBridge(Prompt, Options);
	}
	if (Lower.contains("rainbow") || Lower.contains("arch") || Lower.contains("color"))
	{
		return BuildRainbowArch(Prompt, Options);
	}

	// Fallback to custom artistic sculpture matching prompt colors and concept
	return BuildCustomSculpture(Prompt, Options);
}

// -------------------------------------------------------------
// 1. HOUSE BUILDER
// -------------------------------------------------------------
lcAIModelResult lcAIGenerator::BuildHouse(const QString& Prompt, const lcAIOptions& Options)
{
	lcAIModelResult Res;
	Res.Title = "Cozy House with Gabled Roof";

	int WallColor = ResolveColor(Prompt, COLOR_RED);
	int RoofColor = Prompt.toLower().contains("blue") ? COLOR_BLUE : (Prompt.toLower().contains("black") ? COLOR_BLACK : COLOR_BLUE);
	if (WallColor == COLOR_BLUE) RoofColor = COLOR_RED;
	int DoorColor = COLOR_YELLOW;
	int LawnColor = COLOR_GREEN;
	int ChimneyColor = COLOR_DARK_GRAY;

	std::vector<lcAIModelPiece> Pieces;
	int Step = 1;

	// Step 1: Foundation lawn plate
	// 4x8 plates at ground Y = 0 (top is Y = -8)
	AddPieceRotY(Pieces, LawnColor, -40.0f, 0.0f, 0.0f, 0.0f, "3035", Step);
	AddPieceRotY(Pieces, LawnColor, 40.0f, 0.0f, 0.0f, 0.0f, "3035", Step);
	Step++;

	// Wall course 1: Y = -8 (brick height 24, top will be Y = -32)
	// Base footprint: 80x80 LDU (4x4 studs)
	// Front with doorway: left 1x1, doorway gap (1x2), right 1x1
	AddPieceRotY(Pieces, WallColor, -30.0f, -8.0f, 30.0f, 0.0f, "3005", Step);
	AddPieceRotY(Pieces, DoorColor, 0.0f, -8.0f, 30.0f, 90.0f, "3023", Step); // Doorstep plate
	AddPieceRotY(Pieces, WallColor, 30.0f, -8.0f, 30.0f, 0.0f, "3005", Step);
	// Back wall: 1x4 brick
	AddPieceRotY(Pieces, WallColor, 0.0f, -8.0f, -30.0f, 90.0f, "3010", Step);
	// Left wall: 1x2 brick
	AddPieceRotY(Pieces, WallColor, -30.0f, -8.0f, 0.0f, 0.0f, "3004", Step);
	// Right wall: 1x2 brick
	AddPieceRotY(Pieces, WallColor, 30.0f, -8.0f, 0.0f, 0.0f, "3004", Step);
	Step++;

	// Wall course 2: Y = -32 (top Y = -56)
	// Interlocking running bond!
	AddPieceRotY(Pieces, WallColor, -30.0f, -32.0f, 30.0f, 0.0f, "3005", Step);
	AddPieceRotY(Pieces, DoorColor, 0.0f, -32.0f, 30.0f, 90.0f, "3004", Step); // Door brick
	AddPieceRotY(Pieces, WallColor, 30.0f, -32.0f, 30.0f, 0.0f, "3005", Step);
	// Side windows using trans-clear bricks
	AddPieceRotY(Pieces, COLOR_TRANS_CLEAR, -30.0f, -32.0f, 0.0f, 0.0f, "3004", Step);
	AddPieceRotY(Pieces, COLOR_TRANS_CLEAR, 30.0f, -32.0f, 0.0f, 0.0f, "3004", Step);
	AddPieceRotY(Pieces, WallColor, 0.0f, -32.0f, -30.0f, 90.0f, "3010", Step);
	Step++;

	// Wall course 3 (Lintel course): Y = -56 (top Y = -80)
	AddPieceRotY(Pieces, WallColor, 0.0f, -56.0f, 30.0f, 90.0f, "3010", Step); // Solid front lintel
	AddPieceRotY(Pieces, WallColor, 0.0f, -56.0f, -30.0f, 90.0f, "3010", Step);
	AddPieceRotY(Pieces, WallColor, -30.0f, -56.0f, 0.0f, 0.0f, "3004", Step);
	AddPieceRotY(Pieces, WallColor, 30.0f, -56.0f, 0.0f, 0.0f, "3004", Step);
	Step++;

	// Ceiling / Eaves plate: Y = -80 (top Y = -88)
	AddPieceRotY(Pieces, COLOR_WHITE, 0.0f, -80.0f, 0.0f, 0.0f, "3031", Step); // 4x4 plate cornice
	Step++;

	// Roof course 1: 45-degree slope bricks: Y = -88 (top Y = -112)
	// Left slope faces inwards towards center (+X), right slope faces inwards (-X)
	AddPieceRotY(Pieces, RoofColor, -20.0f, -88.0f, 0.0f, 90.0f, "3037", Step); // Slope 2x4
	AddPieceRotY(Pieces, RoofColor, 20.0f, -88.0f, 0.0f, -90.0f, "3037", Step); // Slope 2x4
	Step++;

	// Roof ridge tiles & chimney: Y = -112 (top Y = -136)
	AddPieceRotY(Pieces, RoofColor, 0.0f, -112.0f, -20.0f, 90.0f, "3023", Step);
	AddPieceRotY(Pieces, RoofColor, 0.0f, -112.0f, 20.0f, 90.0f, "3023", Step);
	// Chimney
	AddPieceRotY(Pieces, ChimneyColor, 0.0f, -112.0f, 0.0f, 0.0f, "3005", Step);
	AddPieceRotY(Pieces, COLOR_LIGHT_GRAY, 0.0f, -136.0f, 0.0f, 0.0f, "3062b", Step); // Smoke puff

	Res.Success = true;
	Res.BrickCount = static_cast<int>(Pieces.size());
	Res.StepCount = Step;
	Res.Pieces = Pieces;
	for (const auto& p : Pieces)
		Res.PartCounts[GetPartDescription(p.PartId)]++;

	Res.Explanation = QString("Designed a charming LEGO house with an authentic interlocking brick structure. "
	                          "Includes a green front lawn plate, recessed doorway, transparent glass windows, "
	                          "cornice eaves, a dual-sloped gabled roof, and a rooftop chimney with smoke.");
	Res.LDrawContent = SerializeLDraw(Res.Title, Pieces, Options.StepByStep);
	return Res;
}

// -------------------------------------------------------------
// 2. CAR / VEHICLE BUILDER
// -------------------------------------------------------------
lcAIModelResult lcAIGenerator::BuildCar(const QString& Prompt, const lcAIOptions& Options)
{
	lcAIModelResult Res;
	Res.Title = "Sports Car with Aerodynamic Wing";

	int BodyColor = ResolveColor(Prompt, COLOR_YELLOW);
	int ChassisColor = COLOR_BLACK;
	int WindshieldColor = COLOR_TRANS_CLEAR;
	int InteriorColor = COLOR_DARK_GRAY;

	std::vector<lcAIModelPiece> Pieces;
	int Step = 1;

	// Step 1: Chassis Base (2x8 / 2x6 plates)
	// Center at (0, 0, 0), length along Z from -70 to +70
	AddPieceRotY(Pieces, ChassisColor, 0.0f, 0.0f, 0.0f, 0.0f, "3020", Step); // 2x4 plate center
	AddPieceRotY(Pieces, ChassisColor, 0.0f, 0.0f, -50.0f, 0.0f, "3020", Step); // front
	AddPieceRotY(Pieces, ChassisColor, 0.0f, 0.0f, 50.0f, 0.0f, "3020", Step); // rear
	Step++;

	// Step 2: Wheels and Axles (4 wheels at corners)
	AddPieceRotY(Pieces, COLOR_DARK_GRAY, -30.0f, 0.0f, -40.0f, 90.0f, "6014b", Step); // Front Left
	AddPieceRotY(Pieces, COLOR_DARK_GRAY, 30.0f, 0.0f, -40.0f, -90.0f, "6014b", Step); // Front Right
	AddPieceRotY(Pieces, COLOR_DARK_GRAY, -30.0f, 0.0f, 40.0f, 90.0f, "6014b", Step); // Rear Left
	AddPieceRotY(Pieces, COLOR_DARK_GRAY, 30.0f, 0.0f, 40.0f, -90.0f, "6014b", Step); // Rear Right
	Step++;

	// Step 3: Front & Rear Bumpers: Y = -8
	// Front bumper & Headlights
	AddPieceRotY(Pieces, ChassisColor, 0.0f, -8.0f, -70.0f, 90.0f, "3710", Step); // 1x4 plate front
	AddPieceRotY(Pieces, COLOR_TRANS_YELLOW, -15.0f, -16.0f, -70.0f, 0.0f, "3024", Step); // Left headlight
	AddPieceRotY(Pieces, COLOR_TRANS_YELLOW, 15.0f, -16.0f, -70.0f, 0.0f, "3024", Step); // Right headlight
	// Rear bumper & Taillights
	AddPieceRotY(Pieces, ChassisColor, 0.0f, -8.0f, 70.0f, 90.0f, "3710", Step);
	AddPieceRotY(Pieces, COLOR_TRANS_RED, -15.0f, -16.0f, 70.0f, 0.0f, "3024", Step); // Left taillight
	AddPieceRotY(Pieces, COLOR_TRANS_RED, 15.0f, -16.0f, 70.0f, 0.0f, "3024", Step); // Right taillight
	Step++;

	// Step 4: Hood and Cockpit Base: Y = -16
	// Front hood
	AddPieceRotY(Pieces, BodyColor, 0.0f, -16.0f, -40.0f, 90.0f, "3020", Step); // 2x4 plate
	// Driver Cockpit floor & Steering wheel
	AddPieceRotY(Pieces, InteriorColor, 0.0f, -16.0f, 0.0f, 0.0f, "3022", Step); // 2x2 plate
	AddPieceRotY(Pieces, COLOR_BLACK, 0.0f, -24.0f, -10.0f, 0.0f, "3829c01", Step); // Steering wheel
	// Rear deck
	AddPieceRotY(Pieces, BodyColor, 0.0f, -16.0f, 40.0f, 90.0f, "3020", Step);
	Step++;

	// Step 5: Aerodynamic Sloped Windshield & Cabin: Y = -24
	// Front sloped hood
	AddPieceRotY(Pieces, BodyColor, 0.0f, -32.0f, -40.0f, 0.0f, "3039", Step); // 2x2 slope
	// Sloped Windshield
	AddPieceRotY(Pieces, WindshieldColor, 0.0f, -40.0f, -15.0f, 0.0f, "3039", Step);
	// Cabin Roof
	AddPieceRotY(Pieces, BodyColor, 0.0f, -48.0f, 10.0f, 0.0f, "3022", Step);
	// Side doors
	AddPieceRotY(Pieces, BodyColor, -15.0f, -32.0f, 10.0f, 0.0f, "3004", Step);
	AddPieceRotY(Pieces, BodyColor, 15.0f, -32.0f, 10.0f, 0.0f, "3004", Step);
	Step++;

	// Step 6: Rear Aerodynamic Racing Spoiler / Wing
	AddPieceRotY(Pieces, COLOR_BLACK, -10.0f, -24.0f, 50.0f, 0.0f, "3005", Step); // Left strut
	AddPieceRotY(Pieces, COLOR_BLACK, 10.0f, -24.0f, 50.0f, 0.0f, "3005", Step); // Right strut
	AddPieceRotY(Pieces, BodyColor, 0.0f, -48.0f, 50.0f, 90.0f, "3710", Step); // 1x4 wing plate
	AddPieceRotY(Pieces, COLOR_BLACK, -25.0f, -56.0f, 50.0f, 0.0f, "3024", Step); // Winglet
	AddPieceRotY(Pieces, COLOR_BLACK, 25.0f, -56.0f, 50.0f, 0.0f, "3024", Step); // Winglet

	Res.Success = true;
	Res.BrickCount = static_cast<int>(Pieces.size());
	Res.StepCount = Step;
	Res.Pieces = Pieces;
	for (const auto& p : Pieces)
		Res.PartCounts[GetPartDescription(p.PartId)]++;

	Res.Explanation = QString("Engineered a sleek 4-wheel sports car with authentic LEGO styling. "
	                          "Featuring low-profile tires on 4 wheel wells, front grille bumper with twin headlights, "
	                          "driver cockpit with a steering wheel, sloped windshield, and an aerodynamic rear spoiler.");
	Res.LDrawContent = SerializeLDraw(Res.Title, Pieces, Options.StepByStep);
	return Res;
}

// -------------------------------------------------------------
// 3. ROCKET BUILDER
// -------------------------------------------------------------
lcAIModelResult lcAIGenerator::BuildRocket(const QString& Prompt, const lcAIOptions& Options)
{
	lcAIModelResult Res;
	Res.Title = "Multi-Stage Space Exploration Rocket";

	int BodyColor = ResolveColor(Prompt, COLOR_WHITE);
	int AccentColor = COLOR_RED;
	int FlameColor = COLOR_TRANS_YELLOW;

	std::vector<lcAIModelPiece> Pieces;
	int Step = 1;

	// Launch Thruster & Exhaust Flame: Y = 0 to 24
	AddPieceRotY(Pieces, FlameColor, 0.0f, 24.0f, 0.0f, 0.0f, "4589", Step); // Flame cone
	AddPieceRotY(Pieces, COLOR_BLACK, 0.0f, 0.0f, 0.0f, 0.0f, "3941", Step); // Engine nozzle (round 2x2)
	Step++;

	// Stage 1 Base & Stabilizer Fins: Y = -24
	AddPieceRotY(Pieces, AccentColor, 0.0f, -24.0f, 0.0f, 0.0f, "3941", Step); // Round 2x2
	// 4 Stabilizer fins radiating outwards (+X, -X, +Z, -Z)
	AddPieceRotY(Pieces, AccentColor, 25.0f, -24.0f, 0.0f, -90.0f, "3039", Step); // Slope 2x2 East
	AddPieceRotY(Pieces, AccentColor, -25.0f, -24.0f, 0.0f, 90.0f, "3039", Step); // Slope 2x2 West
	AddPieceRotY(Pieces, AccentColor, 0.0f, -24.0f, 25.0f, 180.0f, "3039", Step); // Slope 2x2 South
	AddPieceRotY(Pieces, AccentColor, 0.0f, -24.0f, -25.0f, 0.0f, "3039", Step); // Slope 2x2 North
	Step++;

	// Stage 1 Main Fuselage: Y = -48 to -96
	AddPieceRotY(Pieces, BodyColor, 0.0f, -48.0f, 0.0f, 0.0f, "3941", Step);
	AddPieceRotY(Pieces, BodyColor, 0.0f, -72.0f, 0.0f, 0.0f, "3941", Step);
	AddPieceRotY(Pieces, BodyColor, 0.0f, -96.0f, 0.0f, 0.0f, "3941", Step);
	Step++;

	// Interstage Decoupler Ring: Y = -120
	AddPieceRotY(Pieces, COLOR_BLACK, 0.0f, -120.0f, 0.0f, 0.0f, "3022", Step); // Black 2x2 plate
	Step++;

	// Stage 2 Booster: Y = -128 to -152
	AddPieceRotY(Pieces, BodyColor, 0.0f, -144.0f, 0.0f, 0.0f, "3941", Step);
	// Porthole / Command module window
	AddPieceRotY(Pieces, COLOR_TRANS_DARK_BLUE, 0.0f, -168.0f, 0.0f, 0.0f, "3062b", Step);
	Step++;

	// Aerodynamic Nose Cone Fairing: Y = -192 to -240
	AddPieceRotY(Pieces, AccentColor, 0.0f, -192.0f, 0.0f, 0.0f, "3942", Step); // Cone 2x2 (height 48)
	AddPieceRotY(Pieces, COLOR_WHITE, 0.0f, -240.0f, 0.0f, 0.0f, "4589", Step); // Cone 1x1 pinnacle

	Res.Success = true;
	Res.BrickCount = static_cast<int>(Pieces.size());
	Res.StepCount = Step;
	Res.Pieces = Pieces;
	for (const auto& p : Pieces)
		Res.PartCounts[GetPartDescription(p.PartId)]++;

	Res.Explanation = QString("Constructed a multi-stage orbital rocket with vertical symmetry. "
	                          "Featuring an exhaust thrust plume, engine bells, 4 aerodynamic delta stabilizing fins, "
	                          "cylindrical fuel tank stages, a black stage decoupler ring, and a tapered command nose cone.");
	Res.LDrawContent = SerializeLDraw(Res.Title, Pieces, Options.StepByStep);
	return Res;
}

// -------------------------------------------------------------
// 4. SPACESHIP / STARFIGHTER BUILDER
// -------------------------------------------------------------
lcAIModelResult lcAIGenerator::BuildSpaceship(const QString& Prompt, const lcAIOptions& Options)
{
	lcAIModelResult Res;
	Res.Title = "Futuristic Delta-Wing Starfighter";

	int HullColor = ResolveColor(Prompt, COLOR_LIGHT_BLUISH_GRAY);
	int WingColor = Prompt.toLower().contains("blue") ? COLOR_BLUE : COLOR_DARK_RED;
	int CanopyColor = COLOR_TRANS_DARK_BLUE;

	std::vector<lcAIModelPiece> Pieces;
	int Step = 1;

	// Central Fuselage spine: Y = 0
	AddPieceRotY(Pieces, HullColor, 0.0f, 0.0f, 0.0f, 0.0f, "3020", Step); // 2x4 plate center
	AddPieceRotY(Pieces, HullColor, 0.0f, 0.0f, -40.0f, 0.0f, "3020", Step); // front
	AddPieceRotY(Pieces, HullColor, 0.0f, 0.0f, 40.0f, 0.0f, "3020", Step); // rear
	Step++;

	// Dual Delta Wings expanding outwards in X: Y = 0
	AddPieceRotY(Pieces, WingColor, -40.0f, 0.0f, 20.0f, 0.0f, "3035", Step); // Left 4x8 wing plate
	AddPieceRotY(Pieces, WingColor, 40.0f, 0.0f, 20.0f, 0.0f, "3035", Step); // Right 4x8 wing plate
	// Wingtip Laser Cannons
	AddPieceRotY(Pieces, COLOR_TRANS_RED, -70.0f, -8.0f, -20.0f, 0.0f, "4589", Step); // Left laser
	AddPieceRotY(Pieces, COLOR_TRANS_RED, 70.0f, -8.0f, -20.0f, 0.0f, "4589", Step); // Right laser
	Step++;

	// Cockpit & Upper Hull: Y = -8 to -32
	AddPieceRotY(Pieces, HullColor, 0.0f, -8.0f, -20.0f, 0.0f, "3003", Step); // 2x2 brick forward
	AddPieceRotY(Pieces, CanopyColor, 0.0f, -32.0f, -20.0f, 0.0f, "3039", Step); // Sloped cockpit canopy
	AddPieceRotY(Pieces, HullColor, 0.0f, -8.0f, 20.0f, 0.0f, "3003", Step); // 2x2 brick aft
	Step++;

	// Twin Ion Thruster Engines at rear: Z = 60
	AddPieceRotY(Pieces, COLOR_BLACK, -20.0f, -8.0f, 60.0f, 0.0f, "3941", Step); // Left engine
	AddPieceRotY(Pieces, COLOR_BLACK, 20.0f, -8.0f, 60.0f, 0.0f, "3941", Step); // Right engine
	AddPieceRotY(Pieces, COLOR_TRANS_DARK_BLUE, -20.0f, -8.0f, 80.0f, 0.0f, "4589", Step); // Left exhaust glow
	AddPieceRotY(Pieces, COLOR_TRANS_DARK_BLUE, 20.0f, -8.0f, 80.0f, 0.0f, "4589", Step); // Right exhaust glow
	Step++;

	// Vertical Stabilizer Fin: Y = -32 to -56
	AddPieceRotY(Pieces, WingColor, 0.0f, -40.0f, 40.0f, 90.0f, "3040", Step); // Slope 45
	AddPieceRotY(Pieces, WingColor, 0.0f, -64.0f, 40.0f, 0.0f, "3024", Step); // Fin crest

	Res.Success = true;
	Res.BrickCount = static_cast<int>(Pieces.size());
	Res.StepCount = Step;
	Res.Pieces = Pieces;
	for (const auto& p : Pieces)
		Res.PartCounts[GetPartDescription(p.PartId)]++;

	Res.Explanation = QString("Built a futuristic sci-fi starfighter featuring swept delta wings with wingtip plasma lasers, "
	                          "an aerodynamic cockpit canopy in transparent dark blue, a dorsal stabilizer fin, "
	                          "and dual rear-mounted ion thrusters with glowing exhaust trails.");
	Res.LDrawContent = SerializeLDraw(Res.Title, Pieces, Options.StepByStep);
	return Res;
}

// -------------------------------------------------------------
// 5. CASTLE TOWER BUILDER
// -------------------------------------------------------------
lcAIModelResult lcAIGenerator::BuildCastleTower(const QString& Prompt, const lcAIOptions& Options)
{
	lcAIModelResult Res;
	Res.Title = "Medieval Fortress Watchtower";

	int StoneColor = ResolveColor(Prompt, COLOR_LIGHT_BLUISH_GRAY);
	int DarkStone = COLOR_DARK_BLUISH_GRAY;
	int FlagColor = COLOR_RED;

	std::vector<lcAIModelPiece> Pieces;
	int Step = 1;

	// Foundation Plinth: Y = 0 (4x4 stone plate)
	AddPieceRotY(Pieces, DarkStone, 0.0f, 0.0f, 0.0f, 0.0f, "3031", Step);
	Step++;

	// Tower Shaft Tier 1: Y = -24 (top Y = -48)
	// 4 wall bricks surrounding an arched gateway
	AddPieceRotY(Pieces, StoneColor, 0.0f, -24.0f, -20.0f, 90.0f, "3004", Step); // North
	AddPieceRotY(Pieces, StoneColor, -20.0f, -24.0f, 0.0f, 0.0f, "3004", Step); // West
	AddPieceRotY(Pieces, StoneColor, 20.0f, -24.0f, 0.0f, 0.0f, "3004", Step); // East
	AddPieceRotY(Pieces, COLOR_BROWN, 0.0f, -24.0f, 20.0f, 90.0f, "3023", Step); // Wooden Gate threshold
	Step++;

	// Tower Shaft Tier 2: Y = -48 (top Y = -72)
	// Arrow slits
	AddPieceRotY(Pieces, StoneColor, 0.0f, -48.0f, -20.0f, 90.0f, "3004", Step);
	AddPieceRotY(Pieces, StoneColor, -20.0f, -48.0f, 0.0f, 0.0f, "3005", Step); // Left slit
	AddPieceRotY(Pieces, StoneColor, 20.0f, -48.0f, 0.0f, 0.0f, "3005", Step); // Right slit
	AddPieceRotY(Pieces, StoneColor, 0.0f, -48.0f, 20.0f, 90.0f, "3004", Step); // Gate lintel
	Step++;

	// Tower Shaft Tier 3 (Decorative corbel band): Y = -72 (top Y = -96)
	AddPieceRotY(Pieces, DarkStone, -20.0f, -72.0f, -20.0f, 0.0f, "3003", Step);
	AddPieceRotY(Pieces, DarkStone, 20.0f, -72.0f, -20.0f, 0.0f, "3003", Step);
	AddPieceRotY(Pieces, DarkStone, -20.0f, -72.0f, 20.0f, 0.0f, "3003", Step);
	AddPieceRotY(Pieces, DarkStone, 20.0f, -72.0f, 20.0f, 0.0f, "3003", Step);
	Step++;

	// Battlement Platform: Y = -96 (top Y = -104)
	AddPieceRotY(Pieces, StoneColor, 0.0f, -96.0f, 0.0f, 0.0f, "3031", Step); // 4x4 platform
	Step++;

	// Crenellations / Merlons: Y = -104 (height 24, top Y = -128)
	// Alternating battlements at the 4 corners
	AddPieceRotY(Pieces, StoneColor, -30.0f, -104.0f, -30.0f, 0.0f, "3005", Step); // NW merlon
	AddPieceRotY(Pieces, StoneColor, 30.0f, -104.0f, -30.0f, 0.0f, "3005", Step); // NE merlon
	AddPieceRotY(Pieces, StoneColor, -30.0f, -104.0f, 30.0f, 0.0f, "3005", Step); // SW merlon
	AddPieceRotY(Pieces, StoneColor, 30.0f, -104.0f, 30.0f, 0.0f, "3005", Step); // SE merlon
	// Center Flagpole & Banner
	AddPieceRotY(Pieces, COLOR_BROWN, 0.0f, -128.0f, 0.0f, 0.0f, "3062b", Step); // Flagpole base
	AddPieceRotY(Pieces, COLOR_BROWN, 0.0f, -152.0f, 0.0f, 0.0f, "3062b", Step); // Flagpole staff
	AddPieceRotY(Pieces, FlagColor, 10.0f, -152.0f, 0.0f, 0.0f, "3023", Step); // Royal banner

	Res.Success = true;
	Res.BrickCount = static_cast<int>(Pieces.size());
	Res.StepCount = Step;
	Res.Pieces = Pieces;
	for (const auto& p : Pieces)
		Res.PartCounts[GetPartDescription(p.PartId)]++;

	Res.Explanation = QString("Erected a sturdy medieval fortress watchtower in weathered stone grey. "
	                          "Featuring a reinforced plinth, reinforced arched timber gate, arrow slit embrasures, "
	                          "machicolated parapet walk, defensive corner crenellations, and an imperial heraldic banner.");
	Res.LDrawContent = SerializeLDraw(Res.Title, Pieces, Options.StepByStep);
	return Res;
}

// -------------------------------------------------------------
// 6. PYRAMID BUILDER
// -------------------------------------------------------------
lcAIModelResult lcAIGenerator::BuildPyramid(const QString& Prompt, const lcAIOptions& Options)
{
	lcAIModelResult Res;
	Res.Title = "Monumental Egyptian Pyramid";

	int SandColor = ResolveColor(Prompt, COLOR_TAN);
	int CapstoneColor = COLOR_YELLOW; // Golden capstone

	std::vector<lcAIModelPiece> Pieces;
	int Step = 1;

	// Tier 1 (Base): 4x4 arrangement of 2x4 bricks -> 160x160 LDU
	// 4 rows along Z at -60, -20, 20, 60
	for (float z = -60.0f; z <= 60.0f; z += 40.0f)
	{
		AddPieceRotY(Pieces, SandColor, -40.0f, 0.0f, z, 90.0f, "3001", Step);
		AddPieceRotY(Pieces, SandColor, 40.0f, 0.0f, z, 90.0f, "3001", Step);
	}
	Step++;

	// Tier 2: 120x120 LDU at Y = -24
	for (float z = -40.0f; z <= 40.0f; z += 40.0f)
	{
		AddPieceRotY(Pieces, SandColor, -20.0f, -24.0f, z, 90.0f, "3001", Step);
		AddPieceRotY(Pieces, SandColor, 20.0f, -24.0f, z, 90.0f, "3001", Step);
	}
	Step++;

	// Tier 3: 80x80 LDU at Y = -48
	AddPieceRotY(Pieces, SandColor, 0.0f, -48.0f, -20.0f, 90.0f, "3001", Step);
	AddPieceRotY(Pieces, SandColor, 0.0f, -48.0f, 20.0f, 90.0f, "3001", Step);
	Step++;

	// Tier 4: 40x40 LDU at Y = -72
	AddPieceRotY(Pieces, SandColor, 0.0f, -72.0f, 0.0f, 0.0f, "3003", Step); // 2x2 brick
	Step++;

	// Tier 5: Golden Pyramidion Capstone at Y = -96
	AddPieceRotY(Pieces, CapstoneColor, 0.0f, -96.0f, 0.0f, 0.0f, "3039", Step); // Slope apex

	Res.Success = true;
	Res.BrickCount = static_cast<int>(Pieces.size());
	Res.StepCount = Step;
	Res.Pieces = Pieces;
	for (const auto& p : Pieces)
		Res.PartCounts[GetPartDescription(p.PartId)]++;

	Res.Explanation = QString("Constructed a stepped Egyptian pyramid with authentic mathematical proportions. "
	                          "Consisting of four concentric interlocking sandstone courses tapering gracefully to an apex, "
	                          "crowned with a polished golden pyramidion capstone.");
	Res.LDrawContent = SerializeLDraw(Res.Title, Pieces, Options.StepByStep);
	return Res;
}

// -------------------------------------------------------------
// 7. TREE / NATURE BUILDER
// -------------------------------------------------------------
lcAIModelResult lcAIGenerator::BuildTree(const QString& Prompt, const lcAIOptions& Options)
{
	lcAIModelResult Res;
	Res.Title = "Lush Evergreen Pine Tree";

	int TrunkColor = COLOR_BROWN;
	int FoliageColor = ResolveColor(Prompt, COLOR_DARK_GREEN);
	if (Prompt.toLower().contains("bright") || Prompt.toLower().contains("lime"))
		FoliageColor = COLOR_BRIGHT_GREEN;

	std::vector<lcAIModelPiece> Pieces;
	int Step = 1;

	// Forest Ground Mound: Y = 0
	AddPieceRotY(Pieces, COLOR_GREEN, 0.0f, 0.0f, 0.0f, 0.0f, "3031", Step); // 4x4 grass plate
	Step++;

	// Tree Trunk: Round 2x2 bricks stacked vertically
	AddPieceRotY(Pieces, TrunkColor, 0.0f, -24.0f, 0.0f, 0.0f, "3941", Step);
	AddPieceRotY(Pieces, TrunkColor, 0.0f, -48.0f, 0.0f, 0.0f, "3941", Step);
	Step++;

	// Canopy Tier 1 (Broad lower branch layer): Y = -72
	// 4 large slopes facing outwards
	AddPieceRotY(Pieces, FoliageColor, 20.0f, -72.0f, 0.0f, -90.0f, "3037", Step); // East
	AddPieceRotY(Pieces, FoliageColor, -20.0f, -72.0f, 0.0f, 90.0f, "3037", Step); // West
	AddPieceRotY(Pieces, FoliageColor, 0.0f, -72.0f, 20.0f, 180.0f, "3037", Step); // South
	AddPieceRotY(Pieces, FoliageColor, 0.0f, -72.0f, -20.0f, 0.0f, "3037", Step); // North
	Step++;

	// Canopy Tier 2 (Mid branches): Y = -96
	AddPieceRotY(Pieces, FoliageColor, 10.0f, -96.0f, 0.0f, -90.0f, "3039", Step);
	AddPieceRotY(Pieces, FoliageColor, -10.0f, -96.0f, 0.0f, 90.0f, "3039", Step);
	AddPieceRotY(Pieces, FoliageColor, 0.0f, -96.0f, 10.0f, 180.0f, "3039", Step);
	AddPieceRotY(Pieces, FoliageColor, 0.0f, -96.0f, -10.0f, 0.0f, "3039", Step);
	Step++;

	// Canopy Tier 3 (Tapered Tree Tip): Y = -120 to -144
	AddPieceRotY(Pieces, FoliageColor, 0.0f, -120.0f, 0.0f, 0.0f, "3003", Step); // 2x2 brick center
	AddPieceRotY(Pieces, FoliageColor, 0.0f, -144.0f, 0.0f, 0.0f, "3942", Step); // Cone 2x2 tip

	Res.Success = true;
	Res.BrickCount = static_cast<int>(Pieces.size());
	Res.StepCount = Step;
	Res.Pieces = Pieces;
	for (const auto& p : Pieces)
		Res.PartCounts[GetPartDescription(p.PartId)]++;

	Res.Explanation = QString("Created a realistic botanical pine tree on a grassy forest base. "
	                          "Featuring a round timber trunk, cascading three-tier sloped foliage canopy, "
	                          "and an organic coniferous spire crown.");
	Res.LDrawContent = SerializeLDraw(Res.Title, Pieces, Options.StepByStep);
	return Res;
}

// -------------------------------------------------------------
// 8. BOAT / MARITIME BUILDER
// -------------------------------------------------------------
lcAIModelResult lcAIGenerator::BuildBoat(const QString& Prompt, const lcAIOptions& Options)
{
	lcAIModelResult Res;
	Res.Title = "Speedboat with Outboard Motor";

	int HullColor = ResolveColor(Prompt, COLOR_WHITE);
	int TrimColor = COLOR_BLUE;

	std::vector<lcAIModelPiece> Pieces;
	int Step = 1;

	// Hull Keel: Y = 0
	AddPieceRotY(Pieces, HullColor, 0.0f, 0.0f, 0.0f, 0.0f, "3035", Step); // 4x8 base plate
	Step++;

	// Tapered Bow (Front): Y = -8
	AddPieceRotY(Pieces, TrimColor, 0.0f, -8.0f, -90.0f, 0.0f, "3039", Step); // Bow point
	AddPieceRotY(Pieces, TrimColor, -20.0f, -8.0f, -70.0f, 45.0f, "3039", Step);
	AddPieceRotY(Pieces, TrimColor, 20.0f, -8.0f, -70.0f, -45.0f, "3039", Step);
	Step++;

	// Gunwales (Sides) & Cockpit: Y = -24
	AddPieceRotY(Pieces, HullColor, -30.0f, -24.0f, 0.0f, 0.0f, "3008", Step); // Port side
	AddPieceRotY(Pieces, HullColor, 30.0f, -24.0f, 0.0f, 0.0f, "3008", Step); // Starboard side
	AddPieceRotY(Pieces, COLOR_TRANS_CLEAR, 0.0f, -32.0f, -30.0f, 0.0f, "3039", Step); // Windscreen
	AddPieceRotY(Pieces, COLOR_BLACK, 0.0f, -24.0f, -10.0f, 0.0f, "3829c01", Step); // Helm wheel
	Step++;

	// Stern & Outboard Motor: Y = -24 to -48
	AddPieceRotY(Pieces, HullColor, 0.0f, -24.0f, 70.0f, 90.0f, "3010", Step); // Transom
	AddPieceRotY(Pieces, COLOR_DARK_GRAY, 0.0f, -32.0f, 90.0f, 0.0f, "3003", Step); // Engine casing
	AddPieceRotY(Pieces, COLOR_BLACK, 0.0f, -48.0f, 90.0f, 0.0f, "3062b", Step); // Propeller shaft

	Res.Success = true;
	Res.BrickCount = static_cast<int>(Pieces.size());
	Res.StepCount = Step;
	Res.Pieces = Pieces;
	for (const auto& p : Pieces)
		Res.PartCounts[GetPartDescription(p.PartId)]++;

	Res.Explanation = QString("Built a maritime vessel featuring a hydro-dynamic tapered bow, "
	                          "open cockpit with a helm steering wheel, protective wraparound windshield, "
	                          "and a rear-mounted twin-stroke outboard marine engine.");
	Res.LDrawContent = SerializeLDraw(Res.Title, Pieces, Options.StepByStep);
	return Res;
}

// -------------------------------------------------------------
// 9. ROBOT BUILDER
// -------------------------------------------------------------
lcAIModelResult lcAIGenerator::BuildRobot(const QString& Prompt, const lcAIOptions& Options)
{
	lcAIModelResult Res;
	Res.Title = "Bipedal Autonomous Android Robot";

	int ArmorColor = ResolveColor(Prompt, COLOR_LIGHT_BLUISH_GRAY);
	int CoreColor = COLOR_TRANS_YELLOW;

	std::vector<lcAIModelPiece> Pieces;
	int Step = 1;

	// Feet Pads: Y = 0
	AddPieceRotY(Pieces, COLOR_DARK_GRAY, -25.0f, 0.0f, 0.0f, 0.0f, "3022", Step); // Left foot
	AddPieceRotY(Pieces, COLOR_DARK_GRAY, 25.0f, 0.0f, 0.0f, 0.0f, "3022", Step); // Right foot
	Step++;

	// Articulated Legs: Y = -24 to -48
	AddPieceRotY(Pieces, ArmorColor, -25.0f, -24.0f, 0.0f, 0.0f, "3005", Step); // Left shin
	AddPieceRotY(Pieces, ArmorColor, 25.0f, -24.0f, 0.0f, 0.0f, "3005", Step); // Right shin
	AddPieceRotY(Pieces, ArmorColor, -25.0f, -48.0f, 0.0f, 0.0f, "3005", Step); // Left thigh
	AddPieceRotY(Pieces, ArmorColor, 25.0f, -48.0f, 0.0f, 0.0f, "3005", Step); // Right thigh
	Step++;

	// Pelvis & Torso Core: Y = -72 to -96
	AddPieceRotY(Pieces, COLOR_BLACK, 0.0f, -72.0f, 0.0f, 90.0f, "3020", Step); // Hip girdle plate
	AddPieceRotY(Pieces, ArmorColor, 0.0f, -96.0f, 0.0f, 90.0f, "3001", Step); // Chest chassis
	AddPieceRotY(Pieces, CoreColor, 0.0f, -96.0f, 15.0f, 0.0f, "3024", Step); // Arc reactor power core
	Step++;

	// Arms & Gripper Claws: Y = -96 to -72
	AddPieceRotY(Pieces, ArmorColor, -35.0f, -96.0f, 0.0f, 0.0f, "3005", Step); // Left shoulder
	AddPieceRotY(Pieces, ArmorColor, 35.0f, -96.0f, 0.0f, 0.0f, "3005", Step); // Right shoulder
	AddPieceRotY(Pieces, COLOR_DARK_GRAY, -35.0f, -72.0f, 10.0f, 0.0f, "3023", Step); // Left hand claw
	AddPieceRotY(Pieces, COLOR_DARK_GRAY, 35.0f, -72.0f, 10.0f, 0.0f, "3023", Step); // Right hand claw
	Step++;

	// Robot Head with Optical Visor & Antenna: Y = -120 to -144
	AddPieceRotY(Pieces, ArmorColor, 0.0f, -120.0f, 0.0f, 0.0f, "3003", Step); // Head block
	AddPieceRotY(Pieces, COLOR_TRANS_RED, 0.0f, -120.0f, 15.0f, 90.0f, "3023", Step); // Optical scanner visor
	AddPieceRotY(Pieces, COLOR_YELLOW, 0.0f, -144.0f, 0.0f, 0.0f, "4589", Step); // Sensor antenna spire

	Res.Success = true;
	Res.BrickCount = static_cast<int>(Pieces.size());
	Res.StepCount = Step;
	Res.Pieces = Pieces;
	for (const auto& p : Pieces)
		Res.PartCounts[GetPartDescription(p.PartId)]++;

	Res.Explanation = QString("Engineered an articulated humanoid android mecha. "
	                          "Featuring weighted stabilization foot plates, two-tier bipedal legs, a central fusion power reactor, "
	                          "dual shoulder mounts with manipulator claws, a scanning visor sensor, and a cranial communications antenna.");
	Res.LDrawContent = SerializeLDraw(Res.Title, Pieces, Options.StepByStep);
	return Res;
}

// -------------------------------------------------------------
// 10. AIRPLANE BUILDER
// -------------------------------------------------------------
lcAIModelResult lcAIGenerator::BuildAirplane(const QString& Prompt, const lcAIOptions& Options)
{
	lcAIModelResult Res;
	Res.Title = "Twin-Turbine Jet Airliner";

	int FuselageColor = ResolveColor(Prompt, COLOR_WHITE);
	int WingColor = COLOR_BLUE;

	std::vector<lcAIModelPiece> Pieces;
	int Step = 1;

	// Fuselage Spine: Y = 0
	AddPieceRotY(Pieces, FuselageColor, 0.0f, 0.0f, -60.0f, 0.0f, "3020", Step); // Nose
	AddPieceRotY(Pieces, FuselageColor, 0.0f, 0.0f, -20.0f, 0.0f, "3020", Step); // Mid
	AddPieceRotY(Pieces, FuselageColor, 0.0f, 0.0f, 20.0f, 0.0f, "3020", Step); // Center
	AddPieceRotY(Pieces, FuselageColor, 0.0f, 0.0f, 60.0f, 0.0f, "3020", Step); // Tail
	Step++;

	// Main Wings (Wide wingspan): Y = 0
	AddPieceRotY(Pieces, WingColor, -50.0f, 0.0f, 0.0f, 0.0f, "3035", Step); // Left wing (4x8)
	AddPieceRotY(Pieces, WingColor, 50.0f, 0.0f, 0.0f, 0.0f, "3035", Step); // Right wing (4x8)
	// Under-wing Jet Turbine Engines
	AddPieceRotY(Pieces, COLOR_LIGHT_BLUISH_GRAY, -40.0f, 16.0f, 0.0f, 0.0f, "3941", Step); // Left turbine
	AddPieceRotY(Pieces, COLOR_LIGHT_BLUISH_GRAY, 40.0f, 16.0f, 0.0f, 0.0f, "3941", Step); // Right turbine
	Step++;

	// Cockpit & Upper Cabin: Y = -24
	AddPieceRotY(Pieces, COLOR_TRANS_DARK_BLUE, 0.0f, -24.0f, -50.0f, 0.0f, "3039", Step); // Flight deck windshield
	AddPieceRotY(Pieces, FuselageColor, 0.0f, -24.0f, -10.0f, 0.0f, "3001", Step); // Passenger cabin
	AddPieceRotY(Pieces, FuselageColor, 0.0f, -24.0f, 30.0f, 0.0f, "3001", Step); // Aft cabin
	Step++;

	// Tail Empennage (Vertical Fin & Horizontal Stabilizers): Y = -48 to -72
	AddPieceRotY(Pieces, WingColor, -25.0f, -24.0f, 70.0f, 0.0f, "3023", Step); // Left tail stabilizer
	AddPieceRotY(Pieces, WingColor, 25.0f, -24.0f, 70.0f, 0.0f, "3023", Step); // Right tail stabilizer
	AddPieceRotY(Pieces, WingColor, 0.0f, -48.0f, 60.0f, 90.0f, "3040", Step); // Vertical fin
	AddPieceRotY(Pieces, WingColor, 0.0f, -72.0f, 60.0f, 0.0f, "3024", Step); // Fin tip

	Res.Success = true;
	Res.BrickCount = static_cast<int>(Pieces.size());
	Res.StepCount = Step;
	Res.Pieces = Pieces;
	for (const auto& p : Pieces)
		Res.PartCounts[GetPartDescription(p.PartId)]++;

	Res.Explanation = QString("Built an aerodynamic twin-engine jet aircraft. "
	                          "Featuring a streamlined fuselage, large cantilevered swept wings with underslung turbofan engines, "
	                          "a multi-pane cockpit windshield, and a high T-tail vertical stabilizer.");
	Res.LDrawContent = SerializeLDraw(Res.Title, Pieces, Options.StepByStep);
	return Res;
}

// -------------------------------------------------------------
// 11. FURNITURE (TABLE & CHAIRS) BUILDER
// -------------------------------------------------------------
lcAIModelResult lcAIGenerator::BuildFurniture(const QString& Prompt, const lcAIOptions& Options)
{
	lcAIModelResult Res;
	Res.Title = "Dining Room Table with Surrounding Chairs";

	int WoodColor = ResolveColor(Prompt, COLOR_BROWN);
	int CushionColor = COLOR_RED;

	std::vector<lcAIModelPiece> Pieces;
	int Step = 1;

	// Table 4 legs: Y = 0 (height 24)
	AddPieceRotY(Pieces, WoodColor, -30.0f, 0.0f, -15.0f, 0.0f, "3005", Step);
	AddPieceRotY(Pieces, WoodColor, 30.0f, 0.0f, -15.0f, 0.0f, "3005", Step);
	AddPieceRotY(Pieces, WoodColor, -30.0f, 0.0f, 15.0f, 0.0f, "3005", Step);
	AddPieceRotY(Pieces, WoodColor, 30.0f, 0.0f, 15.0f, 0.0f, "3005", Step);
	Step++;

	// Table Top Surface: Y = -24 (4x8 plate)
	AddPieceRotY(Pieces, WoodColor, 0.0f, -24.0f, 0.0f, 90.0f, "3035", Step);
	Step++;

	// Table Centerpiece / Fruit Bowl: Y = -32
	AddPieceRotY(Pieces, COLOR_YELLOW, 0.0f, -32.0f, 0.0f, 0.0f, "3941", Step);
	AddPieceRotY(Pieces, COLOR_RED, 0.0f, -48.0f, 0.0f, 0.0f, "3062b", Step);
	Step++;

	// Chair 1 (Left): Seat at Y = -16, Backrest at Y = -40
	AddPieceRotY(Pieces, WoodColor, -55.0f, 0.0f, 0.0f, 0.0f, "3003", Step); // Chair leg block
	AddPieceRotY(Pieces, CushionColor, -55.0f, -24.0f, 0.0f, 0.0f, "3022", Step); // Cushion
	AddPieceRotY(Pieces, WoodColor, -65.0f, -48.0f, 0.0f, 0.0f, "3004", Step); // Backrest
	Step++;

	// Chair 2 (Right): Seat at Y = -16, Backrest at Y = -40
	AddPieceRotY(Pieces, WoodColor, 55.0f, 0.0f, 0.0f, 0.0f, "3003", Step);
	AddPieceRotY(Pieces, CushionColor, 55.0f, -24.0f, 0.0f, 0.0f, "3022", Step);
	AddPieceRotY(Pieces, WoodColor, 65.0f, -48.0f, 0.0f, 0.0f, "3004", Step);

	Res.Success = true;
	Res.BrickCount = static_cast<int>(Pieces.size());
	Res.StepCount = Step;
	Res.Pieces = Pieces;
	for (const auto& p : Pieces)
		Res.PartCounts[GetPartDescription(p.PartId)]++;

	Res.Explanation = QString("Assembled an interior furniture dining suite. "
	                          "Featuring a sturdy rectangular timber table supported by four corner pillar legs, "
	                          "a vibrant decorative centerpiece bowl, and two ergonomic upholstered dining chairs.");
	Res.LDrawContent = SerializeLDraw(Res.Title, Pieces, Options.StepByStep);
	return Res;
}

// -------------------------------------------------------------
// 12. BRIDGE BUILDER
// -------------------------------------------------------------
lcAIModelResult lcAIGenerator::BuildBridge(const QString& Prompt, const lcAIOptions& Options)
{
	lcAIModelResult Res;
	Res.Title = "Arching Suspension Bridge";

	int PierColor = ResolveColor(Prompt, COLOR_DARK_BLUISH_GRAY);
	int DeckColor = COLOR_LIGHT_BLUISH_GRAY;

	std::vector<lcAIModelPiece> Pieces;
	int Step = 1;

	// Bridge Support Piers at Left (-60) and Right (+60): Y = 0 to -48
	AddPieceRotY(Pieces, PierColor, -60.0f, 0.0f, 0.0f, 0.0f, "3001", Step);
	AddPieceRotY(Pieces, PierColor, 60.0f, 0.0f, 0.0f, 0.0f, "3001", Step);
	AddPieceRotY(Pieces, PierColor, -60.0f, -24.0f, 0.0f, 0.0f, "3001", Step);
	AddPieceRotY(Pieces, PierColor, 60.0f, -24.0f, 0.0f, 0.0f, "3001", Step);
	Step++;

	// Roadway Deck span: Y = -48
	AddPieceRotY(Pieces, DeckColor, -60.0f, -48.0f, 0.0f, 90.0f, "3035", Step);
	AddPieceRotY(Pieces, DeckColor, 0.0f, -48.0f, 0.0f, 90.0f, "3035", Step);
	AddPieceRotY(Pieces, DeckColor, 60.0f, -48.0f, 0.0f, 90.0f, "3035", Step);
	Step++;

	// Roadside Safety Railings & Suspension Cables: Y = -72 to -96
	for (float x = -80.0f; x <= 80.0f; x += 40.0f)
	{
		AddPieceRotY(Pieces, COLOR_YELLOW, x, -56.0f, -15.0f, 90.0f, "3710", Step); // North rail
		AddPieceRotY(Pieces, COLOR_YELLOW, x, -56.0f, 15.0f, 90.0f, "3710", Step); // South rail
	}
	// Suspension towers
	AddPieceRotY(Pieces, COLOR_RED, -40.0f, -80.0f, 0.0f, 0.0f, "3008", Step);
	AddPieceRotY(Pieces, COLOR_RED, 40.0f, -80.0f, 0.0f, 0.0f, "3008", Step);

	Res.Success = true;
	Res.BrickCount = static_cast<int>(Pieces.size());
	Res.StepCount = Step;
	Res.Pieces = Pieces;
	for (const auto& p : Pieces)
		Res.PartCounts[GetPartDescription(p.PartId)]++;

	Res.Explanation = QString("Constructed an engineered suspension bridge spanning over a water gap. "
	                          "Featuring reinforced abutment masonry piers, an expansive level roadway deck, "
	                          "twin structural suspension towers, and perimeter safety guardrails.");
	Res.LDrawContent = SerializeLDraw(Res.Title, Pieces, Options.StepByStep);
	return Res;
}

// -------------------------------------------------------------
// 13. RAINBOW ARCH BUILDER
// -------------------------------------------------------------
lcAIModelResult lcAIGenerator::BuildRainbowArch(const QString& Prompt, const lcAIOptions& Options)
{
	lcAIModelResult Res;
	Res.Title = "Vibrant Spectrum Rainbow Arch";

	std::vector<lcAIModelPiece> Pieces;
	int Step = 1;

	// Ground Cloud Pedestals (White plates at Left and Right)
	AddPieceRotY(Pieces, COLOR_WHITE, -60.0f, 0.0f, 0.0f, 0.0f, "3031", Step);
	AddPieceRotY(Pieces, COLOR_WHITE, 60.0f, 0.0f, 0.0f, 0.0f, "3031", Step);
	Step++;

	// Rainbow Spectrum Bands: Red, Orange, Yellow, Green, Blue, Magenta
	int Colors[6] = { COLOR_RED, COLOR_ORANGE, COLOR_YELLOW, COLOR_BRIGHT_GREEN, COLOR_BLUE, COLOR_MAGENTA };

	for (int i = 0; i < 6; ++i)
	{
		float Y = -8.0f - (i * 16.0f);
		// Left ascending pillar
		AddPieceRotY(Pieces, Colors[i], -50.0f + (i * 4.0f), Y, 0.0f, 0.0f, "3004", Step);
		// Center span arch
		AddPieceRotY(Pieces, Colors[i], 0.0f, Y - 16.0f, 0.0f, 90.0f, "3001", Step);
		// Right descending pillar
		AddPieceRotY(Pieces, Colors[i], 50.0f - (i * 4.0f), Y, 0.0f, 0.0f, "3004", Step);
		Step++;
	}

	Res.Success = true;
	Res.BrickCount = static_cast<int>(Pieces.size());
	Res.StepCount = Step;
	Res.Pieces = Pieces;
	for (const auto& p : Pieces)
		Res.PartCounts[GetPartDescription(p.PartId)]++;

	Res.Explanation = QString("Sculpted a vibrant 6-color prismatic rainbow archway. "
	                          "Resting on billowing white cumulus cloud bases, with cascading concentric color bands "
	                          "(Red, Orange, Yellow, Green, Blue, and Magenta) meeting at a harmonious central peak.");
	Res.LDrawContent = SerializeLDraw(Res.Title, Pieces, Options.StepByStep);
	return Res;
}

// -------------------------------------------------------------
// 14. CUSTOM SCULPTURE / GENERIC FALLBACK
// -------------------------------------------------------------
lcAIModelResult lcAIGenerator::BuildCustomSculpture(const QString& Prompt, const lcAIOptions& Options)
{
	lcAIModelResult Res;
	Res.Title = "Custom Architectural Creation: " + Prompt.trimmed();

	int PrimaryColor = ResolveColor(Prompt, COLOR_BLUE);
	int SecondaryColor = (PrimaryColor == COLOR_YELLOW) ? COLOR_RED : COLOR_YELLOW;
	int AccentColor = COLOR_WHITE;

	std::vector<lcAIModelPiece> Pieces;
	int Step = 1;

	// Sturdy Pedestal Base: Y = 0 (4x4 plate)
	AddPieceRotY(Pieces, COLOR_DARK_BLUISH_GRAY, 0.0f, 0.0f, 0.0f, 0.0f, "3031", Step);
	Step++;

	// Tier 1: Foundation ring (2x4 bricks)
	AddPieceRotY(Pieces, PrimaryColor, 0.0f, -24.0f, -20.0f, 90.0f, "3001", Step);
	AddPieceRotY(Pieces, PrimaryColor, 0.0f, -24.0f, 20.0f, 90.0f, "3001", Step);
	Step++;

	// Tier 2: Rotated Cross layer
	AddPieceRotY(Pieces, SecondaryColor, -20.0f, -48.0f, 0.0f, 0.0f, "3001", Step);
	AddPieceRotY(Pieces, SecondaryColor, 20.0f, -48.0f, 0.0f, 0.0f, "3001", Step);
	Step++;

	// Tier 3: Sloped Elevation
	AddPieceRotY(Pieces, PrimaryColor, 0.0f, -72.0f, -15.0f, 0.0f, "3039", Step);
	AddPieceRotY(Pieces, PrimaryColor, 0.0f, -72.0f, 15.0f, 180.0f, "3039", Step);
	Step++;

	// Tier 4: Pinnacle Spire
	AddPieceRotY(Pieces, AccentColor, 0.0f, -96.0f, 0.0f, 0.0f, "3003", Step);
	AddPieceRotY(Pieces, COLOR_TRANS_YELLOW, 0.0f, -120.0f, 0.0f, 0.0f, "4589", Step);

	Res.Success = true;
	Res.BrickCount = static_cast<int>(Pieces.size());
	Res.StepCount = Step;
	Res.Pieces = Pieces;
	for (const auto& p : Pieces)
		Res.PartCounts[GetPartDescription(p.PartId)]++;

	Res.Explanation = QString("Custom designed a geometric architectural LEGO model corresponding to '%1'. "
	                          "Featuring an interlocked multi-level foundation, rotational cross bracing, "
	                          "sloped architectural facets, and a gleaming crystal spire.").arg(Prompt);
	Res.LDrawContent = SerializeLDraw(Res.Title, Pieces, Options.StepByStep);
	return Res;
}
