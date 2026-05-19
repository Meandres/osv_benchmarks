#pragma once

// OSv Parquet-reader integration with uCache.
//
// This header is included by DuckDB's parquet extension (cmake build) as well
// as by osv_ucache_file_system.cc (OSv kernel build).  It therefore must NOT
// include any OSv kernel headers (<osv/ucache.hh>, <osv/mmu.hh>, etc.).
//
// OSv kernel types are referenced only as opaque pointers via forward
// declarations.  All code that actually dereferences those types lives in
// osv_ucache_file_system.cc, which is compiled as part of the OSv kernel build
// with the full kernel header tree available.
//
#include <duckdb/common/file_system.hpp>
#include <duckdb/common/file_open_flags.hpp>
#include <duckdb/common/optional_ptr.hpp>
#include <duckdb/common/types/timestamp.hpp>
#include <duckdb/storage/buffer/buffer_handle.hpp>

// Thrift transport base + ParquetTransportBase ABC
#include "thrift/transport/TVirtualTransport.h"
#include "thrift_tools.hpp"

#include <cstring>
#include <algorithm>
#include <cstdint>

// ── Forward-declare OSv kernel types ──────────────────────────────────────────
// Only used as pointers; no kernel headers pulled in.
namespace ucache {
class VMA;
class Buffer;
} // namespace ucache

namespace osv_duckdb {

// Issue async IO for the [pos, pos+len) byte range into the VMA's uCache buffers.
// Only Uncached buffers in the range are prefetched; already-cached pages are skipped.
// Implementation in osv_ucache_file_system.cc.
void enqueue_prefetch(ucache::VMA *vma, duckdb::idx_t pos, duckdb::idx_t len);


// ─────────────────────────────────────────────────────────────────────────────
// OsvCachingFileHandle
//
// Drop-in replacement for CachingFileHandle backed by a uCache VMA.
// Reads return a pointer directly into VMA memory — no copy, no DuckDB
// buffer-manager allocation.  Page faults are handled transparently by
// uCache → local_ufile → NVMe (polled).
//
// Note: ucache::VMA is an incomplete type here; the pointer is opaque.
//       Any method that dereferences vma is non-inline and defined in
//       osv_ucache_file_system.cc.
// ─────────────────────────────────────────────────────────────────────────────
struct OsvCachingFileHandle {
    ucache::VMA                             *vma;
    const char                              *vma_base;   // = static_cast<char*>(vma->start)
    duckdb::idx_t                            file_size;
    duckdb::string                           path;
    duckdb::unique_ptr<duckdb::FileHandle>   inner;       // for metadata queries
    duckdb::FileSystem                      &fs;

    OsvCachingFileHandle(ucache::VMA *vma_p,
                         const char *base_p,
                         duckdb::idx_t size_p,
                         const duckdb::string &path_p,
                         duckdb::unique_ptr<duckdb::FileHandle> inner_p,
                         duckdb::FileSystem &fs_p)
        : vma(vma_p), vma_base(base_p), file_size(size_p),
          path(path_p), inner(std::move(inner_p)), fs(fs_p)
    {}

    // ── Read API ─────────────────────────────────────────────────────────────
    // Sets buffer to point directly into VMA memory (zero-copy).
    duckdb::BufferHandle Read(duckdb::data_ptr_t &buffer,
                              duckdb::idx_t /*nr_bytes*/,
                              duckdb::idx_t location) {
        buffer = reinterpret_cast<duckdb::data_ptr_t>(
            const_cast<char *>(vma_base + location));
        return duckdb::BufferHandle();   // sentinel — VMA owns the memory
    }

    // ── Prefetch API ─────────────────────────────────────────────────────────
    void RegisterPrefetch(duckdb::idx_t pos, duckdb::idx_t len) {
        enqueue_prefetch(vma, pos, len);
    }
    void PrefetchRegistered() {}  // IO was already issued at RegisterPrefetch time
    void ClearPrefetch() {}       // in-flight IOs drain naturally via checkPipeline

    // ── Metadata ─────────────────────────────────────────────────────────────
    duckdb::idx_t       GetFileSize()        { return file_size; }
    duckdb::string      GetPath()  const     { return path; }
    bool                CanSeek()            { return true; }
    bool                IsRemoteFile() const { return false; }
    // OnDiskFile() = false → DuckDB enables prefetch mode for Parquet reads.
    bool                OnDiskFile()         { return false; }
    duckdb::FileHandle &GetFileHandle()      { return *inner; }

    duckdb::timestamp_t GetLastModifiedTime() {
        if (!inner) return duckdb::timestamp_t::ninfinity();
        return inner->file_system.GetLastModifiedTime(*inner);
    }
    duckdb::string GetVersionTag() {
        if (!inner) return {};
        return inner->file_system.GetVersionTag(*inner);
    }
    bool Validate() const { return true; }
    duckdb::idx_t SeekPosition() { return 0; }
    void Seek(duckdb::idx_t /*location*/) {}
};


// ─────────────────────────────────────────────────────────────────────────────
// OsvThriftFileTransport
//
// A Thrift transport backed by OsvCachingFileHandle.  Reads are direct
// memcpy's from VMA memory; prefetch registration populates the uCache
// prefetch queue; actual prefetch drain is handled via parquet_prefetch_pol.
// ─────────────────────────────────────────────────────────────────────────────
class OsvThriftFileTransport
    : public duckdb_apache::thrift::transport::TVirtualTransport<OsvThriftFileTransport>,
      public duckdb::ParquetTransportBase {
public:
    static constexpr uint64_t PREFETCH_FALLBACK_BUFFERSIZE = 1000000;

    OsvThriftFileTransport(OsvCachingFileHandle &handle_p, bool /*prefetch_mode_p*/)
        : handle(handle_p), location(0), size(handle_p.file_size)
    {}

    // ── TVirtualTransport read ────────────────────────────────────────────────
    uint32_t read(uint8_t *buf, uint32_t len) override {
        duckdb::idx_t actual = std::min(static_cast<duckdb::idx_t>(len),
                                        size - location);
        if (actual == 0) return 0;
        std::memcpy(buf, handle.vma_base + location, actual);
        location += actual;
        return static_cast<uint32_t>(actual);
    }

    // ── ParquetTransportBase interface ───────────────────────────────────────
    void SetLocation(duckdb::idx_t loc) override         { location = loc; }
    duckdb::idx_t GetLocation() const override           { return location; }
    duckdb::idx_t GetSize() const override               { return size; }
    void Skip(duckdb::idx_t skip_count) override         { location += skip_count; }

    // VMA reads are always available — no ReadHead buffering needed.
    duckdb::optional_ptr<duckdb::ReadHead> GetReadHead(duckdb::idx_t /*pos*/) override {
        return nullptr;
    }

    bool HasPrefetch() const override { return !prefetch_registered; }

    void RegisterPrefetch(duckdb::idx_t pos, uint64_t len, bool /*can_merge*/ = true) override {
        handle.RegisterPrefetch(pos, static_cast<duckdb::idx_t>(len));
    }
    void FinalizeRegistration() override {}
    void Prefetch(duckdb::idx_t pos, uint64_t len) override {
        RegisterPrefetch(pos, len);
        prefetch_registered = true;
    }
    void PrefetchRegistered() override { prefetch_registered = true; }
    void ClearPrefetch() override {
        prefetch_registered = false;
        handle.ClearPrefetch();
    }

private:
    OsvCachingFileHandle &handle;
    duckdb::idx_t         location;
    duckdb::idx_t         size;
    bool                  prefetch_registered = false;
};


// ─────────────────────────────────────────────────────────────────────────────
// OsvParquetFileSystemBase
//
// Thin mixin interface implemented by OsvUCacheFileSystem.
// Included by DuckDB parquet source (no kernel headers needed) so that
// parquet_reader.cpp can dynamic_cast to this type and call OpenParquetHandle
// without depending on the full OsvUCacheFileSystem definition.
// ─────────────────────────────────────────────────────────────────────────────
class OsvParquetFileSystemBase {
public:
    virtual ~OsvParquetFileSystemBase() = default;

    virtual duckdb::unique_ptr<OsvCachingFileHandle> OpenParquetHandle(
        const duckdb::string &path,
        duckdb::FileOpenFlags flags,
        duckdb::optional_ptr<duckdb::FileOpener> opener = nullptr) = 0;
};

} // namespace osv_duckdb
