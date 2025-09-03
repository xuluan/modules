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



/**
 * SEGY to VDS converter using sliding window approach
 * Memory-efficient conversion for large datasets
 */
class Converter {
private:
    std::string m_outputFile;
    std::unique_ptr<VDSHandler> m_vdsHandler;
    
    // Attribute field information storage
    struct AttributeFieldInfo {
        std::string name;
        int byteLocation;
        int width;
        OpenVDS::VolumeDataFormat format;
        ValueRange valueRange;
    };
    std::vector<AttributeFieldInfo> m_attributeFields;
    
    // SEGY Trace Header control
    bool m_enableTraceHeader = true;
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
    std::unique_ptr<SlidingWindow> m_traceHeaderWindow;
    std::map<std::string, std::unique_ptr<SlidingWindow>> m_attributeWindows;
    
    // ChannelChunkWriter instances for different channels
    std::unique_ptr<ChannelChunkWriter> m_amplitudeChunkWriter;
    std::unique_ptr<ChannelChunkWriter> m_traceHeaderChunkWriter;
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
private:
    // === Initialization methods ===
    
    // Setup sliding windows for all data types
    bool setupSlidingWindows();
    
    // Initialize VDS chunk writers for all channels
    bool initializeChunkWriters();
    
    // === Data loading methods ===
    
    // Fill all sliding windows initially
    bool fillAllSlidingWindows();
    
    // Slide all windows for next batch
    bool slideAllWindows();
    
    // Data loader functions for different types
    bool loadAmplitudeData(int globalInlineIdx, char* buffer, size_t bufferSize);
    bool loadTraceHeaderData(int globalInlineIdx, char* buffer, size_t bufferSize);
    bool loadAttributeData(const std::string& attrName, int globalInlineIdx, char* buffer, size_t bufferSize);
    
    // === Batch processing methods ===
    
    // Process current batch for all channels
    bool processBatch(int batchStartIdx, int batchEndIdx);
    
    // === Channel-specific batch writing ===
    
    // Write amplitude data for batch
    bool writeBatchAmplitudeData(int batchStartIdx, int batchInlineCount);
    
    // Write trace header data for batch
    bool writeBatchTraceHeaderData(int batchStartIdx, int batchInlineCount);
    
    // Write attribute data for batch
    bool writeBatchAttributeData(const std::string& attrName, int batchStartIdx, int batchInlineCount);
    
    // === Utility methods ===
    
    // Add attribute field
    void addAttributeField(const std::string& name, int byteLocation, int width, OpenVDS::VolumeDataFormat format);

    // Get data size for format
    size_t getDataSize(OpenVDS::VolumeDataFormat format);

};