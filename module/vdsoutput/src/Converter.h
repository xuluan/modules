#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <set>
#include <cstring>
#include "VDSHandler.h"
#include "ChannelChunkWriter.h"
#include "SlidingWindow.h"
#include <GdLogger.h>


// Attribute field information storage
struct AttributeFieldInfo {
    std::string name;
    int width;
    OpenVDS::VolumeDataFormat format;
    ValueRange valueRange;
};

size_t getVDSDataSize(OpenVDS::VolumeDataFormat format) {
    switch(format) {
        case OpenVDS::VolumeDataFormat::Format_U8:      return 1;
        case OpenVDS::VolumeDataFormat::Format_U16:     return 2;
        case OpenVDS::VolumeDataFormat::Format_U32:     return 4;
        case OpenVDS::VolumeDataFormat::Format_R32:     return 4;
        case OpenVDS::VolumeDataFormat::Format_R64:     return 8;
        case OpenVDS::VolumeDataFormat::Format_U64:     return 8;
        default: return 4; // Default to float
    }
}


/**
 * SEGY to VDS converter using sliding window approach
 * Memory-efficient conversion for large datasets
 */
class Converter {
private:
    std::string m_outputFile;
    std::unique_ptr<VDSHandler> m_vdsHandler;
    

    std::vector<AttributeFieldInfo> m_attributeFields;
    
    // SEGY Trace Header control
    static const size_t TRACE_HEADER_SIZE = 240;
    
    // SEGY parameters
    int m_primaryOffset = 5;
    int m_secondaryOffset = 21;
    int m_sampleIntervalOffset = 17;
    int m_traceLengthOffset = 21;
    int m_dataFormatCodeOffset = 25;
    
    // Conversion parameters
    int m_brickSize = 64;                // Must match VDS brick size
    int m_lodLevels = 0;
    OpenVDS::CompressionMethod m_compressionMethod = OpenVDS::CompressionMethod::None;
    float m_compressionTolerance = 0.01f;
    
    // Dimension info
    int m_inlineMin, m_inlineMax, m_inlineCount, m_inlineStep;
    int m_crosslineMin, m_crosslineMax, m_crosslineCount, m_crosslineStep;
    float m_timeMin, m_timeMax;
    int m_sampleCount;
    float m_sampleInterval;
    OpenVDS::VolumeDataFormat m_dataFormat;
    
    // Sliding windows for different data types
    std::unique_ptr<SlidingWindow> m_amplitudeWindow;
    std::map<std::string, std::unique_ptr<SlidingWindow>> m_attributeWindows;
    
    // ChannelChunkWriter instances for different channels
    std::unique_ptr<ChannelChunkWriter> m_amplitudeChunkWriter;
    std::map<std::string, std::unique_ptr<ChannelChunkWriter>> m_attributeChunkWriters;
    
    // Processing state
    int m_processedInlineCount = 0;
    
    // Logger members
    gdlog::GdLogger* m_logger;
    void* m_log_data;

public:
    Converter(std::string outputFile, int brick_size, int lod_levels, OpenVDS::CompressionMethod compression_method, 
        float compression_tolerance, OpenVDS::VolumeDataFormat data_format)
        : m_outputFile(outputFile),
          m_brickSize(brick_size), m_lodLevels(lod_levels),
          m_compressionMethod(compression_method), 
          m_compressionTolerance(compression_tolerance),
          m_dataFormat(data_format) {
        m_vdsHandler = nullptr;
        // Initialize logger
        m_logger = &gdlog::GdLogger::GetInstance();
        m_log_data = m_logger->Init("Converter");
    }
    
    // === Public interface methods ===
    bool createVdsStore();
    bool convertDataWithSlidingWindow();
    bool finalize();

    void SetPrimaryKeyAxis(int min_val, int max_val, int num_vals);
    void SetSecondaryKeyAxis(int min_val, int max_val, int num_vals);
    void SetDataAxis(float min_val, float max_val, int num_vals);    

    // Add attribute field
    void addAttributeField(const std::string& name, int width, OpenVDS::VolumeDataFormat format);

    // Setup sliding windows for all data types
    bool setupSlidingWindows();

    // Initialize VDS chunk writers for all channels
    bool initializeChunkWriters();

    // Data loader functions for different types
    bool loadAmplitudeData(int globalInlineIdx, char* buffer, size_t bufferSize);
    bool loadAttributeData(const std::string& attrName, int globalInlineIdx, char* buffer, size_t bufferSize);

    bool fillSlidingWindows(const std::string& attrName, char *data);

    bool slidingWindows(const std::string& attrName);

    // Process current batch for all channels
    bool processBatch(const std::string& attrName, int batchStartIdx, int batchEndIdx);
private:

    
    // Write amplitude data for batch
    bool writeBatchAmplitudeData(int batchStartIdx, int batchInlineCount);
    
    // Write attribute data for batch
    bool writeBatchAttributeData(const std::string& attrName, int batchStartIdx, int batchInlineCount);

};