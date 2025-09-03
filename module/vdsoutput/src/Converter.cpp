#include "Converter.h"
#include <getopt.h>
#include <cstdlib>
#include <algorithm>
#include <fstream>

#define ENABLE_ATTR 1

// === Public interface methods ===

void Converter::SetPrimaryKeyAxis(int min_val, int max_val, int num_vals)
{
    m_inlineMin = min_val;
    m_inlineMax = max_val;
    m_inlineCount = num_vals;
    m_inlineStep = (m_inlineMax - m_inlineMin) / static_cast<float>(m_inlineCount - 1);

}

void Converter::SetSecondaryKeyAxis(int min_val, int max_val, int num_vals)
{
    m_crosslineMin = min_val;
    m_crosslineMax = max_val;
    m_crosslineCount = num_vals;
    m_crosslineStep = (m_crosslineMax - m_crosslineMin) / static_cast<float>(m_crosslineCount - 1);

}

void Converter::SetDataAxis(float min_val, float max_val, int num_vals)
{
    m_timeMin = min_val;
    m_timeMax = max_val;
    m_sampleCount = num_vals;
    m_sampleInterval = (m_timeMax - m_timeMin) / static_cast<float>(m_sampleCount - 1);

}

bool Converter::createVdsStore() {
    m_logger->LogInfo(m_log_data, "Creating VDS using sliding window method...");
    
    try {
        // 1. Create VDSHandler
        m_vdsHandler = std::make_unique<VDSHandler>();
        
        // 2. Set basic parameters
        if (!m_vdsHandler->SetBasicParameters(m_outputFile, "",
                                             m_brickSize, m_lodLevels, 
                                             m_compressionMethod, m_compressionTolerance)) {
            m_logger->LogError(m_log_data, "Failed to set basic parameters: {}", m_vdsHandler->GetLastError());
            return false;
        }
        
        // 3. Set dimensions
        if (!m_vdsHandler->SetDimensions(m_sampleCount, m_timeMin, m_timeMax,
                                       m_crosslineCount, m_crosslineMin, m_crosslineMax,
                                       m_inlineCount, m_inlineMin, m_inlineMax,
                                       SampleUnits::Milliseconds)) {
            m_logger->LogError(m_log_data, "Failed to set dimensions: {}", m_vdsHandler->GetLastError());
            return false;
        }
        
        // 4. Set primary data channel
        ValueRange primaryRange(-1000.0f, 1000.0f);
        if (!m_vdsHandler->SetPrimaryChannel(m_dataFormat, "Amplitude", "", primaryRange)) {
            m_logger->LogError(m_log_data, "Failed to set primary channel: {}", m_vdsHandler->GetLastError());
            return false;
        }

#if ENABLE_ATTR           
        // 5. Add attribute channels
        for (const auto& attrField : m_attributeFields) {
            VDSAttributeField vdsAttrField;
            vdsAttrField.name = attrField.name;
            vdsAttrField.format = attrField.format;
            vdsAttrField.width = attrField.width;
            vdsAttrField.valueRange = attrField.valueRange;
            
            if (!m_vdsHandler->AddAttributeChannel(vdsAttrField)) {
                m_logger->LogError(m_log_data, "Failed to add attribute channel '{}': {}", attrField.name, m_vdsHandler->GetLastError());
                return false;
            }
        }
        
#endif  
        // 5.1 Add SEGY Trace Header channel
        if (m_enableTraceHeader) {
            VDSAttributeField traceHeaderField;
            traceHeaderField.name = "SEGYTraceHeader";
            traceHeaderField.format = OpenVDS::VolumeDataFormat::Format_U8;
            traceHeaderField.width = 240;
            traceHeaderField.valueRange = ValueRange(0.0f, 255.0f);
            
            if (!m_vdsHandler->AddAttributeChannel(traceHeaderField)) {
                m_logger->LogError(m_log_data, "ERROR: Failed to add SEGYTraceHeader channel: {}", m_vdsHandler->GetLastError());
                return false;
            }
            m_logger->LogInfo(m_log_data, "Added SEGYTraceHeader channel (240 bytes per trace)");
        }

        // 6. Create VDS file
        if (!m_vdsHandler->CreateVDS()) {
            m_logger->LogError(m_log_data, "Failed to create VDS: {}", m_vdsHandler->GetLastError());
            return false;
        }
        
    } catch (const std::exception& e) {
        m_logger->LogError(m_log_data, "Exception during VDS creation: {}", e.what());
        return false;
    }
    
    m_logger->LogInfo(m_log_data, "VDS created successfully using sliding window method");
    return true;
}

bool Converter::convertDataWithSlidingWindow() {
    m_logger->LogInfo(m_log_data, "Starting sliding window data conversion...");

    // Step 2: Setup sliding windows for all data types
    if (!setupSlidingWindows()) {
        return false;
    }
    
    // Step 3: Initialize chunk writers
    if (!initializeChunkWriters()) {
        return false;
    }
    
    // Step 4: Fill all windows initially
    if (!fillAllSlidingWindows()) {
        return false;
    }

    int batchStart = 0;
    int batchEnd = std::min(m_brickSize*2, m_inlineCount);    
    // Step 4: Process batches until all inlines are completed
    while (batchStart < m_inlineCount) {

        
        m_logger->LogInfo(m_log_data, "Processing batch: inlines [{}, {}) ({} inlines)", batchStart, batchEnd, (batchEnd - batchStart));
        
        // Process current batch for all channels
        if (!processBatch(batchStart, batchEnd)) {
            return false;
        }

        if(batchEnd >= m_inlineCount) {
            break;
        }
        // Update processed count
        batchStart += m_brickSize;
        batchEnd = std::min(batchEnd + m_brickSize, m_inlineCount);
        if (!slideAllWindows()) {
            return false;
        }


    }
    
    m_logger->LogInfo(m_log_data, "Sliding window data conversion completed!");
    return true;
}

bool Converter::finalize() {
    m_logger->LogInfo(m_log_data, "Finalizing VDS file...");
    
    // Reset all ChannelChunkWriter instances
    m_logger->LogInfo(m_log_data, "Cleaning up chunk writers...");
    m_amplitudeChunkWriter.reset();
    m_traceHeaderChunkWriter.reset();
    
    for (auto& pair : m_attributeChunkWriters) {
        pair.second.reset();
    }
    m_attributeChunkWriters.clear();
    
    if (m_vdsHandler) {
        m_vdsHandler->Close();
    }
    
    m_logger->LogInfo(m_log_data, "VDS file finalized successfully");
    return true;
}

// === Initialization methods ===

bool Converter::setupSlidingWindows() {
    m_logger->LogInfo(m_log_data, "Setting up sliding windows...");
    
    try {
        // 1. Create amplitude window
        size_t ampElementSize = getDataSize(m_dataFormat);
        int ampElementNum = m_crosslineCount * m_sampleCount;
        m_amplitudeWindow = std::make_unique<SlidingWindow>(m_brickSize, ampElementSize, ampElementNum);

        m_logger->LogInfo(m_log_data, "Created amplitude window: elementSize={}, elementNum={}", 
                  ampElementSize, ampElementNum);
        
        // 2. Create trace header window
        if (m_enableTraceHeader) {
            m_traceHeaderWindow = std::make_unique<SlidingWindow>(m_brickSize, 1, m_crosslineCount * 240);
            m_logger->LogInfo(m_log_data, "Created trace header window: elementSize=1, elementNum={}", 
                      m_crosslineCount * 240);
        }
        
        // 3. Create attribute windows
        for (const auto& attr : m_attributeFields) {
            size_t attrElementSize = attr.width;
            int attrElementNum = m_crosslineCount;
            m_attributeWindows[attr.name] = std::make_unique<SlidingWindow>(m_brickSize, attrElementSize, attrElementNum);
            
            m_logger->LogInfo(m_log_data, "Created attribute window '{}': elementSize={}, elementNum={}", 
                      attr.name, attrElementSize, attrElementNum);
        }
        
    } catch (const std::exception& e) {
        m_logger->LogError(m_log_data, "Exception setting up sliding windows: {}", e.what());
        return false;
    }
    
    return true;
}

bool Converter::initializeChunkWriters() {
    m_logger->LogInfo(m_log_data, "Initializing VDS chunk writers...");
    
    try {
        // 1. Initialize amplitude chunk writer
        m_amplitudeChunkWriter = std::make_unique<ChannelChunkWriter>(m_vdsHandler->GetVDSHandle());
        if (!m_amplitudeChunkWriter->Initialize("Amplitude",
                                              m_inlineCount, m_crosslineCount, m_sampleCount,
                                              m_inlineMin, m_inlineStep,
                                              m_crosslineMin, m_crosslineStep)) {
            m_logger->LogError(m_log_data, "ERROR: Failed to initialize amplitude chunk writer: {}", 
                      m_amplitudeChunkWriter->GetLastError());
            return false;
        }
        m_logger->LogInfo(m_log_data, "Amplitude chunk writer initialized successfully");
        
        // 2. Initialize trace header chunk writer
        if (m_enableTraceHeader) {
            m_traceHeaderChunkWriter = std::make_unique<ChannelChunkWriter>(m_vdsHandler->GetVDSHandle());
            if (!m_traceHeaderChunkWriter->Initialize("SEGYTraceHeader",
                                                    m_inlineCount, m_crosslineCount, 240,
                                                    m_inlineMin, m_inlineStep,
                                                    m_crosslineMin, m_crosslineStep)) {
                m_logger->LogError(m_log_data, "ERROR: Failed to initialize trace header chunk writer: {}", 
                          m_traceHeaderChunkWriter->GetLastError());
                return false;
            }
            m_logger->LogInfo(m_log_data, "Trace header chunk writer initialized successfully");
        }
        
        // 3. Initialize attribute chunk writers
        for (const auto& attr : m_attributeFields) {
            auto attrChunkWriter = std::make_unique<ChannelChunkWriter>(m_vdsHandler->GetVDSHandle());
            if (!attrChunkWriter->Initialize(attr.name.c_str(),
                                           m_inlineCount, m_crosslineCount, 1,  // Attributes have 1 sample per trace
                                           m_inlineMin, m_inlineStep,
                                           m_crosslineMin, m_crosslineStep)) {
                m_logger->LogError(m_log_data, "ERROR: Failed to initialize attribute chunk writer '{}': {}", attr.name, 
                          attrChunkWriter->GetLastError());
                return false;
            }
            m_attributeChunkWriters[attr.name] = std::move(attrChunkWriter);
            m_logger->LogInfo(m_log_data, "Attribute '{}' chunk writer initialized successfully", attr.name);
        }
        
    } catch (const std::exception& e) {
        m_logger->LogError(m_log_data, "Exception initializing chunk writers: {}", e.what());
        return false;
    }
    
    m_logger->LogInfo(m_log_data, "All chunk writers initialized successfully");
    return true;
}

// === Data loading methods ===

bool Converter::fillAllSlidingWindows() {
    int initialCount = std::min(2 * m_brickSize, m_inlineCount);
    m_logger->LogInfo(m_log_data, "Filling all sliding windows with initial {} inlines...", initialCount);
    
    // Fill amplitude window
    auto ampLoader = [this](int globalIdx, char* buffer, size_t size) -> bool {
        return loadAmplitudeData(globalIdx, buffer, size);
    };
    
    if (!m_amplitudeWindow->fillInitial(0, initialCount, ampLoader)) {
        m_logger->LogError(m_log_data, "Failed to fill amplitude window");
        return false;
    }
    m_logger->LogInfo(m_log_data, "Amplitude window filled successfully");
    
    // Fill trace header window
    if (m_traceHeaderWindow) {
        auto headerLoader = [this](int globalIdx, char* buffer, size_t size) -> bool {
            return loadTraceHeaderData(globalIdx, buffer, size);
        };
        
        if (!m_traceHeaderWindow->fillInitial(0, initialCount, headerLoader)) {
            m_logger->LogError(m_log_data, "Failed to fill trace header window");
            return false;
        }
        m_logger->LogInfo(m_log_data, "Trace header window filled successfully");
    }
    
    // Fill attribute windows
    for (const auto& attr : m_attributeFields) {
        auto attrLoader = [this, &attr](int globalIdx, char* buffer, size_t size) -> bool {
            return loadAttributeData(attr.name, globalIdx, buffer, size);
        };
        
        if (!m_attributeWindows[attr.name]->fillInitial(0, initialCount, attrLoader)) {
            m_logger->LogError(m_log_data, "ERROR: Failed to fill attribute window '{}'", attr.name);
            return false;
        }
        m_logger->LogInfo(m_log_data, "Attribute window '{}' filled successfully", attr.name);
    }
    
    return true;
}

bool Converter::slideAllWindows() {
    m_logger->LogInfo(m_log_data, "Sliding all windows...");
    
    // Slide amplitude window
    auto ampLoader = [this](int globalIdx, char* buffer, size_t size) -> bool {
        return loadAmplitudeData(globalIdx, buffer, size);
    };
    
    if (!m_amplitudeWindow->slide(m_inlineCount, ampLoader)) {
        m_logger->LogError(m_log_data, "Failed to slide amplitude window");
        return false;
    }
    
    // Slide trace header window
    if (m_traceHeaderWindow) {
        auto headerLoader = [this](int globalIdx, char* buffer, size_t size) -> bool {
            return loadTraceHeaderData(globalIdx, buffer, size);
        };
        
        if (!m_traceHeaderWindow->slide(m_inlineCount, headerLoader)) {
            m_logger->LogError(m_log_data, "Failed to slide trace header window");
            return false;
        }
    }
    
    // Slide attribute windows
    for (const auto& attr : m_attributeFields) {
        auto attrLoader = [this, &attr](int globalIdx, char* buffer, size_t size) -> bool {
            return loadAttributeData(attr.name, globalIdx, buffer, size);
        };
        
        if (!m_attributeWindows[attr.name]->slide(m_inlineCount, attrLoader)) {
            m_logger->LogError(m_log_data, "ERROR: Failed to slide attribute window '{}'", attr.name);
            return false;
        }
    }
    
    m_logger->LogInfo(m_log_data, "All windows slid successfully");
    return true;
}

bool Converter::loadAmplitudeData(int globalInlineIdx, char* buffer, size_t bufferSize) {
    int actualInline = m_inlineMin + globalInlineIdx * m_inlineStep;
    
    // Direct read into target buffer (zero-copy optimization)
    //bool success = m_segyReader->readTraceByPriIdx(
    //    actualInline, 
    //    m_crosslineMin, 
    //    m_crosslineMax,
    //    0, 
    //    m_sampleCount - 1,
    //    buffer  // Direct write to target buffer
    //);
    //
    //if (!success) {
    //    static_cast<gdlog::ModuleLogger*>(m_log_data)->LogError("ERROR: Failed to read amplitude data for inline {}: {}", actualInline, 
    //              m_segyReader->getErrMsg());
    //    return false;
    //}
    
    return true;
}

bool Converter::loadTraceHeaderData(int globalInlineIdx, char* buffer, size_t bufferSize) {
    int actualInline = m_inlineMin + globalInlineIdx * m_inlineStep;
    
    //bool success = m_segyReader->readSEGYTraceHeaderByPriIdx(
    //    actualInline, 
    //    m_crosslineMin, 
    //    m_crosslineMax,
    //    buffer
    //);
    //
    //if (!success) {
    //    static_cast<gdlog::ModuleLogger*>(m_log_data)->LogError("ERROR: Failed to read trace header data for inline {}: {}", actualInline, 
    //              m_segyReader->getErrMsg());
    //    return false;
    //}
    
    return true;
}

bool Converter::loadAttributeData(const std::string& attrName, int globalInlineIdx, char* buffer, size_t bufferSize) {
    int actualInline = m_inlineMin + globalInlineIdx * m_inlineStep;
    
    //bool success = m_segyReader->readAttrByPriIdx(
    //    attrName,
    //    actualInline, 
    //    m_crosslineMin, 
    //    m_crosslineMax,
    //    buffer
    //);
    //
    //if (!success) {
    //    static_cast<gdlog::ModuleLogger*>(m_log_data)->LogError("ERROR: Failed to read attribute data '{}' for inline {}: {}", attrName, 
    //              actualInline, m_segyReader->getErrMsg());
    //    return false;
    //}
    
    return true;
}

// === Batch processing methods ===

bool Converter::processBatch(int batchStartIdx, int batchEndIdx) {
    m_logger->LogInfo(m_log_data, "Processing batch: inlines [{}, {})", batchStartIdx, batchEndIdx);
    
    int batchInlineCount = batchEndIdx - batchStartIdx;
    
    // Write amplitude data for entire batch
    if (!writeBatchAmplitudeData(batchStartIdx, batchInlineCount)) {
        m_logger->LogError(m_log_data, "Failed to write batch amplitude data");
        return false;
    }
    
    // Write trace header data for entire batch  
    if (m_traceHeaderWindow && !writeBatchTraceHeaderData(batchStartIdx, batchInlineCount)) {
        m_logger->LogError(m_log_data, "Failed to write batch trace header data");
        return false;
    }
    
    // Write attribute data for entire batch
    for (const auto& attr : m_attributeFields) {
        if (!writeBatchAttributeData(attr.name, batchStartIdx, batchInlineCount)) {
            m_logger->LogError(m_log_data, "ERROR: Failed to write batch attribute data '{}'", attr.name);
            return false;
        }
    }
    
    m_logger->LogInfo(m_log_data, "Completed batch processing for all channels");
    return true;
}

// === Channel-specific batch writing ===

bool Converter::writeBatchAmplitudeData(int batchStartIdx, int batchInlineCount) {
    m_logger->LogInfo(m_log_data, "Writing amplitude batch data: start={}, count={}", batchStartIdx, 
              batchInlineCount);
    //    batchStartIdx = m_amplitudeWindow->getWindowStartIdx();
    //    batchInlineCount = m_amplitudeWindow->getValidInlineCount();    
    // Check if window contains required data
    if (!m_amplitudeWindow->containsInline(batchStartIdx) || 
        !m_amplitudeWindow->containsInline(batchStartIdx + batchInlineCount - 1)) {
        m_logger->LogError(m_log_data, "Sliding window does not contain required amplitude data for batch");
        return false;
    }
    
    // Get direct pointer to batch data (zero-copy)
    size_t batchDataSize;
    const char* batchDataPtr = m_amplitudeWindow->getRangePointer(batchStartIdx, batchInlineCount, batchDataSize);
    if (!batchDataPtr) {
        m_logger->LogError(m_log_data, "Failed to get amplitude batch data pointer");
        return false;
    }
    
    // Write batch data using member ChannelChunkWriter
    try {
        if (!m_amplitudeChunkWriter) {
            m_logger->LogError(m_log_data, "Amplitude chunk writer not initialized");
            return false;
        }
        
        size_t elementSize = getDataSize(m_dataFormat);

        if (!m_amplitudeChunkWriter->WriteBatchData(batchDataPtr, batchDataSize, 
                                                  batchStartIdx, batchInlineCount, elementSize)) {
            m_logger->LogError(m_log_data, "ERROR: Failed to write amplitude batch data: {}", 
                      m_amplitudeChunkWriter->GetLastError());
            return false;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        m_logger->LogError(m_log_data, "Exception writing amplitude batch: {}", e.what());
        return false;
    }
}

bool Converter::writeBatchTraceHeaderData(int batchStartIdx, int batchInlineCount) {
    m_logger->LogInfo(m_log_data, "Writing trace header batch data: start={}, count={}", batchStartIdx, 
              batchInlineCount);
    
    // Check if window contains required data
    if (!m_traceHeaderWindow->containsInline(batchStartIdx) || 
        !m_traceHeaderWindow->containsInline(batchStartIdx + batchInlineCount - 1)) {
        m_logger->LogError(m_log_data, "Sliding window does not contain required trace header data for batch");
        return false;
    }
    
    // Get direct pointer to batch data (zero-copy)
    size_t batchDataSize;
    const char* batchDataPtr = m_traceHeaderWindow->getRangePointer(batchStartIdx, batchInlineCount, batchDataSize);
    if (!batchDataPtr) {
        m_logger->LogError(m_log_data, "Failed to get trace header batch data pointer");
        return false;
    }
    
    // Write batch data using member ChannelChunkWriter
    try {
        if (!m_traceHeaderChunkWriter) {
            m_logger->LogError(m_log_data, "Trace header chunk writer not initialized");
            return false;
        }
        
        if (!m_traceHeaderChunkWriter->WriteBatchData(batchDataPtr, batchDataSize, 
                                                    batchStartIdx, batchInlineCount, 1)) {
            m_logger->LogError(m_log_data, "ERROR: Failed to write trace header batch data: {}", 
                      m_traceHeaderChunkWriter->GetLastError());
            return false;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        m_logger->LogError(m_log_data, "Exception writing trace header batch: {}", e.what());
        return false;
    }
}

bool Converter::writeBatchAttributeData(const std::string& attrName, int batchStartIdx, int batchInlineCount) {
    m_logger->LogInfo(m_log_data, "Writing attribute '{}' batch data: start={}, count={}", attrName, batchStartIdx, 
              batchInlineCount);
    
    // Find attribute info
    auto attrIter = std::find_if(m_attributeFields.begin(), m_attributeFields.end(),
        [&](const AttributeFieldInfo& field) { return field.name == attrName; });
    
    if (attrIter == m_attributeFields.end()) {
        m_logger->LogError(m_log_data, "Attribute field not found: {}", attrName);
        return false;
    }
    
    // Get attribute window
    auto windowIter = m_attributeWindows.find(attrName);
    if (windowIter == m_attributeWindows.end()) {
        m_logger->LogError(m_log_data, "Attribute window not found: {}", attrName);
        return false;
    }
    
    auto& attrWindow = windowIter->second;
    
    // Check if window contains required data
    if (!attrWindow->containsInline(batchStartIdx) || 
        !attrWindow->containsInline(batchStartIdx + batchInlineCount - 1)) {
        m_logger->LogError(m_log_data, "ERROR: Sliding window does not contain required data for attribute batch {}", 
                  attrName);
        return false;
    }
    
    // Get direct pointer to batch data (zero-copy)
    size_t batchDataSize;
    const char* batchDataPtr = attrWindow->getRangePointer(batchStartIdx, batchInlineCount, batchDataSize);
    if (!batchDataPtr) {
        m_logger->LogError(m_log_data, "Failed to get attribute batch data pointer for {}", attrName);
        return false;
    }
    
    // Write batch data using member ChannelChunkWriter
    try {
        auto chunkWriterIter = m_attributeChunkWriters.find(attrName);
        if (chunkWriterIter == m_attributeChunkWriters.end() || !chunkWriterIter->second) {
            m_logger->LogError(m_log_data, "Attribute chunk writer not initialized for {}", attrName);
            return false;
        }
        
        size_t elementSize = attrIter->width;
        if (!chunkWriterIter->second->WriteBatchData(batchDataPtr, batchDataSize,
                                                   batchStartIdx, batchInlineCount, elementSize)) {
            m_logger->LogError(m_log_data, "ERROR: Failed to write attribute batch data for {}: {}", attrName,
                      chunkWriterIter->second->GetLastError());
            return false;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        m_logger->LogError(m_log_data, "ERROR: Exception writing attribute batch {}: {}", attrName, e.what());
        return false;
    }
}

// === Utility methods ===

void Converter::addAttributeField(const std::string& name, int byteLocation, int width, OpenVDS::VolumeDataFormat format) {
    //m_segyReader->addAttrField(name, byteLocation, width, format);
    
    AttributeFieldInfo attrInfo;
    attrInfo.name = name;
    attrInfo.byteLocation = byteLocation;
    attrInfo.width = width;
    attrInfo.format = format;
    
    // Set reasonable value ranges
    switch(format) {
        case OpenVDS::VolumeDataFormat::Format_U8:
            attrInfo.valueRange = ValueRange(-128.0f, 127.0f);
            break;
        case OpenVDS::VolumeDataFormat::Format_U16:
            attrInfo.valueRange = ValueRange(-32768.0f, 32767.0f);
            break;
        case OpenVDS::VolumeDataFormat::Format_U32:
            attrInfo.valueRange = ValueRange(-2147483648.0f, 2147483647.0f);
            break;
        case OpenVDS::VolumeDataFormat::Format_R32:
            attrInfo.valueRange = ValueRange(-1e6f, 1e6f);
            break;
        default:
            attrInfo.valueRange = ValueRange(-1e6f, 1e6f);
            break;
    }
    
    m_attributeFields.push_back(attrInfo);
    m_logger->LogInfo(m_log_data, "Registered attribute field: {} at byte {}, width={}, format={}, range=[{}, {}]", name, byteLocation,
              width, static_cast<int>(format), attrInfo.valueRange.min, attrInfo.valueRange.max);
}

size_t Converter::getDataSize(OpenVDS::VolumeDataFormat format) {
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

