#include "segy_writer.h"
#include <iostream>
#include <algorithm>

namespace SEGY {

    void readFieldFromHeader(const void *header, void *data, const HeaderField &headerField, Endianness endianness) {
        if (!headerField.defined()) {
            return;
        }
        
        int index = headerField.byteLocation - 1;
        auto signed_header = reinterpret_cast<const signed char *>(header);
        auto unsigned_header = reinterpret_cast<const unsigned char *>(header);
        
        if (headerField.fieldWidth == 4) {
            int32_t result;
            if (endianness == Endianness::BigEndian) {
                result = (int32_t)(signed_header[index + 0] << 24 | unsigned_header[index + 1] << 16 | 
                               unsigned_header[index + 2] << 8 | unsigned_header[index + 3]);
            } else {
                result = (int32_t)(signed_header[index + 3] << 24 | unsigned_header[index + 2] << 16 | 
                               unsigned_header[index + 1] << 8 | unsigned_header[index + 0]);
            }
            *reinterpret_cast<int32_t*>(data) = result;
        } else if (headerField.fieldWidth == 2) {
            int16_t result;
            if (endianness == Endianness::BigEndian) {
                result = (int16_t)(signed_header[index + 0] << 8 | unsigned_header[index + 1]);
            } else {
                result = (int16_t)(signed_header[index + 1] << 8 | unsigned_header[index + 0]);
            }
            *reinterpret_cast<int16_t*>(data) = result;
        } else if (headerField.fieldWidth == 1) {
            int8_t result = signed_header[index];
            *reinterpret_cast<int8_t*>(data) = result;
        }
    }
    
    // Helper function for backward compatibility
    int readFieldFromHeaderInt(const void *header, const HeaderField &headerField, Endianness endianness) {
        int result = 0;
        readFieldFromHeader(header, &result, headerField, endianness);
        return result;
    }
}

SEGYWriter::SEGYWriter(SEGYWriteInfo writeInfo) 
    : m_writeInfo(writeInfo), m_initialized(false), m_fileCreated(false), m_log_data(nullptr) {
    
    m_logger = &gdlog::GdLogger::GetInstance();
    m_log_data = m_logger->Init("SEGYWriter");
}

SEGYWriter::~SEGYWriter() {
    if (m_outputFile.is_open()) {
        m_outputFile.close();
    }
}

void SEGYWriter::addCustomField(const std::string& name, int byteLocation, int width) {
    SEGY::HeaderField field;
    field.byteLocation = byteLocation;
    field.width = width;
    field.isSigned = true; // Default to signed
    m_customFields[name] = field;
}

void SEGYWriter::addAttrField(const std::string& name, int byteLocation, int width, SEGY::DataSampleFormatCode format) {
    SEGY::HeaderField field;
    field.byteLocation = byteLocation;
    field.width = width;
    field.isSigned = true;
    m_attrFields[name] = field;
}

bool SEGYWriter::initialize(const std::string& filename) {
    m_filename = filename;
    
    try {
        // Open output file in binary mode
        m_outputFile.open(filename, std::ios::binary | std::ios::out | std::ios::trunc);
        if (!m_outputFile.is_open()) {
            m_lastError = "Cannot open file for writing: " + filename;
            return false;
        }
        
        // Write SEGY headers
        if (!writeTextualHeader()) {
            m_lastError = "Failed to write textual header";
            return false;
        }
        
        if (!writeBinaryHeader()) {
            m_lastError = "Failed to write binary header";
            return false;
        }
        
        m_initialized = true;
        m_fileCreated = true;
        
        m_logger->Info(m_log_data, "SEGYWriter initialized for file: {}", filename);
        return true;
        
    } catch (const std::exception& e) {
        m_lastError = "Exception during initialization: " + std::string(e.what());
        return false;
    }
}

bool SEGYWriter::finalize() {
    if (m_outputFile.is_open()) {
        m_outputFile.close();
    }
    
    m_initialized = false;
    m_logger->Info(m_log_data, "SEGYWriter finalized for file: {}", m_filename);
    return true;
}

bool SEGYWriter::addTrace(int inlineNum, int crosslineNum, const void* sampleData, 
                         const std::map<std::string, int>& customHeaders) {
    if (!m_initialized) {
        m_lastError = "Writer not initialized";
        return false;
    }
    
    try {
        // Generate trace header (240 bytes)
        std::vector<char> traceHeader(240, 0);
        
        // Set coordinate information in trace header
        writeFieldToHeader(traceHeader.data(), &inlineNum, m_writeInfo.primaryKey, m_writeInfo.headerEndianness);
        writeFieldToHeader(traceHeader.data(), &crosslineNum, m_writeInfo.secondaryKey, m_writeInfo.headerEndianness);
        
        // Set sample count and interval
        writeFieldToHeader(traceHeader.data(), &m_writeInfo.sampleCount, m_writeInfo.numSamplesKey, m_writeInfo.headerEndianness);
        writeFieldToHeader(traceHeader.data(), &m_writeInfo.sampleInterval, m_writeInfo.sampleIntervalKey, m_writeInfo.headerEndianness);
        
        // Set data format code
        int formatCode = static_cast<int>(m_writeInfo.dataSampleFormatCode);
        writeFieldToHeader(traceHeader.data(), &formatCode, m_writeInfo.dataSampleFormatCodeKey, m_writeInfo.headerEndianness);
        
        // Set custom header fields
        for (const auto& customHeader : customHeaders) {
            auto fieldIt = m_customFields.find(customHeader.first);
            if (fieldIt != m_customFields.end()) {
                writeFieldToHeader(traceHeader.data(), &customHeader.second, fieldIt->second, m_writeInfo.headerEndianness);
            }
        }
        
        // Write trace header
        m_outputFile.write(traceHeader.data(), 240);
        
        // Write sample data
        m_outputFile.write(static_cast<const char*>(sampleData), m_writeInfo.traceByteSize);
        
        if (m_outputFile.fail()) {
            m_lastError = "Failed to write trace data";
            return false;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        m_lastError = "Exception during trace writing: " + std::string(e.what());
        return false;
    }
}

bool SEGYWriter::addTraceWithHeader(int inlineNum, int crosslineNum, const void* sampleData, const void* traceHeader) {
    if (!m_initialized) {
        m_lastError = "Writer not initialized";
        return false;
    }
    
    try {
        // Write provided trace header (240 bytes)
        m_outputFile.write(static_cast<const char*>(traceHeader), 240);
        
        // Write sample data
        m_outputFile.write(static_cast<const char*>(sampleData), m_writeInfo.traceByteSize);
        
        if (m_outputFile.fail()) {
            m_lastError = "Failed to write trace with header";
            return false;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        m_lastError = "Exception during trace with header writing: " + std::string(e.what());
        return false;
    }
}

bool SEGYWriter::flushTraces() {
    if (m_outputFile.is_open()) {
        m_outputFile.flush();
        return !m_outputFile.fail();
    }
    return false;
}

bool SEGYWriter::writeTraceData(std::ofstream& file, int inlineNum, int crosslineNum, const void* sampleData) {
    if (!m_initialized) {
        m_lastError = "Writer not initialized";
        return false;
    }
    
    try {
        // Calculate trace position in file
        int64_t traceIndex = calculateTracePosition(inlineNum, crosslineNum);
        if (traceIndex < 0) {
            m_lastError = "Invalid trace coordinates: " + std::to_string(inlineNum) + ", " + std::to_string(crosslineNum);
            return false;
        }
        
        // Calculate file position for sample data (skip trace header)
        int64_t headerSize = 3200 + 400; // Textual + Binary headers
        int64_t traceHeaderSize = 240;
        int64_t filePosition = headerSize + traceIndex * (traceHeaderSize + m_writeInfo.traceByteSize) + traceHeaderSize;
        
        // Seek to position and write sample data
        m_outputFile.seekp(filePosition, std::ios::beg);
        m_outputFile.write(static_cast<const char*>(sampleData), m_writeInfo.traceByteSize);
        
        if (m_outputFile.fail()) {
            m_lastError = "Failed to write trace data at position";
            return false;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        m_lastError = "Exception during trace data writing: " + std::string(e.what());
        return false;
    }
}

bool SEGYWriter::writeTraceHeader(std::ofstream& file, int inlineNum, int crosslineNum, const void* traceHeader) {
    if (!m_initialized) {
        m_lastError = "Writer not initialized";
        return false;
    }
    
    try {
        // Calculate trace position in file
        int64_t traceIndex = calculateTracePosition(inlineNum, crosslineNum);
        if (traceIndex < 0) {
            m_lastError = "Invalid trace coordinates: " + std::to_string(inlineNum) + ", " + std::to_string(crosslineNum);
            return false;
        }
        
        // Calculate file position for trace header
        int64_t headerSize = 3200 + 400; // Textual + Binary headers
        int64_t traceHeaderSize = 240;
        int64_t filePosition = headerSize + traceIndex * (traceHeaderSize + m_writeInfo.traceByteSize);
        
        // Seek to position and write trace header
        m_outputFile.seekp(filePosition, std::ios::beg);
        m_outputFile.write(static_cast<const char*>(traceHeader), 240);
        
        if (m_outputFile.fail()) {
            m_lastError = "Failed to write trace header at position";
            return false;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        m_lastError = "Exception during trace header writing: " + std::string(e.what());
        return false;
    }
}

int64_t SEGYWriter::getExpectedFileSize() const {
    // SEGY file size = textual header (3200) + binary header (400) + traces * (240 + sample data size)
    int64_t headerSize = 3200 + 400;
    int64_t totalTraces = static_cast<int64_t>(m_writeInfo.inlineCount) * m_writeInfo.crosslineCount;
    int64_t traceSize = 240 + m_writeInfo.traceByteSize;
    return headerSize + totalTraces * traceSize;
}

// Private helper functions

void SEGYWriter::generateTextualHeader(std::vector<char>& textualHeader) {
    textualHeader.resize(3200, ' ');
    
    std::string defaultText = m_writeInfo.textualHeaderContent;
    if (defaultText.empty()) {
        defaultText = "C01 SEGY file generated by SEGYWriter\n";
        defaultText += "C02 Inline range: " + std::to_string(m_writeInfo.minInline) + " - " + std::to_string(m_writeInfo.maxInline) + "\n";
        defaultText += "C03 Crossline range: " + std::to_string(m_writeInfo.minCrossline) + " - " + std::to_string(m_writeInfo.maxCrossline) + "\n";
        defaultText += "C04 Sample count: " + std::to_string(m_writeInfo.sampleCount) + "\n";
        defaultText += "C05 Sample interval: " + std::to_string(m_writeInfo.sampleInterval) + " microseconds\n";
    }
    
    // Convert to EBCDIC and copy to header
    size_t copySize = std::min(defaultText.length(), size_t(3200));
    for (size_t i = 0; i < copySize; ++i) {
        textualHeader[i] = asciiToEbcdic(static_cast<unsigned char>(defaultText[i]));
    }
}

void SEGYWriter::generateBinaryHeader(std::vector<char>& binaryHeader) {
    binaryHeader.resize(400, 0);
    
    // Set key binary header fields
    // Job identification number
    int jobId = 1;
    writeFieldToHeader(binaryHeader.data(), &jobId, {3201, 4, true}, m_writeInfo.headerEndianness);
    
    // Line number
    int lineNum = 1;
    writeFieldToHeader(binaryHeader.data(), &lineNum, {3205, 4, true}, m_writeInfo.headerEndianness);
    
    // Reel number
    int reelNum = 1;
    writeFieldToHeader(binaryHeader.data(), &reelNum, {3209, 4, true}, m_writeInfo.headerEndianness);
    
    // Number of traces per ensemble
    short tracesPerEnsemble = static_cast<short>(m_writeInfo.crosslineCount);
    writeFieldToHeader(binaryHeader.data(), &tracesPerEnsemble, {3213, 2, true}, m_writeInfo.headerEndianness);
    
    // Sample interval in microseconds
    short sampleInterval = static_cast<short>(m_writeInfo.sampleInterval);
    writeFieldToHeader(binaryHeader.data(), &sampleInterval, {3217, 2, true}, m_writeInfo.headerEndianness);
    
    // Number of samples per trace
    short sampleCount = static_cast<short>(m_writeInfo.sampleCount);
    writeFieldToHeader(binaryHeader.data(), &sampleCount, {3221, 2, true}, m_writeInfo.headerEndianness);
    
    // Data sample format code
    short formatCode = static_cast<short>(m_writeInfo.dataSampleFormatCode);
    writeFieldToHeader(binaryHeader.data(), &formatCode, {3225, 2, true}, m_writeInfo.headerEndianness);
    
    // Set custom binary header values
    for (const auto& customValue : m_writeInfo.binaryHeaderValues) {
        // This would need field definitions for custom binary header fields
        // For now, just log the attempt
        m_logger->Debug(m_log_data, "Custom binary header value: {} = {}", customValue.first, customValue.second);
    }
}

bool SEGYWriter::writeTextualHeader() {
    std::vector<char> textualHeader;
    generateTextualHeader(textualHeader);
    
    m_outputFile.write(textualHeader.data(), 3200);
    return !m_outputFile.fail();
}

bool SEGYWriter::writeBinaryHeader() {
    std::vector<char> binaryHeader;
    generateBinaryHeader(binaryHeader);
    
    m_outputFile.write(binaryHeader.data(), 400);
    return !m_outputFile.fail();
}

char SEGYWriter::asciiToEbcdic(unsigned char asciiChar) {
    // Simplified ASCII to EBCDIC conversion table
    static const unsigned char ebcdicTable[256] = {
        0x00, 0x01, 0x02, 0x03, 0x37, 0x2D, 0x2E, 0x2F, 0x16, 0x05, 0x25, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
        0x10, 0x11, 0x12, 0x13, 0x3C, 0x3D, 0x32, 0x26, 0x18, 0x19, 0x3F, 0x27, 0x1C, 0x1D, 0x1E, 0x1F,
        0x40, 0x5A, 0x7F, 0x7B, 0x5B, 0x6C, 0x50, 0x7D, 0x4D, 0x5D, 0x5C, 0x4E, 0x6B, 0x60, 0x4B, 0x61,
        0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0x7A, 0x5E, 0x4C, 0x7E, 0x6E, 0x6F,
        0x7C, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6,
        0xD7, 0xD8, 0xD9, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xBA, 0xE0, 0xBB, 0xB0, 0x6D,
        0x79, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96,
        0x97, 0x98, 0x99, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xC0, 0x4F, 0xD0, 0xA1, 0x07,
        0x20, 0x21, 0x22, 0x23, 0x24, 0x15, 0x06, 0x17, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x09, 0x0A, 0x1B,
        0x30, 0x31, 0x1A, 0x33, 0x34, 0x35, 0x36, 0x08, 0x38, 0x39, 0x3A, 0x3B, 0x04, 0x14, 0x3E, 0xFF,
        0x41, 0xAA, 0x4A, 0xB1, 0x9F, 0xB2, 0x6A, 0xB5, 0xBD, 0xB4, 0x9A, 0x8A, 0x5F, 0xCA, 0xAF, 0xBC,
        0x90, 0x8F, 0xEA, 0xFA, 0xBE, 0xA0, 0xB6, 0xB3, 0x9D, 0xDA, 0x9B, 0x8B, 0xB7, 0xB8, 0xB9, 0xAB,
        0x64, 0x65, 0x62, 0x66, 0x63, 0x67, 0x9E, 0x68, 0x74, 0x71, 0x72, 0x73, 0x78, 0x75, 0x76, 0x77,
        0xAC, 0x69, 0xED, 0xEE, 0xEB, 0xEF, 0xEC, 0xBF, 0x80, 0xFD, 0xFE, 0xFB, 0xFC, 0xAD, 0xAE, 0x59,
        0x44, 0x45, 0x42, 0x46, 0x43, 0x47, 0x9C, 0x48, 0x54, 0x51, 0x52, 0x53, 0x58, 0x55, 0x56, 0x57,
        0x8C, 0x49, 0xCD, 0xCE, 0xCB, 0xCF, 0xCC, 0xE1, 0x70, 0xDD, 0xDE, 0xDB, 0xDC, 0x8D, 0x8E, 0xDF
    };
    
    return ebcdicTable[asciiChar];
}

void SEGYWriter::writeFieldToHeader(void* header, const void* data, const SEGY::HeaderField& headerField, SEGY::Endianness endianness) {
    char* headerPtr = static_cast<char*>(header);
    const char* dataPtr = static_cast<const char*>(data);
    
    // Calculate offset (SEGY headers are 1-based, so subtract 1)
    int offset = headerField.byteLocation - 1;
    
    if (headerField.width == 2) {
        // 16-bit field
        short value = *static_cast<const short*>(data);
        if (endianness == SEGY::Endianness::Big) {
            value = ((value & 0xFF) << 8) | ((value & 0xFF00) >> 8);
        }
        std::memcpy(headerPtr + offset, &value, 2);
    } else if (headerField.width == 4) {
        // 32-bit field
        int value = *static_cast<const int*>(data);
        if (endianness == SEGY::Endianness::Big) {
            value = ((value & 0xFF) << 24) | ((value & 0xFF00) << 8) | 
                    ((value & 0xFF0000) >> 8) | ((value & 0xFF000000) >> 24);
        }
        std::memcpy(headerPtr + offset, &value, 4);
    } else {
        // Variable width field - copy as is
        std::memcpy(headerPtr + offset, dataPtr, headerField.width);
    }
}

void SEGYWriter::convertSampleDataForWriting(std::vector<char>& sampleData) {
    // Convert sample data based on format and endianness
    if (m_writeInfo.headerEndianness == SEGY::Endianness::Big) {
        // Convert to big endian
        size_t sampleSize = 0;
        switch (m_writeInfo.dataSampleFormatCode) {
            case SEGY::DataSampleFormatCode::IEEEFloat:
                sampleSize = 4;
                break;
            case SEGY::DataSampleFormatCode::Int32:
                sampleSize = 4;
                break;
            case SEGY::DataSampleFormatCode::Int16:
                sampleSize = 2;
                break;
            default:
                return; // Unsupported format
        }
        
        for (size_t i = 0; i < sampleData.size(); i += sampleSize) {
            if (sampleSize == 4) {
                uint32_t* value = reinterpret_cast<uint32_t*>(&sampleData[i]);
                *value = ((*value & 0xFF) << 24) | ((*value & 0xFF00) << 8) | 
                        ((*value & 0xFF0000) >> 8) | ((*value & 0xFF000000) >> 24);
            } else if (sampleSize == 2) {
                uint16_t* value = reinterpret_cast<uint16_t*>(&sampleData[i]);
                *value = ((*value & 0xFF) << 8) | ((*value & 0xFF00) >> 8);
            }
        }
    }
}

int64_t SEGYWriter::calculateTracePosition(int inlineNum, int crosslineNum) {
    // Validate coordinates are within bounds
    if (inlineNum < m_writeInfo.minInline || inlineNum > m_writeInfo.maxInline ||
        crosslineNum < m_writeInfo.minCrossline || crosslineNum > m_writeInfo.maxCrossline) {
        return -1; // Invalid coordinates
    }
    
    int64_t traceIndex = 0;
    
    if (m_writeInfo.isPrimaryInline) {
        // Primary key is inline, secondary key is crossline
        // Calculate inline index
        int inlineIndex = (inlineNum - m_writeInfo.minInline) / m_writeInfo.primaryStep;
        
        // Calculate crossline index  
        int crosslineIndex = (crosslineNum - m_writeInfo.minCrossline) / m_writeInfo.secondaryStep;
        
        // Calculate linear trace index (inline major ordering)
        traceIndex = static_cast<int64_t>(inlineIndex) * m_writeInfo.crosslineCount + crosslineIndex;
    } else {
        // Primary key is crossline, secondary key is inline
        // Calculate crossline index
        int crosslineIndex = (crosslineNum - m_writeInfo.minCrossline) / m_writeInfo.primaryStep;
        
        // Calculate inline index
        int inlineIndex = (inlineNum - m_writeInfo.minInline) / m_writeInfo.secondaryStep;
        
        // Calculate linear trace index (crossline major ordering)
        traceIndex = static_cast<int64_t>(crosslineIndex) * m_writeInfo.inlineCount + inlineIndex;
    }
    
    // Validate trace index is within expected bounds
    int64_t totalExpectedTraces = static_cast<int64_t>(m_writeInfo.inlineCount) * m_writeInfo.crosslineCount;
    if (traceIndex < 0 || traceIndex >= totalExpectedTraces) {
        return -1; // Invalid trace index
    }
    
    return traceIndex;
}