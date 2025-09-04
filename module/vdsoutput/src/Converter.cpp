#include "Converter.h"
#include <getopt.h>
#include <cstdlib>
#include <algorithm>
#include <fstream>

// === Public interface methods ===

void Converter::SetPrimaryKeyAxis(int min_val, int max_val, int num_vals)
{
    m_inlineMin = min_val;
    m_inlineMax = max_val;
    m_inlineCount = num_vals;
    m_inlineStep = static_cast<int>((m_inlineMax - m_inlineMin) / (m_inlineCount - 1));

}

void Converter::SetSecondaryKeyAxis(int min_val, int max_val, int num_vals)
{
    m_crosslineMin = min_val;
    m_crosslineMax = max_val;
    m_crosslineCount = num_vals;
    m_crosslineStep = static_cast<int>((m_crosslineMax - m_crosslineMin) / (m_crosslineCount - 1));

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

bool Converter::finalize() {
    m_logger->LogInfo(m_log_data, "Finalizing VDS file...");
    
    // Reset all ChannelChunkWriter instances
    m_logger->LogInfo(m_log_data, "Cleaning up chunk writers...");
    m_amplitudeChunkWriter.reset();
    
    for (auto& pair : m_attributeChunkWriters) {
        pair.second.reset();
    }
    m_attributeChunkWriters.clear();

    for (auto& pair : m_attributeWindows) {
        pair.second.reset();
    }

    m_amplitudeWindow.reset();
    
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
        size_t ampElementSize = getVDSDataSize(m_dataFormat);
        int ampElementNum = m_crosslineCount * m_sampleCount;
        m_amplitudeWindow = std::make_unique<SlidingWindow>(m_brickSize, ampElementSize, ampElementNum);

        m_logger->LogInfo(m_log_data, "Created amplitude window: elementSize={}, elementNum={}", 
                  ampElementSize, ampElementNum);
        
        // 2. Create attribute windows
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
            m_logger->LogError(m_log_data, "Error: Failed to initialize amplitude chunk writer: {}", 
                      m_amplitudeChunkWriter->GetLastError());
            return false;
        }
        m_logger->LogInfo(m_log_data, "Amplitude chunk writer initialized successfully");
        
        // 2. Initialize attribute chunk writers
        for (const auto& attr : m_attributeFields) {
            auto attrChunkWriter = std::make_unique<ChannelChunkWriter>(m_vdsHandler->GetVDSHandle());

            int sampleCount = attr.width / getVDSDataSize(attr.format);
            
            if (!attrChunkWriter->Initialize(attr.name.c_str(),
                                           m_inlineCount, m_crosslineCount, sampleCount,
                                           m_inlineMin, m_inlineStep,
                                           m_crosslineMin, m_crosslineStep)) {
                m_logger->LogError(m_log_data, "Error: Failed to initialize attribute chunk writer '{}': {}", attr.name, 
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

bool Converter::fillSlidingWindows(const std::string& attrName, char *data)
{
    if(attrName == "Amplitude") {
        return m_amplitudeWindow->fill(data);
    } else {
        auto it = m_attributeWindows.find(attrName);
        if(it != m_attributeWindows.end()){
            return it->second->fill(data);
        } else {
            m_logger->LogError(m_log_data, "Error: fillSlidingWindows cannot find channel {}", attrName);
            return false;
        }
    }
}

bool Converter::slidingWindows(const std::string& attrName)
{
    if(attrName == "Amplitude") {
        return m_amplitudeWindow->slide();
    } else {
        auto it = m_attributeWindows.find(attrName);
        if(it != m_attributeWindows.end()){
            return it->second->slide();
        } else {            
            m_logger->LogError(m_log_data, "Error: fillSlidingWindows cannot find channel {}", attrName);
            return false;
        }
    }
}

// === Batch processing methods ===

bool Converter::processBatch(const std::string& attrName, int batchStartIdx, int batchEndIdx) {
    m_logger->LogInfo(m_log_data, "Processing channel {} batch: inlines [{}, {})", attrName, batchStartIdx, batchEndIdx);
    
    int batchInlineCount = batchEndIdx - batchStartIdx;

    if(attrName == "Amplitude") {
        // Write amplitude data for entire batch
        return writeBatchAmplitudeData(batchStartIdx, batchInlineCount);
    } else {
        auto it = m_attributeWindows.find(attrName);
        if(it != m_attributeWindows.end()){
            return writeBatchAttributeData(attrName, batchStartIdx, batchInlineCount);
        } else {               
            m_logger->LogError(m_log_data, "Error: processBatch cannot find channel {}", attrName);
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
        
        size_t elementSize = getVDSDataSize(m_dataFormat);

        if (!m_amplitudeChunkWriter->WriteBatchData(batchDataPtr, batchDataSize, 
                                                  batchStartIdx, batchInlineCount, elementSize)) {
            m_logger->LogError(m_log_data, "Error: Failed to write amplitude batch data: {}", 
                      m_amplitudeChunkWriter->GetLastError());
            return false;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        m_logger->LogError(m_log_data, "Exception writing amplitude batch: {}", e.what());
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
        m_logger->LogError(m_log_data, "Error: Sliding window does not contain required data for attribute batch {}", 
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
            m_logger->LogError(m_log_data, "Error: Failed to write attribute batch data for {}: {}", attrName,
                      chunkWriterIter->second->GetLastError());
            return false;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        m_logger->LogError(m_log_data, "Error: Exception writing attribute batch {}: {}", attrName, e.what());
        return false;
    }
}

// === Utility methods ===

void Converter::addAttributeField(const std::string& name, int width, OpenVDS::VolumeDataFormat format) {
    //m_segyReader->addAttrField(name, byteLocation, width, format);
    
    AttributeFieldInfo attrInfo;
    attrInfo.name = name;
    attrInfo.width = width;
    attrInfo.format = format;

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
    m_logger->LogInfo(m_log_data, "Registered attribute field: {}, width={}, format={}, range=[{}, {}]", name,
              width, static_cast<int>(format), attrInfo.valueRange.min, attrInfo.valueRange.max);
}


