#pragma once

#include "filesystem_def.h"
#include <set>

bool DoesBootableExist(const fs::path&);
bool TryRegisterBootable(const fs::path&);
bool TryUpdateLastBootedTime(const fs::path&);
void ScanBootables(const fs::path&, bool = true);
std::set<fs::path> GetActiveBootableDirectories();
void PurgeInexistingFiles();
void FetchGameTitles();
void FetchGameCovers();
