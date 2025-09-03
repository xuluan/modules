#include "ChannelChunkWriter.h"
#include <cassert>
#include <iostream>
#include <algorithm>
#include <iomanip>
#include <cstring>

ChannelChunkWriter::ChannelChunkWriter(OpenVDS::VDSHandle vds)
    : m_vds(vds)
    , m_accessManager(OpenVDS::GetAccessManager(vds))
    , m_inlineCount(0)
    , m_crosslineCount(0)
    , m_sampleCount(0)
    , m_inlineMin(0)
    , m_inlineStep(1)
    , m_crosslineMin(0)
    , m_crosslineStep(1)
    , m_channelIndex(-1)
{
    // Initialize logger
    m_logger = &gdlog::GdLogger::GetInstance();
    m_log_data = m_logger->Init("ChannelChunkWriter");
}

ChannelChunkWriter::~ChannelChunkWriter()
{
    if (m_pageAccessor) {
        OpenVDS::Error error;
        m_pageAccessor->Commit();
        m_accessManager.Flush(error);
        if(error.code) {
            m_logger->LogError(m_log_data, "Error {} writing {}", error.code, error.string);
        } 
    }    

}

bool ChannelChunkWriter::Initialize(const char* channelName, 
                               int inlineCount, int crosslineCount, int sampleCount,
                               int inlineMin, int inlineStep, 
                               int crosslineMin, int crosslineStep)
{
    m_channelName = channelName;
    m_inlineCount = inlineCount;
    m_crosslineCount = crosslineCount;
    m_sampleCount = sampleCount;
    m_inlineMin = inlineMin;
    m_inlineStep = inlineStep;
    m_crosslineMin = crosslineMin;
    m_crosslineStep = crosslineStep;

    // Get layout and channel information
    auto layout = m_accessManager.GetVolumeDataLayout();
    m_channelIndex = layout->GetChannelIndex(channelName);
    
    if (m_channelIndex < 0) {
        m_lastError = "Channel not found: " + std::string(channelName);
        return false;
    }

    // Create PageAccessor
    // Use Dimensions_012 (sample, crossline, inline) dimension combination
    // LOD=0, maxPages=8, AccessMode_Create
    try {
        m_pageAccessor = m_accessManager.CreateVolumeDataPageAccessor(
            OpenVDS::Dimensions_012, 0, m_channelIndex, 8, 
            OpenVDS::VolumeDataAccessManager::AccessMode_Create);
        
        if (!m_pageAccessor) {
            m_lastError = "Failed to create VolumeDataPageAccessor";
            return false;
        }
        
        m_logger->LogInfo(m_log_data, "ChannelChunkWriter initialized successfully for channel: {}", channelName);
        m_logger->LogInfo(m_log_data, "Data dimensions: {} x {} x {}", m_sampleCount, m_crosslineCount, m_inlineCount);
        m_logger->LogInfo(m_log_data, "Total chunks: {}", m_pageAccessor->GetChunkCount());
        
        return true;
    } catch (const std::exception& e) {
        m_lastError = "Exception creating PageAccessor: " + std::string(e.what());
        return false;
    }
}

// VoxelIndexToDataIndex 
int ChannelChunkWriter::VoxelIndexToDataIndex(int sampleIndex, int crosslineIndex, int inlineIndex,
                                          const int chunkMin[6], const int pitch[6])
{
    // 3D poststack mode calculation:
    // Use inline(primary), crossline(secondary) indices
    return (inlineIndex - chunkMin[2]) * pitch[2] + 
           (crosslineIndex - chunkMin[1]) * pitch[1] + 
           (sampleIndex - chunkMin[0]) * pitch[0];
}

// Added: Write batch data (sliding window specific)
bool ChannelChunkWriter::WriteBatchData(
    const void* batchData,
    size_t batchDataSize,
    int batchStartInlineIdx,
    int batchInlineCount,
    size_t elementSize)
{
    m_logger->LogDebug(m_log_data, "WriteBatchData: batchStartIdx={}, count={}, size={}", batchStartInlineIdx, batchInlineCount, batchDataSize);
    
    // Validate parameters
    if (!batchData || batchDataSize == 0 || batchInlineCount <= 0) {
        m_lastError = "Invalid batch data parameters";
        return false;
    }
    
    // Validate batch data size
    size_t expectedBatchSize = static_cast<size_t>(batchInlineCount) * m_crosslineCount * m_sampleCount * elementSize;
    if (batchDataSize != expectedBatchSize) {
        m_lastError = "Batch data size mismatch. Expected: " + std::to_string(expectedBatchSize) 
                     + " bytes, Got: " + std::to_string(batchDataSize) + " bytes";
        return false;
    }
    
    // Validate inline range
    if (batchStartInlineIdx < 0 || batchStartInlineIdx + batchInlineCount > m_inlineCount) {
        m_lastError = "Batch inline range out of bounds";
        return false;
    }
    
    // Get total chunk count
    int64_t totalChunks = m_pageAccessor->GetChunkCount();
    
    m_logger->LogDebug(m_log_data, "Processing {} chunks for batch data", totalChunks);
    
    // Process each chunk
    for (int64_t chunkIndex = 0; chunkIndex < totalChunks; chunkIndex++) {
        // Only process empty chunks (hash == 0) - consistent with original implementation
        if (m_pageAccessor->GetChunkVolumeDataHash(chunkIndex) == 0) {
            if (!ProcessChunk(chunkIndex, batchData, elementSize, batchStartInlineIdx, batchInlineCount)) {
                m_lastError = "Failed to process batch chunk " + std::to_string(chunkIndex) + ": " + m_lastError;
                return false;
            }
        }
    }
    m_logger->LogDebug(m_log_data, "Inline range: {} - {}", batchStartInlineIdx, batchStartInlineIdx + batchInlineCount);

    m_logger->LogDebug(m_log_data, "WriteBatchData completed successfully");
    return true;
}

// Added: Calculate source index in batch data
int ChannelChunkWriter::CalculateSourceDataIndex(
    int inlineIndex, 
    int crosslineIndex, 
    int sampleIndex,
    int batchStartInlineIdx,
    int batchInlineCount)
{
    // Convert global inline index to relative index in batch
    int batchInlineIndex = inlineIndex - batchStartInlineIdx;
    
    // Boundary check
    if (batchInlineIndex < 0 || batchInlineIndex >= batchInlineCount ||
        crosslineIndex < 0 || crosslineIndex >= m_crosslineCount ||
        sampleIndex < 0 || sampleIndex >= m_sampleCount) {
        return -1;  // Invalid index
    }
    
    // Calculate linear index in batch data
    // Data storage order: sample is the fastest varying dimension, then crossline, finally inline
    return batchInlineIndex * (m_crosslineCount * m_sampleCount) + 
           crosslineIndex * m_sampleCount + 
           sampleIndex;
}

// Added: Process single chunk data writing (batch data version)
bool ChannelChunkWriter::ProcessChunk(
    int64_t chunkIndex, 
    const void* batchData, 
    size_t elementSize,
    int batchStartInlineIdx,
    int batchInlineCount)
{
    // Check chunk index range
    int64_t totalChunks = m_pageAccessor->GetChunkCount();
    if (chunkIndex >= totalChunks) {
        m_lastError = "Chunk index out of range: " + std::to_string(chunkIndex);
        return false;
    }
    
    // Get chunk boundaries
    int chunkMin[OpenVDS::Dimensionality_Max];
    int chunkMax[OpenVDS::Dimensionality_Max];
    m_pageAccessor->GetChunkMinMax(chunkIndex, chunkMin, chunkMax);
    
    // Check if this chunk intersects with current batch
    int chunkInlineStart = chunkMin[2];
    int chunkInlineEnd = chunkMax[2];
    int batchInlineEnd = batchStartInlineIdx + batchInlineCount;
    
    // NOT chunk in batch，skip
    if (!(chunkInlineStart >= batchStartInlineIdx && chunkInlineEnd <= batchInlineEnd)) {
        return true;  // skip
    }

    m_logger->LogDebug(m_log_data, "Processing chunk[{}]", chunkIndex);
    
    // Create page
    OpenVDS::VolumeDataPage* page = m_pageAccessor->CreatePage(chunkIndex);
    if (!page) {
        m_lastError = "Failed to create page for chunk " + std::to_string(chunkIndex);
        return false;
    }
    
    // Get writable buffer and pitch
    int pitch[OpenVDS::Dimensionality_Max];
    void* chunkBuffer = page->GetWritableBuffer(pitch);
    
    if (!chunkBuffer) {
        page->Release();
        m_lastError = "Failed to get writable buffer for chunk " + std::to_string(chunkIndex);
        return false;
    }
#if DEBUG_DUMP
    m_logger->LogDebug(m_log_data, "ProcessChunk: {}", chunkIndex);
    m_logger->LogDebug(m_log_data, "Chunk boundaries: [{}, {}, {}] - [{}, {}, {}]",
                       chunkMin[0], chunkMin[1], chunkMin[2],
                       chunkMax[0], chunkMax[1], chunkMax[2]);
    m_logger->LogDebug(m_log_data, "Pitch: [{}, {}, {}]", pitch[0], pitch[1], pitch[2]);
#endif    
    // Get source data pointer
    const char* sourceBytes = static_cast<const char*>(batchData);
    char* chunkBytes = static_cast<char*>(chunkBuffer);
    
    // Copy data (only process parts that intersect with batch)
    for (int inlineIndex = chunkInlineStart; inlineIndex < chunkInlineEnd; ++inlineIndex) {
        for (int crosslineIndex = chunkMin[1]; crosslineIndex < chunkMax[1]; ++crosslineIndex) {
            
            // Check boundaries
            int globalInlineIndex = m_inlineMin + inlineIndex * m_inlineStep;
            int globalCrosslineIndex = m_crosslineMin + crosslineIndex * m_crosslineStep;
            
            if (globalInlineIndex >= m_inlineMin + m_inlineCount * m_inlineStep ||
                globalCrosslineIndex >= m_crosslineMin + m_crosslineCount * m_crosslineStep) {
                continue;
            }
            
            // Calculate sample range
            int sampleStart = chunkMin[0];
            int sampleEnd = std::min(chunkMax[0], m_sampleCount);
            
            if (sampleStart >= sampleEnd) {
                continue;
            }
            
            // Calculate source and target positions for first sample
            int firstSampleSourceIndex = CalculateSourceDataIndex(
                inlineIndex, crosslineIndex, sampleStart, batchStartInlineIdx, batchInlineCount);
            
            if (firstSampleSourceIndex < 0) {
                continue;  // Skip invalid indices
            }
            
            int firstSampleTargetOffset = VoxelIndexToDataIndex(sampleStart, crosslineIndex, inlineIndex, 
                                                              chunkMin, pitch);
            
            // Calculate length of continuous data to copy
            int sampleCount = sampleEnd - sampleStart;
            size_t copySize = sampleCount * elementSize;
            
            // Starting positions for source and target data
            const char* srcPtr = sourceBytes + firstSampleSourceIndex * elementSize;
            char* dstPtr = chunkBytes + firstSampleTargetOffset * elementSize;
            
            // Bulk copy entire sample sequence
            std::memcpy(dstPtr, srcPtr, copySize);
        }
    }
    
    // Release page
    page->Release();
    return true;
}