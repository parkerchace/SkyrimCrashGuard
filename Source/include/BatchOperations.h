// Copyright (C) 2026 Parker Chace
// SPDX-License-Identifier: MIT
//
// This file is part of Skyrim CrashGuard.
// Licensed under the MIT License. See LICENSE file in the project root for details.

#pragma once

#include "PCH.h"
#include <vector>
#include <string>
#include <mutex>
#include <chrono>
#include <functional>

namespace Performance {

    // ========================================================================
    // Batch Writer for Log Operations
    // ========================================================================
    
    // Batches log writes to reduce I/O overhead
    class BatchLogWriter {
    public:
        struct LogEntry {
            std::string message;
            std::chrono::system_clock::time_point timestamp;
            int level;  // spdlog level
        };
        
        static BatchLogWriter& GetInstance() {
            static BatchLogWriter instance;
            return instance;
        }
        
        // Add log entry to batch
        void AddEntry(const std::string& message, int level = 1) {
            std::unique_lock lock(m_mutex);
            
            LogEntry entry;
            entry.message = message;
            entry.timestamp = std::chrono::system_clock::now();
            entry.level = level;
            
            m_batch.push_back(entry);
            
            // Auto-flush if batch is full
            if (m_batch.size() >= m_maxBatchSize) {
                FlushInternal();
            }
        }
        
        // Flush all batched entries
        void Flush() {
            std::unique_lock lock(m_mutex);
            FlushInternal();
        }
        
        // Set batch size threshold
        void SetMaxBatchSize(size_t size) {
            std::unique_lock lock(m_mutex);
            m_maxBatchSize = size;
        }
        
        // Get current batch size
        [[nodiscard]] size_t GetBatchSize() const {
            std::shared_lock lock(m_mutex);
            return m_batch.size();
        }
        
    private:
        BatchLogWriter() = default;
        ~BatchLogWriter() {
            Flush();  // Ensure all logs are written on shutdown
        }
        BatchLogWriter(const BatchLogWriter&) = delete;
        BatchLogWriter& operator=(const BatchLogWriter&) = delete;
        
        void FlushInternal() {
            if (m_batch.empty()) {
                return;
            }
            
            // Write all batched entries
            for (const auto& entry : m_batch) {
                // Write to actual log (spdlog)
                spdlog::log(static_cast<spdlog::level::level_enum>(entry.level), entry.message);
            }
            
            m_batch.clear();
        }
        
        mutable std::shared_mutex m_mutex;
        std::vector<LogEntry> m_batch;
        size_t m_maxBatchSize = 100;  // Flush after 100 entries
    };

    // ========================================================================
    // Batch Writer for Pattern Database
    // ========================================================================
    
    // Batches pattern database writes to reduce I/O overhead
    class BatchPatternWriter {
    public:
        struct PatternUpdate {
            std::string signature;
            std::string operation;  // "success", "failure", "new"
            std::chrono::steady_clock::time_point timestamp;
        };
        
        static BatchPatternWriter& GetInstance() {
            static BatchPatternWriter instance;
            return instance;
        }
        
        // Add pattern update to batch
        void AddUpdate(const std::string& signature, const std::string& operation) {
            std::unique_lock lock(m_mutex);
            
            PatternUpdate update;
            update.signature = signature;
            update.operation = operation;
            update.timestamp = std::chrono::steady_clock::now();
            
            m_batch.push_back(update);
            
            // Auto-flush if batch is full or enough time has passed
            auto now = std::chrono::steady_clock::now();
            auto timeSinceLastFlush = std::chrono::duration_cast<std::chrono::seconds>(now - m_lastFlush);
            
            if (m_batch.size() >= m_maxBatchSize || timeSinceLastFlush.count() >= m_flushIntervalSeconds) {
                FlushInternal();
            }
        }
        
        // Flush all batched updates
        void Flush() {
            std::unique_lock lock(m_mutex);
            FlushInternal();
        }
        
        // Set batch size threshold
        void SetMaxBatchSize(size_t size) {
            std::unique_lock lock(m_mutex);
            m_maxBatchSize = size;
        }
        
        // Set flush interval in seconds
        void SetFlushInterval(int seconds) {
            std::unique_lock lock(m_mutex);
            m_flushIntervalSeconds = seconds;
        }
        
        // Get current batch size
        [[nodiscard]] size_t GetBatchSize() const {
            std::shared_lock lock(m_mutex);
            return m_batch.size();
        }
        
        // Set flush callback (called when batch is flushed)
        void SetFlushCallback(std::function<void(const std::vector<PatternUpdate>&)> callback) {
            std::unique_lock lock(m_mutex);
            m_flushCallback = callback;
        }
        
    private:
        BatchPatternWriter() 
            : m_lastFlush(std::chrono::steady_clock::now()) 
        {}
        
        ~BatchPatternWriter() {
            Flush();  // Ensure all updates are written on shutdown
        }
        
        BatchPatternWriter(const BatchPatternWriter&) = delete;
        BatchPatternWriter& operator=(const BatchPatternWriter&) = delete;
        
        void FlushInternal() {
            if (m_batch.empty()) {
                return;
            }
            
            // Call flush callback if set
            if (m_flushCallback) {
                m_flushCallback(m_batch);
            }
            
            m_batch.clear();
            m_lastFlush = std::chrono::steady_clock::now();
        }
        
        mutable std::shared_mutex m_mutex;
        std::vector<PatternUpdate> m_batch;
        size_t m_maxBatchSize = 50;  // Flush after 50 updates
        int m_flushIntervalSeconds = 60;  // Flush every 60 seconds
        std::chrono::steady_clock::time_point m_lastFlush;
        std::function<void(const std::vector<PatternUpdate>&)> m_flushCallback;
    };

    // ========================================================================
    // Batch Writer for File Operations
    // ========================================================================
    
    // Batches file write operations to reduce I/O overhead
    class BatchFileWriter {
    public:
        struct FileWrite {
            std::string filepath;
            std::string content;
            bool append;
        };
        
        static BatchFileWriter& GetInstance() {
            static BatchFileWriter instance;
            return instance;
        }
        
        // Add file write to batch
        void AddWrite(const std::string& filepath, const std::string& content, bool append = true) {
            std::unique_lock lock(m_mutex);
            
            FileWrite write;
            write.filepath = filepath;
            write.content = content;
            write.append = append;
            
            m_batch.push_back(write);
            
            // Auto-flush if batch is full
            if (m_batch.size() >= m_maxBatchSize) {
                FlushInternal();
            }
        }
        
        // Flush all batched writes
        void Flush() {
            std::unique_lock lock(m_mutex);
            FlushInternal();
        }
        
        // Set batch size threshold
        void SetMaxBatchSize(size_t size) {
            std::unique_lock lock(m_mutex);
            m_maxBatchSize = size;
        }
        
        // Get current batch size
        [[nodiscard]] size_t GetBatchSize() const {
            std::shared_lock lock(m_mutex);
            return m_batch.size();
        }
        
    private:
        BatchFileWriter() = default;
        ~BatchFileWriter() {
            Flush();  // Ensure all writes are completed on shutdown
        }
        BatchFileWriter(const BatchFileWriter&) = delete;
        BatchFileWriter& operator=(const BatchFileWriter&) = delete;
        
        void FlushInternal() {
            if (m_batch.empty()) {
                return;
            }
            
            // Group writes by filepath
            std::unordered_map<std::string, std::vector<std::string>> groupedWrites;
            
            for (const auto& write : m_batch) {
                groupedWrites[write.filepath].push_back(write.content);
            }
            
            // Write all batched content to each file
            for (const auto& [filepath, contents] : groupedWrites) {
                try {
                    std::ofstream file(filepath, std::ios::app);
                    if (file.is_open()) {
                        for (const auto& content : contents) {
                            file << content;
                        }
                        file.close();
                    }
                } catch (const std::exception& e) {
                    spdlog::error("BatchFileWriter: Failed to write to {}: {}", filepath, e.what());
                }
            }
            
            m_batch.clear();
        }
        
        mutable std::shared_mutex m_mutex;
        std::vector<FileWrite> m_batch;
        size_t m_maxBatchSize = 20;  // Flush after 20 writes
    };

    // ========================================================================
    // Batch Operation Manager
    // ========================================================================
    
    // Manages all batch operations and provides unified interface
    class BatchOperationManager {
    public:
        static BatchOperationManager& GetInstance() {
            static BatchOperationManager instance;
            return instance;
        }
        
        // Initialize batch operations
        void Initialize() {
            spdlog::info("BatchOperationManager: Initializing batch operations");
            
            // Set up pattern writer callback
            BatchPatternWriter::GetInstance().SetFlushCallback(
                [](const std::vector<BatchPatternWriter::PatternUpdate>& updates) {
                    spdlog::debug("Flushed {} pattern updates", updates.size());
                }
            );
            
            spdlog::info("BatchOperationManager: Initialized successfully");
        }
        
        // Flush all batch operations
        void FlushAll() {
            spdlog::debug("BatchOperationManager: Flushing all batch operations");
            
            BatchLogWriter::GetInstance().Flush();
            BatchPatternWriter::GetInstance().Flush();
            BatchFileWriter::GetInstance().Flush();
            
            spdlog::debug("BatchOperationManager: All batches flushed");
        }
        
        // Shutdown and flush
        void Shutdown() {
            spdlog::info("BatchOperationManager: Shutting down");
            FlushAll();
            spdlog::info("BatchOperationManager: Shutdown complete");
        }
        
    private:
        BatchOperationManager() = default;
        ~BatchOperationManager() {
            Shutdown();
        }
        BatchOperationManager(const BatchOperationManager&) = delete;
        BatchOperationManager& operator=(const BatchOperationManager&) = delete;
    };

}  // namespace Performance
