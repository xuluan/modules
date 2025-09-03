#ifndef CHANNEL_CHUNK_WRITER_H
#define CHANNEL_CHUNK_WRITER_H

#include <OpenVDS/OpenVDS.h>
#include <OpenVDS/VolumeDataAccessManager.h>
#include <OpenVDS/VolumeDataAccess.h>
#include <memory>
#include <vector>
#include <iostream>
#include <iomanip>
#include "GdLogger.h"

class ChannelChunkWriter 
{
public:
    ChannelChunkWriter(OpenVDS::VDSHandle vds);
    ~ChannelChunkWriter();

    // Initialize writer and set data dimension information (data type independent)
    bool Initialize(const char* channelName, 
                   int inlineCount, int crosslineCount, int sampleCount,
                   int inlineMin, int inlineStep, 
                   int crosslineMin, int crosslineStep);

    // Write batch data (sliding window specific)
    bool WriteBatchData(
        const void* batchData,           // Data from sliding window
        size_t batchDataSize,            // Total size of batch data
        int batchStartInlineIdx,         // Batch starting inline index (global)
        int batchInlineCount,            // Number of inlines in batch
        size_t elementSize               // Size of single element
    );

    // Get last error information
    const std::string& GetLastError() const { return m_lastError; }

private:
    // VoxelIndexToDataIndex - Core coordinate mapping function
    int VoxelIndexToDataIndex(int sampleIndex, int crosslineIndex, int inlineIndex,
                             const int chunkMin[6], const int pitch[6]);

    // Calculate source index in batch data (sliding window specific)
    int CalculateSourceDataIndex(
        int inlineIndex, 
        int crosslineIndex, 
        int sampleIndex,
        int batchStartInlineIdx,
        int batchInlineCount
    );
    
    // Process single chunk data writing (batch data version)
    bool ProcessChunk(
        int64_t chunkIndex, 
        const void* batchData, 
        size_t elementSize,
        int batchStartInlineIdx,
        int batchInlineCount
    );

private:
    OpenVDS::VDSHandle m_vds;
    OpenVDS::VolumeDataAccessManager m_accessManager;
    std::shared_ptr<OpenVDS::VolumeDataPageAccessor> m_pageAccessor;
    
    // Data dimension information
    int m_inlineCount;
    int m_crosslineCount; 
    int m_sampleCount;
    int m_inlineMin;
    int m_inlineStep;
    int m_crosslineMin;
    int m_crosslineStep;
    
    // Channel information
    std::string m_channelName;
    int m_channelIndex;
    
    // Logger members
    gdlog::GdLogger* m_logger;
    void* m_log_data;
    
    std::string m_lastError;
};

#endif // CHANNEL_CHUNK_WRITER_H