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

// Reuse SEGY structure definitions from reader
#include "segy_reader.h"

// Enhanced structure for writing SEGY data
struct SEGYWriteInfo {
    SEGY::Endianness headerEndianness;
    SEGY::DataSampleFormatCode dataSampleFormatCode;
    int sampleCount;
    int sampleInterval;
    int traceByteSize;
    
    SEGY::HeaderField primaryKey;
    SEGY::HeaderField secondaryKey;
    SEGY::HeaderField numSamplesKey;
    SEGY::HeaderField sampleIntervalKey;
    SEGY::HeaderField dataSampleFormatCodeKey;
    
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
    std::map<std::string, SEGY::HeaderField> m_customFields;
    std::map<std::string, std::string> m_fieldAliases;
    std::map<std::string, SEGY::HeaderField> m_attrFields;
    
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
    SEGYWriter(SEGYWriteInfo writeInfo);
    ~SEGYWriter();
    
    // Configuration methods
    
    // Add custom header field definition
    void addCustomField(const std::string& name, int byteLocation, int width);
    
    // Add custom attribute field definition
    void addAttrField(const std::string& name, int byteLocation, int width, SEGY::DataSampleFormatCode format);

    // File creation and initialization
    
    // Initialize writer and create output file
    bool initialize(const std::string& filename);
    
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