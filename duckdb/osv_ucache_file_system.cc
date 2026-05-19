#include "osv_ucache_file_system.hpp"
#include "osv_parquet_file_handle.hpp"

#include <duckdb/common/exception.hpp>
#include <duckdb/common/file_open_flags.hpp>

#include <osv/mmu.hh>

#include <cstring>
#include <algorithm>

namespace osv_duckdb {

// ─────────────────────────────────────────────────────────────────────────────
// enqueue_prefetch — called by OsvCachingFileHandle::RegisterPrefetch()
//
// DuckDB already knows which byte ranges it will need next; this function
// immediately issues async NVMe IO for all pages in [pos, pos+len) that are
// not yet in the cache.  When the page fault arrives later, checkPipeline()
// in uCache::handleFault() finds the buffer in Reading/ReadyToInsert state
// and simply polls for completion rather than starting a new synchronous read.
// ─────────────────────────────────────────────────────────────────────────────
void enqueue_prefetch(ucache::VMA *vma, duckdb::idx_t pos, duckdb::idx_t len) {
    if (len == 0 || ucache::uCacheManager == nullptr) return;

    // Count total in-flight prefetch IOs across all CPUs to avoid saturating the
    // NVMe submission queue.  Only issue up to (prefetch_batch - total_inflight)
    // new IOs per call.
    //
    // per_cpu_inflight_count is a signed int.  It can transiently go negative due
    // to the race between UncachedToPrefetching (which sets the prefetcher field
    // in the PTE, making the buffer visible as Reading) and the fetch_add in
    // prefetch() (which increments the counter a few instructions later).  If a
    // page fault resolves the IO between those two instructions the decrement in
    // ReadyToInsertToCached fires before the increment, yielding -1.  Casting a
    // negative int to u64 wraps to ~2^64, so we clamp each per-CPU value to ≥ 0.
    u64 total_inflight = 0;
    for (size_t i = 0; i < sched::cpus.size(); i++) {
        int v = ucache::uCacheManager->per_cpu_inflight_count[i].load(
                    std::memory_order_relaxed);
        if (v > 0) total_inflight += (u64)v;
    }

    u64 batch_cap = ucache::uCacheManager->prefetch_batch;
    if (total_inflight >= batch_cap) return;  // queue already busy, skip this hint
    u64 max_new = batch_cap - total_inflight;

    duckdb::idx_t first = pos / vma->pageSize;
    duckdb::idx_t last  = (pos + len - 1) / vma->pageSize;

    std::vector<ucache::Buffer *> pl;
    for (duckdb::idx_t i = first; i <= last && i < (duckdb::idx_t)vma->buffers.size(); i++) {
        if (pl.size() >= max_new) break;
        ucache::Buffer *buf = vma->buffers[i];
        ucache::BufferSnapshot bs(vma->nbPages);
        buf->updateSnapshot(&bs);
        if (bs.state == ucache::BufferState::Uncached)
            pl.push_back(buf);
    }
    if (!pl.empty()) {
        ucache::uCacheManager->ensureFreePages(vma->pageSize * pl.size());
        ucache::uCacheManager->prefetch(vma, pl);
    }
}


// ─────────────────────────────────────────────────────────────────────────────
// Internal helpers
// ─────────────────────────────────────────────────────────────────────────────

OsvUCacheFileHandle &OsvUCacheFileSystem::Cast(duckdb::FileHandle &handle) {
    return handle.Cast<OsvUCacheFileHandle>();
}

ucache::VMA *OsvUCacheFileSystem::GetOrCreateVMA(
    const duckdb::string &path,
    duckdb::FileOpenFlags flags,
    duckdb::optional_ptr<duckdb::FileOpener> opener)
{
    // uCacheManager->mmap() is idempotent: it returns the existing VMA if one
    // already exists for this path.  It also creates a local_ufile (LBA table +
    // direct NVMe) and calls switch_to_poll_mode() on first call.
    //
    // We pass the file_size as req_size so mmap() doesn't need to guess.
    // The inner_fs_ stat is cheap because ext4 is only used for metadata.
    auto inner_handle = inner_fs_->OpenFile(path, flags, opener);
    u64 file_size = static_cast<u64>(inner_fs_->GetFileSize(*inner_handle));

    ucache::VMA *vma = ucache::uCacheManager->mmap(
        path.c_str(), file_size, mmu::page_size, nullptr);
    return vma;
}


// ─────────────────────────────────────────────────────────────────────────────
// OsvUCacheFileSystem — file open
// ─────────────────────────────────────────────────────────────────────────────

duckdb::unique_ptr<duckdb::FileHandle>
OsvUCacheFileSystem::OpenFile(const duckdb::string &path,
                              duckdb::FileOpenFlags flags,
                              duckdb::optional_ptr<duckdb::FileOpener> opener)
{
    if (ucache::uCacheManager == nullptr || ucache::uCacheManager->totalPhysSize == 0) {
        throw duckdb::IOException(
            "OsvUCacheFileSystem: cache not initialised — call osv_ucache_init() first");
    }

    std::lock_guard<std::mutex> lk(vma_mu_);
    ucache::VMA *vma = GetOrCreateVMA(path, flags, opener);

    // Keep a thin inner handle for metadata (GetLastModifiedTime, etc.).
    auto inner_handle = inner_fs_->OpenFile(path, flags, opener);
    return duckdb::make_uniq<OsvUCacheFileHandle>(
        *this, path, flags, vma, std::move(inner_handle));
}

duckdb::unique_ptr<OsvCachingFileHandle>
OsvUCacheFileSystem::OpenParquetHandle(
    const duckdb::string &path,
    duckdb::FileOpenFlags flags,
    duckdb::optional_ptr<duckdb::FileOpener> opener)
{
    if (ucache::uCacheManager == nullptr || ucache::uCacheManager->totalPhysSize == 0) {
        throw duckdb::IOException(
            "OsvUCacheFileSystem: cache not initialised — call osv_ucache_init() first");
    }

    std::lock_guard<std::mutex> lk(vma_mu_);
    ucache::VMA *vma = GetOrCreateVMA(path, flags, opener);

    auto inner_handle = inner_fs_->OpenFile(path, flags, opener);
    u64 file_size = static_cast<u64>(vma->file->size);

    return duckdb::make_uniq<OsvCachingFileHandle>(
        vma, static_cast<const char *>(vma->start),
        file_size, path, std::move(inner_handle), *this);
}


void OsvUCacheFileSystem::PreOpen(const duckdb::string &path) {
    std::lock_guard<std::mutex> lk(vma_mu_);
    GetOrCreateVMA(path, duckdb::FileOpenFlags::FILE_FLAGS_READ, nullptr);
}


// ── Cached read path ──────────────────────────────────────────────────────────

void OsvUCacheFileSystem::Read(duckdb::FileHandle &handle, void *buffer,
                               int64_t nr_bytes, duckdb::idx_t location)
{
    auto &h = Cast(handle);
    if (location >= h.file_size || nr_bytes <= 0) {
        return;
    }
    auto actual = static_cast<duckdb::idx_t>(
        std::min(static_cast<u64>(nr_bytes), h.file_size - location));

    std::memcpy(buffer,
                static_cast<char *>(h.vma->start) + location,
                actual);

    if (actual < static_cast<duckdb::idx_t>(nr_bytes)) {
        std::memset(static_cast<char *>(buffer) + actual, 0,
                    static_cast<duckdb::idx_t>(nr_bytes) - actual);
    }
}

int64_t OsvUCacheFileSystem::Read(duckdb::FileHandle &handle, void *buffer,
                                  int64_t nr_bytes)
{
    auto &h = Cast(handle);
    if (h.position >= h.file_size || nr_bytes <= 0) {
        return 0;
    }
    auto actual = static_cast<duckdb::idx_t>(
        std::min(static_cast<u64>(nr_bytes), h.file_size - h.position));

    std::memcpy(buffer,
                static_cast<char *>(h.vma->start) + h.position,
                actual);
    h.position += actual;
    return static_cast<int64_t>(actual);
}

// ── Write path (bypasses cache) ───────────────────────────────────────────────

void OsvUCacheFileSystem::Write(duckdb::FileHandle &handle, void *buffer,
                                int64_t nr_bytes, duckdb::idx_t location)
{
    auto wh = inner_fs_->OpenFile(
        handle.GetPath(),
        duckdb::FileOpenFlags::FILE_FLAGS_WRITE |
        duckdb::FileOpenFlags::FILE_FLAGS_FILE_CREATE_NEW);
    inner_fs_->Write(*wh, buffer, nr_bytes, location);
}

int64_t OsvUCacheFileSystem::Write(duckdb::FileHandle &handle, void *buffer,
                                   int64_t nr_bytes)
{
    auto &h = Cast(handle);
    auto wh = inner_fs_->OpenFile(
        handle.GetPath(),
        duckdb::FileOpenFlags::FILE_FLAGS_WRITE |
        duckdb::FileOpenFlags::FILE_FLAGS_FILE_CREATE_NEW);
    inner_fs_->Seek(*wh, h.position);
    auto written = inner_fs_->Write(*wh, buffer, nr_bytes);
    h.position += static_cast<duckdb::idx_t>(written);
    return written;
}

// ── Seek / position ───────────────────────────────────────────────────────────

void OsvUCacheFileSystem::Seek(duckdb::FileHandle &handle, duckdb::idx_t location) {
    Cast(handle).position = location;
}

void OsvUCacheFileSystem::Reset(duckdb::FileHandle &handle) {
    Cast(handle).position = 0;
}

duckdb::idx_t OsvUCacheFileSystem::SeekPosition(duckdb::FileHandle &handle) {
    return Cast(handle).position;
}

// ── Metadata ──────────────────────────────────────────────────────────────────

int64_t OsvUCacheFileSystem::GetFileSize(duckdb::FileHandle &handle) {
    return static_cast<int64_t>(Cast(handle).file_size);
}

duckdb::timestamp_t
OsvUCacheFileSystem::GetLastModifiedTime(duckdb::FileHandle &handle) {
    auto &h = Cast(handle);
    if (h.inner) {
        return inner_fs_->GetLastModifiedTime(*h.inner);
    }
    return duckdb::timestamp_t::ninfinity();
}

duckdb::string OsvUCacheFileSystem::GetVersionTag(duckdb::FileHandle &handle) {
    auto &h = Cast(handle);
    if (h.inner) {
        return inner_fs_->GetVersionTag(*h.inner);
    }
    return {};
}

duckdb::FileType OsvUCacheFileSystem::GetFileType(duckdb::FileHandle &handle) {
    return duckdb::FileType::FILE_TYPE_REGULAR;
}

void OsvUCacheFileSystem::Truncate(duckdb::FileHandle &handle, int64_t new_size) {
    auto wh = inner_fs_->OpenFile(handle.GetPath(),
                                  duckdb::FileOpenFlags::FILE_FLAGS_WRITE);
    inner_fs_->Truncate(*wh, new_size);
}

void OsvUCacheFileSystem::FileSync(duckdb::FileHandle &handle) {
    (void)handle;
}

bool OsvUCacheFileSystem::Trim(duckdb::FileHandle &handle,
                               duckdb::idx_t offset_bytes,
                               duckdb::idx_t length_bytes) {
    (void)handle; (void)offset_bytes; (void)length_bytes;
    return false;
}

// ── Delegated file-system operations ─────────────────────────────────────────

bool OsvUCacheFileSystem::FileExists(const duckdb::string &filename,
                                     duckdb::optional_ptr<duckdb::FileOpener> opener) {
    return inner_fs_->FileExists(filename, opener);
}

void OsvUCacheFileSystem::RemoveFile(const duckdb::string &filename,
                                     duckdb::optional_ptr<duckdb::FileOpener> opener) {
    inner_fs_->RemoveFile(filename, opener);
}

bool OsvUCacheFileSystem::TryRemoveFile(const duckdb::string &filename,
                                        duckdb::optional_ptr<duckdb::FileOpener> opener) {
    return inner_fs_->TryRemoveFile(filename, opener);
}

void OsvUCacheFileSystem::MoveFile(const duckdb::string &source,
                                   const duckdb::string &target,
                                   duckdb::optional_ptr<duckdb::FileOpener> opener) {
    inner_fs_->MoveFile(source, target, opener);
}

bool OsvUCacheFileSystem::DirectoryExists(const duckdb::string &directory,
                                          duckdb::optional_ptr<duckdb::FileOpener> opener) {
    return inner_fs_->DirectoryExists(directory, opener);
}

void OsvUCacheFileSystem::CreateDirectory(const duckdb::string &directory,
                                          duckdb::optional_ptr<duckdb::FileOpener> opener) {
    inner_fs_->CreateDirectory(directory, opener);
}

void OsvUCacheFileSystem::RemoveDirectory(const duckdb::string &directory,
                                          duckdb::optional_ptr<duckdb::FileOpener> opener) {
    inner_fs_->RemoveDirectory(directory, opener);
}

bool OsvUCacheFileSystem::ListFiles(
    const duckdb::string &directory,
    const std::function<void(const duckdb::string &, bool)> &callback,
    duckdb::FileOpener *opener)
{
    return inner_fs_->ListFiles(directory, callback, opener);
}

bool OsvUCacheFileSystem::IsPipe(const duckdb::string &filename,
                                 duckdb::optional_ptr<duckdb::FileOpener> opener) {
    return inner_fs_->IsPipe(filename, opener);
}

duckdb::vector<duckdb::OpenFileInfo>
OsvUCacheFileSystem::Glob(const duckdb::string &path,
                          duckdb::FileOpener *opener) {
    return inner_fs_->Glob(path, opener);
}

duckdb::string OsvUCacheFileSystem::GetHomeDirectory() {
    return inner_fs_->GetHomeDirectory();
}

duckdb::string OsvUCacheFileSystem::ExpandPath(const duckdb::string &path) {
    return inner_fs_->ExpandPath(path);
}

duckdb::string OsvUCacheFileSystem::PathSeparator(const duckdb::string &path) {
    return inner_fs_->PathSeparator(path);
}

bool OsvUCacheFileSystem::IsPathAbsolute(const duckdb::string &path) {
    return inner_fs_->IsPathAbsolute(path);
}

} // namespace osv_duckdb
