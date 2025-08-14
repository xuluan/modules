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

// Note: Using simplified approach without OpenVDS dependencies

// SEGY Format Constants (based on SEG-Y Revision 1 specification)
// File structure constants
#define SEGY_TEXTUAL_HEADER_SIZE        3200    // EBCDIC text header size
#define SEGY_BINARY_HEADER_SIZE         400     // Binary header size  
#define SEGY_TRACE_HEADER_SIZE          240     // Trace header size
#define SEGY_TOTAL_HEADER_SIZE          (SEGY_TEXTUAL_HEADER_SIZE + SEGY_BINARY_HEADER_SIZE)  // 3600

// Binary header field offsets (0-based, from start of binary header)
#define SEGY_BIN_SAMPLE_INTERVAL_OFFSET 16      // Sample interval in microseconds (bytes 3216-3217)
#define SEGY_BIN_SAMPLES_PER_TRACE_OFFSET 20    // Number of samples per trace (bytes 3220-3221)
#define SEGY_BIN_DATA_FORMAT_OFFSET 24          // Data format code (bytes 3224-3225)

// Trace header field offsets (0-based, from start of trace header)  
#define SEGY_TRC_COORD_SCALE_OFFSET     70      // Coordinate scalar (bytes 70-71)
#define SEGY_TRC_X_COORD_OFFSET         72     // CDP X coordinate (bytes 180-183)
#define SEGY_TRC_Y_COORD_OFFSET         76     // CDP Y coordinate (bytes 184-187)
#define SEGY_TRC_INLINE_OFFSET          4     // Inline number (bytes 188-191)
#define SEGY_TRC_CROSSLINE_OFFSET       20     // Crossline number (bytes 192-195)

// Data type sizes
#define SEGY_FLOAT32_SIZE               4       // IEEE 32-bit float
#define SEGY_INT32_SIZE                 4       // 32-bit integer
#define SEGY_INT16_SIZE                 2       // 16-bit integer

// Validation constants
#define SEGY_MAX_SAMPLES_PER_TRACE      32000   // Maximum reasonable samples per trace
#define SEGY_MAX_TRACE_SCAN_COUNT       10000   // Maximum traces to scan for analysis
#define SEGY_MAX_COORD_SCAN_COUNT       1000    // Maximum traces to scan for coordinates
#define SEGY_TRACE_SEARCH_RANGE         100     // Range for trace coordinate search

// Default fallback values
#define SEGY_DEFAULT_INLINE_START       1000    // Default inline start if not found
#define SEGY_DEFAULT_CROSSLINE_START    2000    // Default crossline start if not found
#define SEGY_DEFAULT_X_MIN              100000.0 // Default X coordinate minimum
#define SEGY_DEFAULT_X_MAX              105000.0 // Default X coordinate maximum  
#define SEGY_DEFAULT_Y_MIN              200000.0 // Default Y coordinate minimum
#define SEGY_DEFAULT_Y_MAX              204000.0 // Default Y coordinate maximum

// Memory constants
#define SEGY_BYTES_PER_MB               (1024 * 1024)   // Bytes per megabyte
#define SEGY_DEFAULT_TRACES_PER_PAGE    1000            // Default traces per page

// Simple SEGY enums for this demonstration
namespace SEGY {
    namespace BinaryHeader {
        enum class DataSampleFormatCode {
            Unknown = 0,
            IEEEFloat = 5,
            Int32 = 2,
            Int16 = 3,
            Int8 = 8
        };
    }
    
    enum class Endianness {
        BigEndian = 0,
        LittleEndian = 1
    };
}

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

/*
    // Get extracted metadata
    const SEGYMetadata& getMetadata() const { return m_metadata; }
    
    // Get format compatibility information
    const SEGYFormatInfo& getFormatInfo() const { return m_formatInfo; }
*/    

    
    // Read trace data for a specific inline/crossline
    bool readTrace(int inline_num, int crossline_num, std::vector<float>& traceData);
    
    // Read multiple traces in a region
    bool readTraceRegion(int inlineStart, int inlineEnd, 
                        int crosslineStart, int crosslineEnd,
                        std::vector<float>& volumeData);
    
    // Error handling
    const std::string& getLastError() const { return m_lastError; }
    
    // Print 3200-byte textual header content
    bool printTextualHeader() const;
    
private:
    // Private implementation details
    bool scanSEGYFile();
    bool validateSEGYFile();
    void calculateVolumeInfo();
    bool setupDataProvider();
    bool setupTraceManagement();
    bool extractRealCoordinateBounds();
    int64_t getTraceNumberFromCoordinates(int inlineNum, int crosslineNum);
    
    // Endian conversion utilities
    static uint16_t swapEndian16(uint16_t value);
    static uint32_t swapEndian32(uint32_t value);
    static int16_t readInt16BE(const void* data);
    static int32_t readInt32BE(const void* data);
    static uint16_t readUInt16BE(const void* data);
    static uint32_t readUInt32BE(const void* data);
    static float readFloatBE(const void* data);
    
    // EBCDIC to ASCII conversion utility
    static char ebcdicToAscii(unsigned char ebcdicChar);
    
    // Convert SEGY data format code to enum
    static SEGY::BinaryHeader::DataSampleFormatCode convertDataFormatCode(uint16_t formatCode);
    
    // Get bytes per sample for a given data format
    static size_t getBytesPerSample(SEGY::BinaryHeader::DataSampleFormatCode dataFormat);
    
    // Member variables
    std::string m_segyFilePath;
    
    SEGYVolumeInfo m_volumeInfo;
    std::string m_lastError;
/*
    SEGYMetadata m_metadata;
    SEGYFormatInfo m_formatInfo;
*/

    
    // Internal state
    bool m_initialized;
    std::vector<std::string> m_inputFiles;
    
    // Note: Using direct file I/O approach since OpenVDS binary distribution
    // doesn't include complete SEGYUtils headers
    
    // Additional members for trace management
    int64_t m_traceByteSize;
    int64_t m_tracesPerPage;
};

#endif // SEGY_READER_H