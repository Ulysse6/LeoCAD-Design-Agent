#pragma once

#include <QString>
#include <QStringList>
#include <vector>

struct lcKBModelEntry
{
	const char* SetNumber;
	const char* Name;
	const char* Theme;
	int Year;
	int PieceCount;
	const char* Categories; // Space-delimited
	const char* Tags;       // Space-delimited
	int DominantColor;
	const char* LDrawData;
};

class lcKBCatalog
{
public:
	static size_t GetModelCount();
	static const lcKBModelEntry* GetModel(size_t Index);
	static const lcKBModelEntry* FindBestMatch(const QString& Prompt, int* OutScore = nullptr);
	static std::vector<const lcKBModelEntry*> Search(const QString& Query, int MaxResults = 5);
};
