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

// OpenVDS-style segment information structure
struct SEGYSegmentInfo {
    int primaryKey;
    int64_t traceStart;
    int64_t traceStop;
    float score;
    
    SEGYSegmentInfo(int pk, int64_t start) 
        : primaryKey(pk), traceStart(start), traceStop(start), score(0.0f) {}
    
    int64_t traceCount() const { return traceStop - traceStart + 1; }
};

// Simplified single-file OpenVDS-style file information structure
struct SEGYFileInfo {
    SEGY::Endianness headerEndianness;
    SEGY::DataSampleFormatCode dataSampleFormatCode;
    int sampleCount;
    int sampleInterval;
    int64_t totalTraces;
    int traceByteSize;
    
    SEGY::HeaderField primaryKey;
    SEGY::HeaderField secondaryKey;

    SEGY::HeaderField numSamplesKey;
    SEGY::HeaderField sampleIntervalKey;
    SEGY::HeaderField dataSampleFormatCodeKey;

    std::vector<SEGYSegmentInfo> segments; // Single file segments only
    
    // Coordinate range information for trace index calculation
    int minInline, maxInline, inlineCount;
    int minCrossline, maxCrossline, crosslineCount;
    int primaryStep, secondaryStep;
    bool isPrimaryInline; // true if primary key is inline, false if crossline
};

// Single-file SEGY analyzer based on true OpenVDS algorithms
class SEGYReader {
private:
    std::string m_filename;
    std::string m_lastError;
    // Internal state
    bool m_initialized;
        
    SEGYFileInfo m_fileInfo;
    std::map<std::string, SEGY::HeaderField> m_customFields;
    std::map<std::string, std::string> m_fieldAliases;
    std::map<std::string, SEGY::HeaderField> m_attrFields;

    gdlog::GdLogger* m_logger;
    void * m_log_data;
    
    // Smart endianness detection
    SEGY::Endianness detectEndianness(const char* binaryHeader, const char* firstTraceHeader);
    
    // True OpenVDS segment building algorithm for single file
    void buildSegmentInfo(std::ifstream& file);
    
    // True OpenVDS representative segment finding for single file
    SEGYSegmentInfo findRepresentativeSegment(int& primaryStep);
    
    // OpenVDS secondary key analysis - Precise port of SEGYImport.cpp:1247+
    bool analyzeSegment(std::ifstream& file, const SEGYSegmentInfo& segmentInfo, int& secondaryStep, int& fold);
    
    // Calculate coordinate ranges based on segments for trace index conversion
    void calculateCoordinateRanges();
    
    // OpenVDS-style coordinate to sample index conversion
    int coordinateToSampleIndex(int coordinate, int coordinateMin, int coordinateMax, int numSamples) const;
    
    // Fast rectangular grid calculation for regular SEGY data
    int64_t calculateRectangularTraceIndex(int inlineNum, int crosslineNum);
    
    // Verify if calculated index matches actual coordinates in SEGY file
    bool verifyTraceCoordinates(std::ifstream& file, int64_t traceIndex, int expectedInline, int expectedCrossline);
    
    // Find trace number from inline/crossline coordinates using OpenVDS algorithm
    int64_t findTraceNumber(std::ifstream& file, int inlineNum, int crosslineNum);
    
    char ebcdicToAscii(unsigned char ebcdicChar);

    int getSampleCodeSize();
public:
    SEGYReader();
    
    void addCustomField(const std::string& name, int byteLocation, int width);

    void addAttrField(const std::string& name, int byteLocation, int width, SEGY::DataSampleFormatCode format);

    bool initialize(const std::string& filename);
    
    void printFileInfo();

    SEGY::DataSampleFormatCode getSampleFormatCode() const { return m_fileInfo.dataSampleFormatCode; }
    
    // Get trace number with rectangular assumption and fallback to precise search
    int64_t getTraceNumber(std::ifstream& file,int inlineNum, int crosslineNum);
    
    // Read trace data for a specific trace number
    bool readTrace(std::ifstream& file, int64_t trace_num, char *data);

    // Read multiple traces by primary index
    bool readTraceByPriIdx(int priIndex, int sndStart, int sndEnd,
                        int dataStart, int dataEnd, void* data); 

    // Read multiple attributes by primary index
    bool readAttrByPriIdx(std::string attr, int priIndex, int sndStart, int sndEnd, void* data); 

    bool printTextualHeader(std::string filename);

    bool getPrimaryKeyAxis(int& min_val, int& max_val, int& num_vals, int& step);

    bool getSecondaryKeyAxis(int& min_val, int& max_val, int& num_vals, int& step);
    
    bool getDataAxis(float& min_val, float& max_val, int& num_vals, int& sinterval);

    std::string getErrMsg() const { return m_lastError; }
    
};