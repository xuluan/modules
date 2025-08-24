#pragma once

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <map>
#include <cstring>
#include <cstdint>
#include <algorithm>
#include <set>
#include <iomanip>
#include <cmath>
#include <memory>
#include <cassert>
#include <climits>
#include <GdLogger.h>

// OpenVDS SEGY structure definitions
namespace SEGY {
    enum { TextualFileHeaderSize = 3200, BinaryFileHeaderSize = 400, TraceHeaderSize = 240 };
    
    enum class Endianness { BigEndian, LittleEndian };
    
    enum class DataSampleFormatCode {
        Unknown = 0, 
        IBMFloat = 1, 
        Int32 = 2, 
        Int16 = 3, 
        FixedPoint = 4, 
        IEEEFloat = 5, 
        IEEEDouble = 6, 
        Int24 = 7, 
        Int8 = 8
    };

    struct HeaderField {
        int byteLocation;
        int fieldWidth;
        DataSampleFormatCode fieldType;
        
        HeaderField() : byteLocation(0), fieldWidth(2), fieldType(DataSampleFormatCode::Unknown) {}
        HeaderField(int loc, int width) : byteLocation(loc), fieldWidth(width), fieldType(DataSampleFormatCode::Unknown) {}
        HeaderField(int loc, int width, DataSampleFormatCode type) : byteLocation(loc), fieldWidth(width), fieldType(type) {}
        bool defined() const { return byteLocation != 0; }
    };
    
    // Standard SEGY header field definitions
    namespace BinaryHeader {
        const HeaderField SampleIntervalHeaderField(17, 2);
        const HeaderField NumSamplesHeaderField(21, 2);
        const HeaderField DataSampleFormatCodeHeaderField(25, 2);
    }
    
    namespace TraceHeader {
        const HeaderField NumSamplesHeaderField(115, 2);
        const HeaderField SampleIntervalHeaderField(117, 2);
        const HeaderField InlineNumberHeaderField(189, 4);
        const HeaderField CrosslineNumberHeaderField(193, 4);
    }
    
    // OpenVDS-style ReadFieldFromHeader implementation
    void readFieldFromHeader(const void *header, void *data, const HeaderField &headerField, Endianness endianness);
}

// Enhanced structure for writing SEGY data
struct SEGYWriteInfo {
    SEGY::Endianness headerEndianness;
    SEGY::DataSampleFormatCode dataSampleFormatCode;
    int sampleCount;
    int sampleInterval;
    int traceByteSize;
    
    // Coordinate system information
    int minInline, maxInline, inlineCount;
    int minCrossline, maxCrossline, crosslineCount;
    int primaryStep, secondaryStep;
    bool isPrimaryInline;
 
    // Custom textual header content
    std::string textualHeaderContent;
    
    // Custom binary header values
    std::map<std::string, int> binaryHeaderValues;
};

// SEGY file writer class for creating standard SEGY files
class SEGYWriter {
private:
    std::string m_filename;
    std::string m_lastError;
    bool m_initialized;
    bool m_fileCreated;
    
    SEGYWriteInfo m_writeInfo;
    std::map<std::string, SEGY::HeaderField> m_binaryFields;
    std::map<std::string, std::string> m_fieldAliases;
    std::map<std::string, SEGY::HeaderField> m_traceFields;
    
    gdlog::GdLogger* m_logger;
    void* m_log_data;
    
    // Private helper functions for file creation
    
    // Generate textual header in EBCDIC format
    void generateTextualHeader(std::vector<char>& textualHeader);
    
    // Generate binary header with standard SEGY fields
    void generateBinaryHeader(std::vector<char>& binaryHeader);
    
    // Write textual header (3200 bytes) to file
    bool writeTextualHeader();
    
    // Write binary header (400 bytes) to file
    bool writeBinaryHeader();
    
    // ASCII to EBCDIC conversion for textual header
    char asciiToEbcdic(unsigned char asciiChar);
    
    // Helper function to write field to header buffer
    void writeFieldToHeader(void* header, const void* data, const SEGY::HeaderField& headerField, SEGY::Endianness endianness);
    
    // Convert sample data to proper endianness and format
    void convertSampleDataForWriting(std::vector<char>& sampleData);
    
    // Generate trace header with coordinate information
    void generateTraceHeader(SEGYTraceData& traceData);
    
public:
    SEGYWriter();
    ~SEGYWriter();
    
    // Configuration methods
    
    // Add custom header field definition
    void addBinaryField(const std::string& name, int byteLocation, int width);

    // Add custom trace field definition
    void addTraceField(const std::string& name, int byteLocation, int width, SEGY::DataSampleFormatCode format);

    // File creation and initialization
    
    // Initialize writer and create output file
    bool initialize(const std::string& filename, SEGYWriteInfo writeInfo);
    
    // Finalize file writing and close
    bool finalize();
    
    // Data writing methods
    
    // Add single trace to buffer
    bool addTrace(int inlineNum, int crosslineNum, const void* sampleData, 
                  const std::map<std::string, int>& customHeaders = {});
    
    // Add trace with custom trace header
    bool addTraceWithHeader(int inlineNum, int crosslineNum, const void* sampleData,
                           const void* traceHeader);
    
    // Flush buffered traces to file
    bool flushTraces();
    
    // Write trace data into file
    bool writeTraceData(std::ofstream& file, int inlineNum, int crosslineNum, const void* sampleData);

    // Write trace header into file
    bool writeTraceHeader(std::ofstream& file, int inlineNum, int crosslineNum, const void* traceHeader);
    
    // Calculate expected file size
    int64_t getExpectedFileSize() const;
    
    // Error handling
    std::string getLastError() const { return m_lastError; }
    

};