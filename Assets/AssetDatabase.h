#pragma once
#include <string>
#include "../Core/Guid.h"

class AssetDatabase {
public:
    // Возвращает GUID ассета: если рядом уже лежит .meta — читает его,
    // если нет (ассет встречен впервые) — создаёт новый .meta со свежим GUID.
    static Guid GetOrCreateGuid(const std::string& assetPath, const std::string& assetType);

    // Сканирует папку рекурсивно, создаёт .meta для всех файлов, у которых его
    // ещё нет. Возвращает количество новых .meta, которые были созданы.
    static int ScanFolder(const std::string& folderPath);

private:
    static std::string MetaPathFor(const std::string& assetPath);
};
