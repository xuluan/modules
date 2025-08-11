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

// OpenVDS library includes
#include <OpenVDS/OpenVDS.h>
#include <memory>
#include <climits>

// Note: Using simplified approach due to binary distribution limitations
// Full SEGYUtils integration would require building from source

SEGYReader::SEGYReader()
    : m_initialized(false)
    , m_traceByteSize(0)
    , m_tracesPerPage(1000)
{
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
/*
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
        // Skip text headers (3200 bytes)
        file.seekg(3200);
        
        // Read binary header to get sample count
        char binaryHeader[400];
        file.read(binaryHeader, 400);
        
        // Extract samples per trace (bytes 3220-3221, 0-based in binary header is 20-21)
        uint16_t samplesPerTrace = 0;
        file.seekg(3220);
        file.read(reinterpret_cast<char*>(&samplesPerTrace), 2);
        // Convert from big-endian if needed
        samplesPerTrace = ((samplesPerTrace & 0xFF) << 8) | ((samplesPerTrace >> 8) & 0xFF);
        
        // Extract sample interval (bytes 3216-3217)
        uint16_t sampleIntervalMicros = 0;
        file.seekg(3216);
        file.read(reinterpret_cast<char*>(&sampleIntervalMicros), 2);
        sampleIntervalMicros = ((sampleIntervalMicros & 0xFF) << 8) | ((sampleIntervalMicros >> 8) & 0xFF);
        
        // Scan traces to find actual inline/crossline ranges
        file.seekg(3600); // Start of trace data
        
        int minInline = INT_MAX, maxInline = INT_MIN;
        int minCrossline = INT_MAX, maxCrossline = INT_MIN;
        int traceCount = 0;
        
        // Calculate trace size: 240 bytes header + samples * 4 bytes (assuming float)
        size_t traceSize = 240 + samplesPerTrace * 4;
        
        while (file.good() && !file.eof()) {
            // Read trace header
            char traceHeader[240];
            if (!file.read(traceHeader, 240)) break;
            
            // Extract inline number (bytes 188-191 in trace header, big-endian int32)
            int32_t inlineNum = 0;
            memcpy(&inlineNum, &traceHeader[188], 4);
            inlineNum = ((inlineNum & 0xFF) << 24) | (((inlineNum >> 8) & 0xFF) << 16) | 
                       (((inlineNum >> 16) & 0xFF) << 8) | ((inlineNum >> 24) & 0xFF);
            
            // Extract crossline number (bytes 192-195 in trace header, big-endian int32)
            int32_t crosslineNum = 0;
            memcpy(&crosslineNum, &traceHeader[192], 4);
            crosslineNum = ((crosslineNum & 0xFF) << 24) | (((crosslineNum >> 8) & 0xFF) << 16) | 
                          (((crosslineNum >> 16) & 0xFF) << 8) | ((crosslineNum >> 24) & 0xFF);
            
            // Update ranges if valid coordinates found
            if (inlineNum > 0 && crosslineNum > 0) {
                minInline = std::min(minInline, inlineNum);
                maxInline = std::max(maxInline, inlineNum);
                minCrossline = std::min(minCrossline, crosslineNum);
                maxCrossline = std::max(maxCrossline, crosslineNum);
            }
            
            traceCount++;
            
            // Skip trace data
            file.seekg(file.tellg() + std::streamoff(samplesPerTrace * 4));
            
            // Limit scanning for very large files
            if (traceCount > 10000) break;
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
            m_volumeInfo.inlineStart = 1000;
            m_volumeInfo.inlineEnd = 1000 + (int)std::sqrt(traceCount) - 1;
        }
        
        if (minCrossline != INT_MAX && maxCrossline != INT_MIN) {
            m_volumeInfo.crosslineStart = minCrossline;
            m_volumeInfo.crosslineEnd = maxCrossline;
        } else {
            // Fallback for files without proper coordinates
            m_volumeInfo.crosslineStart = 2000;
            m_volumeInfo.crosslineEnd = 2000 + (traceCount / (int)std::sqrt(traceCount)) - 1;
        }
        
        m_volumeInfo.inlineCount = m_volumeInfo.inlineEnd - m_volumeInfo.inlineStart + 1;
        m_volumeInfo.crosslineCount = m_volumeInfo.crosslineEnd - m_volumeInfo.crosslineStart + 1;
        m_volumeInfo.totalTraces = traceCount;
        
        // Extract real coordinate bounds from SEGY trace headers
        if (!extractRealCoordinateBounds()) {
            std::cout << "Warning: Failed to extract real coordinates, using fallback values" << std::endl;
            // Fallback to mock values if extraction fails
            m_volumeInfo.xMin = 100000.0;
            m_volumeInfo.xMax = 105000.0;
            m_volumeInfo.yMin = 200000.0;
            m_volumeInfo.yMax = 204000.0;
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
    size_t bytesPerSample = 4; // Assuming float data
    size_t totalBytes = m_volumeInfo.totalTraces * m_volumeInfo.sampleCount * bytesPerSample;
    
    std::cout << "Estimated data size: " << (totalBytes / (1024 * 1024)) << " MB" << std::endl;
}

bool SEGYReader::setupTraceManagement() {
    try {
        std::cout << "Setting up trace management..." << std::endl;
        
        // Calculate trace byte size
        int bytesPerSample = 4; // Assume IEEE float for now
        m_traceByteSize = 240 + m_volumeInfo.sampleCount * bytesPerSample; // 240 bytes header + sample data
        
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
        // SEGY format: 3200 byte textual header + 400 byte binary header + traces
        int64_t traceStartOffset = 3600 + traceNumber * m_traceByteSize;
        
        file.seekg(traceStartOffset);
        if (!file.good()) {
            m_lastError = "Failed to seek to trace position";
            file.close();
            return false;
        }
        
        // Read trace header first (240 bytes) - we need to verify inline/crossline
        char traceHeader[240];
        file.read(traceHeader, 240);
        if (file.gcount() != 240) {
            m_lastError = "Failed to read trace header";
            file.close();
            return false;
        }
        
        // Verify inline/crossline numbers from trace header  
        int32_t fileInline, fileCrossline;
        std::memcpy(&fileInline, &traceHeader[188], 4);   // Bytes 188-191: Inline
        std::memcpy(&fileCrossline, &traceHeader[192], 4); // Bytes 192-195: Crossline
        
        // Convert from big-endian if needed
        if (m_volumeInfo.endianness == SEGY::Endianness::BigEndian) {
            fileInline = ntohl(fileInline);
            fileCrossline = ntohl(fileCrossline);
        }
        
        // For files without proper inline/crossline, skip verification
        if (fileInline > 0 && fileCrossline > 0) {
            if (fileInline != inline_num || fileCrossline != crossline_num) {
                // Try to find the correct trace by scanning nearby traces
                bool found = false;
                const int maxSearchRange = 100;
                
                for (int offset = -maxSearchRange; offset <= maxSearchRange && !found; offset++) {
                    int64_t searchTraceNum = traceNumber + offset;
                    if (searchTraceNum < 0) continue;
                    
                    int64_t searchOffset = 3600 + searchTraceNum * m_traceByteSize;
                    file.seekg(searchOffset);
                    
                    if (file.read(traceHeader, 240) && file.gcount() == 240) {
                        std::memcpy(&fileInline, &traceHeader[188], 4);
                        std::memcpy(&fileCrossline, &traceHeader[192], 4);
                        
                        if (m_volumeInfo.endianness == SEGY::Endianness::BigEndian) {
                            fileInline = ntohl(fileInline);
                            fileCrossline = ntohl(fileCrossline);
                        }
                        
                        if (fileInline == inline_num && fileCrossline == crossline_num) {
                            found = true;
                            // Continue reading from this position
                        }
                    }
                }
                
                if (!found) {
                    // Use calculated position anyway, but warn
                    std::cout << "Warning: Trace coordinates mismatch, using calculated position" << std::endl;
                    file.seekg(traceStartOffset + 240); // Skip header
                }
            }
        }
        
        // Read sample data
        std::vector<char> rawSampleData(m_volumeInfo.sampleCount * 4); // Assume 4 bytes per sample
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
                const float* floatData = reinterpret_cast<const float*>(rawSampleData.data());
                for (int i = 0; i < m_volumeInfo.sampleCount; i++) {
                    float value = floatData[i];
                    // Handle endianness conversion if needed
                    if (m_volumeInfo.endianness == SEGY::Endianness::BigEndian) {
                        uint32_t temp;
                        std::memcpy(&temp, &value, 4);
                        temp = ntohl(temp);
                        std::memcpy(&value, &temp, 4);
                    }
                    traceData[i] = value;
                }
                break;
            }
            case SEGY::BinaryHeader::DataSampleFormatCode::Int32: {
                // 32-bit integer
                const int32_t* intData = reinterpret_cast<const int32_t*>(rawSampleData.data());
                for (int i = 0; i < m_volumeInfo.sampleCount; i++) {
                    int32_t value = intData[i];
                    // Handle endianness conversion if needed
                    if (m_volumeInfo.endianness == SEGY::Endianness::BigEndian) {
                        value = ntohl(value);
                    }
                    traceData[i] = static_cast<float>(value);
                }
                break;
            }
            case SEGY::BinaryHeader::DataSampleFormatCode::Int16: {
                // 16-bit integer
                const int16_t* shortData = reinterpret_cast<const int16_t*>(rawSampleData.data());
                for (int i = 0; i < m_volumeInfo.sampleCount; i++) {
                    int16_t value = shortData[i];
                    // Handle endianness conversion if needed
                    if (m_volumeInfo.endianness == SEGY::Endianness::BigEndian) {
                        value = ntohs(value);
                    }
                    traceData[i] = static_cast<float>(value);
                }
                break;
            }
            default:
                // Fallback to IEEE Float format
                const float* floatData = reinterpret_cast<const float*>(rawSampleData.data());
                for (int i = 0; i < m_volumeInfo.sampleCount; i++) {
                    float value = floatData[i];
                    if (m_volumeInfo.endianness == SEGY::Endianness::BigEndian) {
                        uint32_t temp;
                        std::memcpy(&temp, &value, 4);
                        temp = ntohl(temp);
                        std::memcpy(&value, &temp, 4);
                    }
                    traceData[i] = value;
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
        
        // Skip textual header (3200 bytes) and read binary header
        file.seekg(3200, std::ios::beg);
        
        // Read key fields from binary header
        char binaryHeader[400];
        file.read(binaryHeader, 400);
        if (!file.good()) {
            std::cout << "Warning: Failed to read SEGY binary header" << std::endl;
            return false;
        }
        
        // Extract sample count from binary header (bytes 3220-3221, 0-based offset 20-21)
        int16_t samplesPerTrace = 0;
        std::memcpy(&samplesPerTrace, &binaryHeader[20], 2);
        
        // Handle endianness (SEGY is typically big endian)
        samplesPerTrace = ntohs(samplesPerTrace);
        
        if (samplesPerTrace <= 0 || samplesPerTrace > 32000) {
            std::cout << "Warning: Invalid samples per trace: " << samplesPerTrace << std::endl;
            return false;
        }
        
        // Calculate trace size: 240 bytes header + samples * 4 bytes (assuming float32)
        size_t traceSize = 240 + samplesPerTrace * 4;
        
        // Skip to first trace (after 3200 byte textual + 400 byte binary headers)
        file.seekg(3600, std::ios::beg);
        
        // Initialize coordinate bounds
        double xMin = std::numeric_limits<double>::max();
        double xMax = std::numeric_limits<double>::lowest();
        double yMin = std::numeric_limits<double>::max();
        double yMax = std::numeric_limits<double>::lowest();
        
        bool foundValidCoordinates = false;
        int validCoordinateCount = 0;
        int tracesScanned = 0;
        const int maxTracesToScan = 1000; // Scan up to 1000 traces for coordinate bounds
        
        char traceHeader[240];
        
        while (file.good() && tracesScanned < maxTracesToScan) {
            // Read trace header
            file.read(traceHeader, 240);
            if (file.gcount() != 240) {
                break; // End of file or read error
            }
            
            // Extract coordinates from trace header
            // SEGY standard coordinate locations (0-based byte offsets):
            // Bytes 180-183: CDP X coordinate (EnsembleXCoordinateHeaderField)
            // Bytes 184-187: CDP Y coordinate (EnsembleYCoordinateHeaderField)  
            // Bytes 70-71:   Coordinate scale factor (CoordinateScaleHeaderField)
            
            int32_t xCoord, yCoord;
            int16_t coordinateScale;
            
            std::memcpy(&xCoord, &traceHeader[180], 4);
            std::memcpy(&yCoord, &traceHeader[184], 4);
            std::memcpy(&coordinateScale, &traceHeader[70], 2);
            
            // Convert from network byte order (big endian)
            xCoord = ntohl(xCoord);
            yCoord = ntohl(yCoord);
            coordinateScale = ntohs(coordinateScale);
            
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
            file.seekg(traceSize - 240, std::ios::cur);
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