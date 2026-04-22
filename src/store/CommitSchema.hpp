#pragma once

#include <sqlite3.h>

namespace git_editor::commit_schema {

bool execOrLog(sqlite3* db, char const* sql);
bool ensureSchema(sqlite3* db, int schemaVersion);

class DeferredFkTransaction final {
public:
    explicit DeferredFkTransaction(sqlite3* db);
    ~DeferredFkTransaction();

    bool begin();
    bool commit();
    void rollback();

private:
    sqlite3* m_db = nullptr;
    bool m_open = false;
};

} // namespace git_editor::commit_schema
