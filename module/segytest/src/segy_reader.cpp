/****************************************************************************
** SEGY to VDS Converter - SEGY Reader Implementation
****************************************************************************/

#include "segy_reader.h"
#include <iostream>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <fstream>
#include <functional>
#include <limits.h>
#include <cstdint>
#include <arpa/inet.h>  // For ntohl, ntohs (network byte order conversion)
#include <memory>
#include <climits>
#include <iomanip>

// Note: Using simplified approach due to binary distribution limitations
// Full SEGYUtils integration would require building from source

SEGYReader::SEGYReader()
    : m_initialized(false)
    , m_traceByteSize(0)
    , m_tracesPerPage(SEGY_DEFAULT_TRACES_PER_PAGE)
{
}

// Endian conversion utilities
uint16_t SEGYReader::swapEndian16(uint16_t value) {
    return ((value & 0xFF) << 8) | ((value >> 8) & 0xFF);
}

uint32_t SEGYReader::swapEndian32(uint32_t value) {
    return ((value & 0xFF) << 24) | (((value >> 8) & 0xFF) << 16) | 
           (((value >> 16) & 0xFF) << 8) | ((value >> 24) & 0xFF);
}

int16_t SEGYReader::readInt16BE(const void* data) {
    uint16_t value;
    std::memcpy(&value, data, sizeof(uint16_t));
    return static_cast<int16_t>(swapEndian16(value));
}

int32_t SEGYReader::readInt32BE(const void* data) {
    uint32_t value;
    std::memcpy(&value, data, sizeof(uint32_t));
    return static_cast<int32_t>(swapEndian32(value));
}

uint16_t SEGYReader::readUInt16BE(const void* data) {
    uint16_t value;
    std::memcpy(&value, data, sizeof(uint16_t));
    return swapEndian16(value);
}

uint32_t SEGYReader::readUInt32BE(const void* data) {
    uint32_t value;
    std::memcpy(&value, data, sizeof(uint32_t));
    return swapEndian32(value);
}

float SEGYReader::readFloatBE(const void* data) {
    uint32_t intValue = readUInt32BE(data);
    float floatValue;
    std::memcpy(&floatValue, &intValue, sizeof(float));
    return floatValue;
}

char SEGYReader::ebcdicToAscii(unsigned char ebcdicChar) {
    // EBCDIC to ASCII conversion table
    static const unsigned char ebcdic_to_ascii[256] = {
        0x00, 0x01, 0x02, 0x03, 0x9C, 0x09, 0x86, 0x7F, 0x97, 0x8D, 0x8E, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
        0x10, 0x11, 0x12, 0x13, 0x9D, 0x85, 0x08, 0x87, 0x18, 0x19, 0x92, 0x8F, 0x1C, 0x1D, 0x1E, 0x1F,
        0x80, 0x81, 0x82, 0x83, 0x84, 0x0A, 0x17, 0x1B, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x05, 0x06, 0x07,
        0x90, 0x91, 0x16, 0x93, 0x94, 0x95, 0x96, 0x04, 0x98, 0x99, 0x9A, 0x9B, 0x14, 0x15, 0x9E, 0x1A,
        0x20, 0xA0, 0xE2, 0xE4, 0xE0, 0xE1, 0xE3, 0xE5, 0xE7, 0xF1, 0xA2, 0x2E, 0x3C, 0x28, 0x2B, 0x7C,
        0x26, 0xE9, 0xEA, 0xEB, 0xE8, 0xED, 0xEE, 0xEF, 0xEC, 0xDF, 0x21, 0x24, 0x2A, 0x29, 0x3B, 0xAC,
        0x2D, 0x2F, 0xC2, 0xC4, 0xC0, 0xC1, 0xC3, 0xC5, 0xC7, 0xD1, 0xA6, 0x2C, 0x25, 0x5F, 0x3E, 0x3F,
        0xF8, 0xC9, 0xCA, 0xCB, 0xC8, 0xCD, 0xCE, 0xCF, 0xCC, 0x60, 0x3A, 0x23, 0x40, 0x27, 0x3D, 0x22,
        0xD8, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0xAB, 0xBB, 0xF0, 0xFD, 0xFE, 0xB1,
        0xB0, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70, 0x71, 0x72, 0xAA, 0xBA, 0xE6, 0xB8, 0xC6, 0xA4,
        0xB5, 0x7E, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0xA1, 0xBF, 0xD0, 0xDD, 0xDE, 0xAE,
        0x5E, 0xA3, 0xA5, 0xB7, 0xA9, 0xA7, 0xB6, 0xBC, 0xBD, 0xBE, 0x5B, 0x5D, 0xAF, 0xA8, 0xB4, 0xD7,
        0x7B, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0xAD, 0xF4, 0xF6, 0xF2, 0xF3, 0xF5,
        0x7D, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50, 0x51, 0x52, 0xB9, 0xFB, 0xFC, 0xF9, 0xFA, 0xFF,
        0x5C, 0xF7, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0xB2, 0xD4, 0xD6, 0xD2, 0xD3, 0xD5,
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0xB3, 0xDB, 0xDC, 0xD9, 0xDA, 0x9F
    };
    
    unsigned char ascii = ebcdic_to_ascii[ebcdicChar];
    
    // Return printable ASCII character or '.' for non-printable
    if (ascii >= 32 && ascii <= 126) {
        return static_cast<char>(ascii);
    } else {
        return '.';
    }
}

SEGYReader::~SEGYReader() = default;

bool SEGYReader::initialize(const std::string& segyFilePath) {
    m_segyFilePath = segyFilePath;
    m_lastError.clear();
    
    try {
        std::cout << "Initializing SEGY reader for: " << segyFilePath << std::endl;
        
        // Setup input files vector
        m_inputFiles.clear();
        m_inputFiles.push_back(segyFilePath);
        
        // Create data provider
        if (!setupDataProvider()) {
            m_lastError = "Failed to setup data provider";
            return false;
        }
        
        // Scan SEGY file to get structure information
        if (!scanSEGYFile()) {
            m_lastError = "Failed to scan SEGY file";
            return false;
        }
        
        // Validate the scanned data
        if (!validateSEGYFile()) {
            m_lastError = "SEGY file validation failed";
            return false;
        }
        
        // Calculate volume information
        calculateVolumeInfo();
        
        // Setup DataViewManager and TraceDataManager
        if (!setupTraceManagement()) {
            m_lastError = "Failed to setup trace management";
            return false;
        }
        
        m_initialized = true;
        std::cout << "SEGY reader initialization completed successfully" << std::endl;
        
        return true;
        
    } catch (const std::exception& e) {
        m_lastError = std::string("Exception during initialization: ") + e.what();
        return false;
    }
}

bool SEGYReader::setupDataProvider() {
    try {
        std::cout << "Setting up file access for: " << m_segyFilePath << std::endl;
        
        // Basic file existence and access check
        std::ifstream file(m_segyFilePath, std::ios::binary);
        if (!file.is_open()) {
            m_lastError = "Cannot open SEGY file: " + m_segyFilePath;
            return false;
        }
        file.close();
        
        std::cout << "File access setup completed" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        m_lastError = std::string("Failed to setup file access: ") + e.what();
        return false;
    }
}

bool SEGYReader::scanSEGYFile() {
    try {
        std::cout << "Scanning SEGY file structure..." << std::endl;
/*      todo : extract meta data
        // Extract comprehensive metadata
        std::cout << "Extracting SEGY metadata..." << std::endl;
        if (!SEGYMetadataExtractor::extractMetadata(m_segyFilePath, m_metadata)) {
            std::cout << "Warning: Metadata extraction failed, using basic parsing" << std::endl;
        }
        
        // Analyze format compatibility
        std::ifstream metaFile(m_segyFilePath, std::ios::binary);
        if (metaFile.is_open()) {
            SEGYMetadataExtractor::analyzeFormat(metaFile, m_formatInfo);
            metaFile.close();
        }
*/           
        // Basic SEGY file parsing to get actual dimensions
        std::ifstream file(m_segyFilePath, std::ios::binary);
        if (!file.is_open()) {
            m_lastError = "Cannot open SEGY file for scanning: " + m_segyFilePath;
            return false;
        }
        // Skip text headers
        file.seekg(SEGY_TEXTUAL_HEADER_SIZE);
        
        // Read binary header to get sample count
        char binaryHeader[SEGY_BINARY_HEADER_SIZE];
        file.read(binaryHeader, SEGY_BINARY_HEADER_SIZE);
        
        // Extract samples per trace from binary header
        uint16_t samplesPerTrace = readUInt16BE(&binaryHeader[SEGY_BIN_SAMPLES_PER_TRACE_OFFSET]);
        
        // Extract sample interval from binary header  
        uint16_t sampleIntervalMicros = readUInt16BE(&binaryHeader[SEGY_BIN_SAMPLE_INTERVAL_OFFSET]);
        
        // Scan traces to find actual inline/crossline ranges
        file.seekg(SEGY_TOTAL_HEADER_SIZE); // Start of trace data
        
        int minInline = INT_MAX, maxInline = INT_MIN;
        int minCrossline = INT_MAX, maxCrossline = INT_MIN;
        int traceCount = 0;
        
        // Calculate trace size: trace header + samples (assuming float)
        size_t traceSize = SEGY_TRACE_HEADER_SIZE + samplesPerTrace * SEGY_FLOAT32_SIZE;
        
        while (file.good() && !file.eof()) {
            // Read trace header
            char traceHeader[SEGY_TRACE_HEADER_SIZE];
            if (!file.read(traceHeader, SEGY_TRACE_HEADER_SIZE)) break;
            
            // Extract inline number from trace header
            int32_t inlineNum = readInt32BE(&traceHeader[SEGY_TRC_INLINE_OFFSET]);
            
            // Extract crossline number from trace header
            int32_t crosslineNum = readInt32BE(&traceHeader[SEGY_TRC_CROSSLINE_OFFSET]);

            //std::cout << "SEGY trace " << traceCount << ", inlineNum: " << inlineNum  << " crosslineNum: " << crosslineNum << std::endl;
            
            // Update ranges if valid coordinates found
            if (inlineNum > 0 && crosslineNum > 0) {
                minInline = std::min(minInline, inlineNum);
                maxInline = std::max(maxInline, inlineNum);
                minCrossline = std::min(minCrossline, crosslineNum);
                maxCrossline = std::max(maxCrossline, crosslineNum);
            }
            
            traceCount++;
            
            // Skip trace data
            file.seekg(file.tellg() + std::streamoff(samplesPerTrace * SEGY_FLOAT32_SIZE));
            
            // Limit scanning for very large files
            //if (traceCount > SEGY_MAX_TRACE_SCAN_COUNT) break;
        }
        
        file.close();
        
        // Set volume info from scanned data
        m_volumeInfo.dataFormat = SEGY::BinaryHeader::DataSampleFormatCode::IEEEFloat;
        m_volumeInfo.endianness = SEGY::Endianness::BigEndian;
        m_volumeInfo.sampleInterval = sampleIntervalMicros;
        m_volumeInfo.startTime = 0.0;
        m_volumeInfo.sampleCount = samplesPerTrace;
        
        // Use scanned ranges or fall back to defaults
        if (minInline != INT_MAX && maxInline != INT_MIN) {
            m_volumeInfo.inlineStart = minInline;
            m_volumeInfo.inlineEnd = maxInline;
        } else {
            // Fallback for files without proper coordinates
            m_volumeInfo.inlineStart = SEGY_DEFAULT_INLINE_START;
            m_volumeInfo.inlineEnd = SEGY_DEFAULT_INLINE_START + (int)std::sqrt(traceCount) - 1;
        }
        
        if (minCrossline != INT_MAX && maxCrossline != INT_MIN) {
            m_volumeInfo.crosslineStart = minCrossline;
            m_volumeInfo.crosslineEnd = maxCrossline;
        } else {
            // Fallback for files without proper coordinates
            m_volumeInfo.crosslineStart = SEGY_DEFAULT_CROSSLINE_START;
            m_volumeInfo.crosslineEnd = SEGY_DEFAULT_CROSSLINE_START + (traceCount / (int)std::sqrt(traceCount)) - 1;
        }
        
        m_volumeInfo.inlineCount = m_volumeInfo.inlineEnd - m_volumeInfo.inlineStart + 1;
        m_volumeInfo.crosslineCount = m_volumeInfo.crosslineEnd - m_volumeInfo.crosslineStart + 1;
        m_volumeInfo.totalTraces = traceCount;
        
        // Extract real coordinate bounds from SEGY trace headers
        if (!extractRealCoordinateBounds()) {
            std::cout << "Warning: Failed to extract real coordinates, using fallback values" << std::endl;
            // Fallback to default values if extraction fails
            m_volumeInfo.xMin = SEGY_DEFAULT_X_MIN;
            m_volumeInfo.xMax = SEGY_DEFAULT_X_MAX;
            m_volumeInfo.yMin = SEGY_DEFAULT_Y_MIN;
            m_volumeInfo.yMax = SEGY_DEFAULT_Y_MAX;
        }
        
        std::cout << "SEGY scan completed:" << std::endl;
        std::cout << "  Inlines: " << m_volumeInfo.inlineStart << " - " << m_volumeInfo.inlineEnd 
                  << " (" << m_volumeInfo.inlineCount << " lines)" << std::endl;
        std::cout << "  Crosslines: " << m_volumeInfo.crosslineStart << " - " << m_volumeInfo.crosslineEnd
                  << " (" << m_volumeInfo.crosslineCount << " lines)" << std::endl;
        std::cout << "  Samples: " << m_volumeInfo.sampleCount << std::endl;
        std::cout << "  Total traces: " << m_volumeInfo.totalTraces << std::endl;
        
        return true;
        
    } catch (const std::exception& e) {
        m_lastError = std::string("Error scanning SEGY file: ") + e.what();
        return false;
    }
}

bool SEGYReader::validateSEGYFile() {
    // Basic validation checks
    if (m_volumeInfo.inlineCount <= 0 || m_volumeInfo.crosslineCount <= 0 || m_volumeInfo.sampleCount <= 0) {
        m_lastError = "Invalid volume dimensions";
        return false;
    }
    
    if (m_volumeInfo.sampleInterval <= 0.0) {
        m_lastError = "Invalid sample interval";
        return false;
    }
    
    if (m_volumeInfo.dataFormat == SEGY::BinaryHeader::DataSampleFormatCode::Unknown) {
        m_lastError = "Unknown data format";
        return false;
    }
    
    std::cout << "SEGY file validation passed" << std::endl;
    return true;
}

void SEGYReader::calculateVolumeInfo() {
    // Additional calculations based on scanned data
    std::cout << "Calculating volume information..." << std::endl;
    
    // Calculate approximate data size
    size_t bytesPerSample = SEGY_FLOAT32_SIZE; // Assuming float data
    size_t totalBytes = m_volumeInfo.totalTraces * m_volumeInfo.sampleCount * bytesPerSample;
    
    std::cout << "Estimated data size: " << (totalBytes / SEGY_BYTES_PER_MB) << " MB" << std::endl;
}

bool SEGYReader::setupTraceManagement() {
    try {
        std::cout << "Setting up trace management..." << std::endl;
        
        // Calculate trace byte size
        int bytesPerSample = SEGY_FLOAT32_SIZE; // Assume IEEE float for now
        m_traceByteSize = SEGY_TRACE_HEADER_SIZE + m_volumeInfo.sampleCount * bytesPerSample; // trace header + sample data
        
        std::cout << "Trace management setup completed" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        m_lastError = std::string("Failed to setup trace management: ") + e.what();
        return false;
    }
}

bool SEGYReader::readTrace(int inline_num, int crossline_num, std::vector<float>& traceData) {
    if (!m_initialized) {
        m_lastError = "SEGY reader not initialized";
        return false;
    }
    
    // Check bounds
    if (inline_num < m_volumeInfo.inlineStart || inline_num > m_volumeInfo.inlineEnd ||
        crossline_num < m_volumeInfo.crosslineStart || crossline_num > m_volumeInfo.crosslineEnd) {
        m_lastError = "Inline/Crossline out of bounds";
        return false;
    }
    
    try {
        // Resize trace data vector
        traceData.resize(m_volumeInfo.sampleCount);
        
        // Calculate trace number from inline/crossline coordinates  
        int64_t traceNumber = getTraceNumberFromCoordinates(inline_num, crossline_num);
        if (traceNumber < 0) {
            m_lastError = "Invalid trace coordinates";
            return false;
        }
        
        // Open SEGY file for reading
        std::ifstream file(m_segyFilePath, std::ios::binary);
        if (!file.is_open()) {
            m_lastError = "Cannot open SEGY file for reading";
            return false;
        }
        
        // Calculate file position for this trace
        // SEGY format: textual header + binary header + traces
        int64_t traceStartOffset = SEGY_TOTAL_HEADER_SIZE + traceNumber * m_traceByteSize;
        
        file.seekg(traceStartOffset);
        if (!file.good()) {
            m_lastError = "Failed to seek to trace position";
            file.close();
            return false;
        }
        
        // Read trace header first - we need to verify inline/crossline
        char traceHeader[SEGY_TRACE_HEADER_SIZE];
        file.read(traceHeader, SEGY_TRACE_HEADER_SIZE);
        if (file.gcount() != SEGY_TRACE_HEADER_SIZE) {
            m_lastError = "Failed to read trace header";
            file.close();
            return false;
        }
        
        // Verify inline/crossline numbers from trace header  
        int32_t fileInline = readInt32BE(&traceHeader[SEGY_TRC_INLINE_OFFSET]);
        int32_t fileCrossline = readInt32BE(&traceHeader[SEGY_TRC_CROSSLINE_OFFSET]);
        
        // For files without proper inline/crossline, skip verification
        if (fileInline > 0 && fileCrossline > 0) {
            if (fileInline != inline_num || fileCrossline != crossline_num) {
                // Try to find the correct trace by scanning nearby traces
                bool found = false;
                
                for (int offset = -SEGY_TRACE_SEARCH_RANGE; offset <= SEGY_TRACE_SEARCH_RANGE && !found; offset++) {
                    int64_t searchTraceNum = traceNumber + offset;
                    if (searchTraceNum < 0) continue;
                    
                    int64_t searchOffset = SEGY_TOTAL_HEADER_SIZE + searchTraceNum * m_traceByteSize;
                    file.seekg(searchOffset);
                    
                    if (file.read(traceHeader, SEGY_TRACE_HEADER_SIZE) && file.gcount() == SEGY_TRACE_HEADER_SIZE) {
                        fileInline = readInt32BE(&traceHeader[SEGY_TRC_INLINE_OFFSET]);
                        fileCrossline = readInt32BE(&traceHeader[SEGY_TRC_CROSSLINE_OFFSET]);
                        
                        if (fileInline == inline_num && fileCrossline == crossline_num) {
                            found = true;
                            // Continue reading from this position
                        }
                    }
                }
                
                if (!found) {
                    // Use calculated position anyway, but warn
                    std::cout << "Warning: Trace coordinates mismatch, using calculated position" << std::endl;
                    file.seekg(traceStartOffset + SEGY_TRACE_HEADER_SIZE); // Skip header
                }
            }
        }
        
        // Read sample data  
        std::vector<char> rawSampleData(m_volumeInfo.sampleCount * SEGY_FLOAT32_SIZE); // Assume 4 bytes per sample
        file.read(rawSampleData.data(), rawSampleData.size());
        if (file.gcount() != static_cast<std::streamsize>(rawSampleData.size())) {
            m_lastError = "Failed to read complete trace data";
            file.close();
            return false;
        }
        
        file.close();
        
        // Convert binary data to float based on data format
        switch (m_volumeInfo.dataFormat) {
            case SEGY::BinaryHeader::DataSampleFormatCode::IEEEFloat: {
                // IEEE 32-bit floating point
                for (int i = 0; i < m_volumeInfo.sampleCount; i++) {
                    traceData[i] = readFloatBE(&rawSampleData[i * SEGY_FLOAT32_SIZE]);
                }
                break;
            }
            case SEGY::BinaryHeader::DataSampleFormatCode::Int32: {
                // 32-bit integer
                for (int i = 0; i < m_volumeInfo.sampleCount; i++) {
                    int32_t value = readInt32BE(&rawSampleData[i * SEGY_INT32_SIZE]);
                    traceData[i] = static_cast<float>(value);
                }
                break;
            }
            case SEGY::BinaryHeader::DataSampleFormatCode::Int16: {
                // 16-bit integer
                for (int i = 0; i < m_volumeInfo.sampleCount; i++) {
                    int16_t value = readInt16BE(&rawSampleData[i * SEGY_INT16_SIZE]);
                    traceData[i] = static_cast<float>(value);
                }
                break;
            }
            default:
                // Fallback to IEEE Float format
                for (int i = 0; i < m_volumeInfo.sampleCount; i++) {
                    traceData[i] = readFloatBE(&rawSampleData[i * SEGY_FLOAT32_SIZE]);
                }
                break;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        m_lastError = std::string("Error reading trace: ") + e.what();
        return false;
    }
}

bool SEGYReader::readTraceRegion(int inlineStart, int inlineEnd,
                                int crosslineStart, int crosslineEnd,
                                std::vector<float>& volumeData) {
    if (!m_initialized) {
        m_lastError = "SEGY reader not initialized";
        return false;
    }
    
    // Validate region bounds
    if (inlineStart < m_volumeInfo.inlineStart || inlineEnd > m_volumeInfo.inlineEnd ||
        crosslineStart < m_volumeInfo.crosslineStart || crosslineEnd > m_volumeInfo.crosslineEnd ||
        inlineStart > inlineEnd || crosslineStart > crosslineEnd) {
        m_lastError = "Invalid region bounds";
        return false;
    }
    
    try {
        int regionInlines = inlineEnd - inlineStart + 1;
        int regionCrosslines = crosslineEnd - crosslineStart + 1;
        size_t totalSamples = size_t(regionInlines) * regionCrosslines * m_volumeInfo.sampleCount;
        
        volumeData.resize(totalSamples);
        
        std::cout << "Reading region: IL " << inlineStart << "-" << inlineEnd 
                  << ", XL " << crosslineStart << "-" << crosslineEnd << std::endl;
        
        // Read traces in the region
        std::vector<float> traceData;
        size_t idx = 0;
        int tracesRead = 0;
        int totalTraces = regionInlines * regionCrosslines;
        
        for (int il = inlineStart; il <= inlineEnd; il++) {
            for (int xl = crosslineStart; xl <= crosslineEnd; xl++) {
                if (!readTrace(il, xl, traceData)) {
                    return false;
                }
                
                // Copy trace data to volume array
                std::memcpy(&volumeData[idx], traceData.data(), m_volumeInfo.sampleCount * sizeof(float));
                idx += m_volumeInfo.sampleCount;
                
                tracesRead++;

            }
        }
        
        std::cout << "Successfully read " << tracesRead << " traces from region" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        m_lastError = std::string("Error reading trace region: ") + e.what();
        return false;
    }
}

bool SEGYReader::extractRealCoordinateBounds() {
    try {
        std::cout << "Extracting real coordinate bounds from SEGY headers..." << std::endl;
        
        std::ifstream file(m_segyFilePath, std::ios::binary);
        if (!file.is_open()) {
            std::cout << "Warning: Cannot open SEGY file for coordinate extraction" << std::endl;
            return false;
        }
        
        // Skip textual header and read binary header
        file.seekg(SEGY_TEXTUAL_HEADER_SIZE, std::ios::beg);
        
        // Read key fields from binary header
        char binaryHeader[SEGY_BINARY_HEADER_SIZE];
        file.read(binaryHeader, SEGY_BINARY_HEADER_SIZE);
        if (!file.good()) {
            std::cout << "Warning: Failed to read SEGY binary header" << std::endl;
            return false;
        }
        
        // Extract sample count from binary header
        int16_t samplesPerTrace = readInt16BE(&binaryHeader[SEGY_BIN_SAMPLES_PER_TRACE_OFFSET]);
        
        if (samplesPerTrace <= 0 || samplesPerTrace > SEGY_MAX_SAMPLES_PER_TRACE) {
            std::cout << "Warning: Invalid samples per trace: " << samplesPerTrace << std::endl;
            return false;
        }
        
        // Calculate trace size: trace header + samples (assuming float32)
        size_t traceSize = SEGY_TRACE_HEADER_SIZE + samplesPerTrace * SEGY_FLOAT32_SIZE;
        
        // Skip to first trace (after textual + binary headers)
        file.seekg(SEGY_TOTAL_HEADER_SIZE, std::ios::beg);
        
        // Initialize coordinate bounds
        double xMin = std::numeric_limits<double>::max();
        double xMax = std::numeric_limits<double>::lowest();
        double yMin = std::numeric_limits<double>::max();
        double yMax = std::numeric_limits<double>::lowest();
        
        bool foundValidCoordinates = false;
        int validCoordinateCount = 0;
        int tracesScanned = 0;
        
        char traceHeader[SEGY_TRACE_HEADER_SIZE];
        
        while (file.good() && tracesScanned < SEGY_MAX_COORD_SCAN_COUNT) {
            // Read trace header
            file.read(traceHeader, SEGY_TRACE_HEADER_SIZE);
            if (file.gcount() != SEGY_TRACE_HEADER_SIZE) {
                break; // End of file or read error
            }
            
            // Extract coordinates from trace header
            // SEGY standard coordinate locations
            
            int32_t xCoord = readInt32BE(&traceHeader[SEGY_TRC_X_COORD_OFFSET]);
            int32_t yCoord = readInt32BE(&traceHeader[SEGY_TRC_Y_COORD_OFFSET]);
            int16_t coordinateScale = readInt16BE(&traceHeader[SEGY_TRC_COORD_SCALE_OFFSET]);
            
            // Check if coordinates are valid (non-zero)
            if (xCoord != 0 || yCoord != 0) {
                double x = static_cast<double>(xCoord);
                double y = static_cast<double>(yCoord);
                
                // Apply coordinate scaling
                if (coordinateScale > 0) {
                    x *= coordinateScale;
                    y *= coordinateScale;
                } else if (coordinateScale < 0) {
                    x /= -coordinateScale;
                    y /= -coordinateScale;
                }
                
                // Update bounds
                xMin = std::min(xMin, x);
                xMax = std::max(xMax, x);
                yMin = std::min(yMin, y);
                yMax = std::max(yMax, y);
                
                foundValidCoordinates = true;
                validCoordinateCount++;
            }
            
            // Skip to next trace
            file.seekg(traceSize - SEGY_TRACE_HEADER_SIZE, std::ios::cur);
            tracesScanned++;
        }
        
        file.close();
        
        if (!foundValidCoordinates) {
            std::cout << "Warning: No valid coordinates found in " << tracesScanned << " trace headers" << std::endl;
            return false;
        }
        
        // Store the extracted coordinate bounds
        m_volumeInfo.xMin = xMin;
        m_volumeInfo.xMax = xMax;
        m_volumeInfo.yMin = yMin;
        m_volumeInfo.yMax = yMax;
        
        std::cout << "Successfully extracted coordinate bounds:" << std::endl;
        printf(" X range:  %lf to %lf \n", xMin, xMax );
        printf(" Y range:  %lf to %lf \n", yMin, yMax );

        std::cout << "  X range: " << xMin << " to " << xMax << std::endl;
        std::cout << "  Y range: " << yMin << " to " << yMax << std::endl;
        std::cout << "  Valid coordinates found: " << validCoordinateCount << " (scanned " << tracesScanned << " traces)" << std::endl;
        
        return true;
        
    } catch (const std::exception& e) {
        std::cout << "Exception during coordinate extraction: " << e.what() << std::endl;
        return false;
    }
}

int64_t SEGYReader::getTraceNumberFromCoordinates(int inlineNum, int crosslineNum) {
    // Simple calculation for regular grid - may need to be more sophisticated for irregular SEGY files
    if (inlineNum < m_volumeInfo.inlineStart || inlineNum > m_volumeInfo.inlineEnd ||
        crosslineNum < m_volumeInfo.crosslineStart || crosslineNum > m_volumeInfo.crosslineEnd) {
        return -1; // Invalid coordinates
    }
    
    // Calculate linear trace number from inline/crossline coordinates
    int inlineIndex = inlineNum - m_volumeInfo.inlineStart;
    int crosslineIndex = crosslineNum - m_volumeInfo.crosslineStart;
    
    // Assuming traces are organized in row-major order (inline varies fastest)
    int64_t traceNumber = (int64_t)crosslineIndex * m_volumeInfo.inlineCount + inlineIndex;
    
    return traceNumber;
}

bool SEGYReader::printTextualHeader() const {
    try {
        std::cout << "Reading SEGY Textual Header (3200 bytes)..." << std::endl;
        
        std::ifstream file(m_segyFilePath, std::ios::binary);
        if (!file.is_open()) {
            std::cout << "Error: Cannot open SEGY file: " << m_segyFilePath << std::endl;
            return false;
        }
        
        // Read the 3200-byte textual header
        char textualHeader[SEGY_TEXTUAL_HEADER_SIZE];
        file.read(textualHeader, SEGY_TEXTUAL_HEADER_SIZE);
        
        if (file.gcount() != SEGY_TEXTUAL_HEADER_SIZE) {
            std::cout << "Error: Failed to read complete textual header. Read " 
                      << file.gcount() << " bytes, expected " << SEGY_TEXTUAL_HEADER_SIZE << std::endl;
            file.close();
            return false;
        }
        
        file.close();
        
        std::cout << "\n========== SEGY Textual Header (3200 bytes) ==========" << std::endl;
        
        // Print header in 40 lines of 80 characters each (SEGY standard format)
        for (int line = 0; line < 40; line++) {
            std::cout << "Line " << std::setfill('0') << std::setw(2) << (line + 1) << ": ";
            
            for (int col = 0; col < 80; col++) {
                int index = line * 80 + col;
                unsigned char ebcdicChar = static_cast<unsigned char>(textualHeader[index]);
                
                // Convert EBCDIC to ASCII
                char asciiChar = ebcdicToAscii(ebcdicChar);
                std::cout << asciiChar;
            }
            std::cout << std::endl;
        }
        
        std::cout << "=================================================" << std::endl;
        
        return true;
        
    } catch (const std::exception& e) {
        std::cout << "Exception while reading textual header: " << e.what() << std::endl;
        return false;
    }
}