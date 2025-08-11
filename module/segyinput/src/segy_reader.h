/****************************************************************************
** SEGY to VDS Converter - SEGY Reader
** 
** This file implements SEGY file reading functionality using OpenVDS SEGYUtils
****************************************************************************/

#ifndef SEGY_READER_H
#define SEGY_READER_H

#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <functional>

// OpenVDS SEGY utilities headers - forward declarations to avoid internal dependencies
#include "SEGYUtils/SEGY.h"

// Forward declarations to avoid including internal headers
class DataProvider;
class TraceDataManager;
struct SEGYFileInfo;

struct SEGYVolumeInfo {
    // Volume dimensions
    int inlineCount;
    int crosslineCount; 
    int sampleCount;
    
    // Sample information
    double sampleInterval;    // in milliseconds
    double startTime;         // in milliseconds
    
    // Coordinate information
    int inlineStart, inlineEnd;
    int crosslineStart, crosslineEnd;
    double xMin, xMax, yMin, yMax;
    
    // Data format
    SEGY::BinaryHeader::DataSampleFormatCode dataFormat;
    SEGY::Endianness endianness;
    
    // Statistics
    int64_t totalTraces;
    
    SEGYVolumeInfo() 
        : inlineCount(0), crosslineCount(0), sampleCount(0)
        , sampleInterval(0.0), startTime(0.0)
        , inlineStart(0), inlineEnd(0), crosslineStart(0), crosslineEnd(0)
        , xMin(0.0), xMax(0.0), yMin(0.0), yMax(0.0)
        , dataFormat(SEGY::BinaryHeader::DataSampleFormatCode::Unknown)
        , endianness(SEGY::Endianness::BigEndian)
        , totalTraces(0) {}
};

class SEGYReader {
public:
    SEGYReader();
    ~SEGYReader();
    
    // Initialize and scan SEGY file
    bool initialize(const std::string& segyFilePath);
    
    // Get volume information after initialization
    const SEGYVolumeInfo& getVolumeInfo() const { return m_volumeInfo; }

    // Read trace data for a specific inline/crossline
    bool readTrace(int inline_num, int crossline_num, std::vector<float>& traceData);
    
    // Read multiple traces in a region
    bool readTraceRegion(int inlineStart, int inlineEnd, 
                        int crosslineStart, int crosslineEnd,
                        std::vector<float>& volumeData);

    
    // Error handling
    const std::string& getLastError() const { return m_lastError; }
    
private:
    // Private implementation details
    bool scanSEGYFile();
    bool validateSEGYFile();
    void calculateVolumeInfo();
    bool setupDataProvider();
    
    // Member variables
    std::string m_segyFilePath;
    // Note: Using simplified implementation for demonstration
    // In production, you would use the OpenVDS SEGY import libraries directly
    
    SEGYVolumeInfo m_volumeInfo;
    std::string m_lastError;
    
    // Internal state
    bool m_initialized;
    std::vector<std::string> m_inputFiles;
};

#endif // SEGY_READER_H