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

    // Custom textual header content
    std::string textualHeaderContent;
    
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
    bool writeTextualHeader(std::ofstream& file);
    
    // Write binary header (400 bytes) to file
    bool writeBinaryHeader(std::ofstream& file);
    
    // ASCII to EBCDIC conversion for textual header
    char asciiToEbcdic(unsigned char asciiChar);
    
    // Helper function to write field to header buffer
    void writeFieldToHeader(void* header, const void* data, const SEGY::HeaderField& headerField);
    
    // Convert sample data to proper endianness and format
    void convertSampleDataForWriting(char* data);

    int64_t calculateFilePosition(int inlineNum, int crosslineNum);
public:
    SEGYWriter();
    ~SEGYWriter();
    
    // Configuration methods
    
    // Add custom header field definition
    void addBinaryField(const std::string& name, int byteLocation, int width, SEGY::DataSampleFormatCode format);

    // Add custom trace field definition
    void addTraceField(const std::string& name, int byteLocation, int width, SEGY::DataSampleFormatCode format);

    // File creation and initialization
    
    // Initialize writer and create output file
    bool initialize(const std::string& filename, SEGYWriteInfo writeInfo);
    
    // Finalize file writing and close
    bool finalize();
    
    // Write trace data into file
    bool writeTraceData(std::ofstream& file, int inlineNum, int crosslineNum, char* sampleData);

    // Write trace header into file
    bool writeTraceHeader(std::ofstream& file, int inlineNum, int crosslineNum, char* data, int offset, int len);
    
    // Error handling
    std::string getLastError() const { return m_lastError; }

    // Get the size of a single trace in bytes
    int getTraceByteSize() const { return m_writeInfo.traceByteSize; }

    SEGY::HeaderField getTraceField(const std::string& name) const {
        auto it = m_traceFields.find(name);
        if (it != m_traceFields.end()) {
            return it->second;
        }
        return SEGY::HeaderField();
    }

    std::string getErrMsg() { return m_lastError;}

};