#pragma once

namespace git_editor {

// Called from the worker-thread epilogue when CommitStore is dirty.
// Takes a memory snapshot of the live DB and schedules an async zip write.
// Returns immediately; the zip write races against subsequent calls and the
// version counter ensures only the latest snapshot lands on disk.
void scheduleLocalDbFlush();

// Synchronous final flush. Acquires the zip mutex, snapshots the live DB,
// writes git-editor.db.zip, and (on success with compress-local-database ON)
// deletes the raw .db and WAL files so the save dir is tidy.
// Safe to call from any thread. No-op if compress-local-database is OFF.
void flushLocalDbNow();

} // namespace git_editor
