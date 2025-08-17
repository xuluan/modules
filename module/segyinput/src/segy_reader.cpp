#include <arpa/inet.h>  // For ntohl, ntohs (network byte order conversion)
#include <GdLogger.h>
#include "segy_reader.h"

//#define DEBUG_EN 1

namespace SEGY {
    // OpenVDS-style ReadFieldFromHeader implementation
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

SEGYReader::SEGYReader() : m_initialized(false), m_log_data(NULL){
    // Initialize field aliases
    m_fieldAliases["inline"] = "inlinenumber";
    m_fieldAliases["crossline"] = "crosslinenumber"; 
    m_fieldAliases["iline"] = "inlinenumber";
    m_fieldAliases["xline"] = "crosslinenumber";
    m_logger = &gdlog::GdLogger::GetInstance();
}

void SEGYReader::addCustomField(const std::string& name, int byteLocation, int width) {
    std::string canonicalName = name;
    if (m_fieldAliases.find(name) != m_fieldAliases.end()) {
        canonicalName = m_fieldAliases[name];
    }
    
    int fieldWidth = width; // Support 1, 2, or 4 byte widths
    if (fieldWidth != 1 && fieldWidth != 2 && fieldWidth != 4) {
        fieldWidth = 2; // Default to 2 bytes for invalid widths
    }

    SEGY::DataSampleFormatCode format;

    switch (fieldWidth) {
        case 1: format = SEGY::DataSampleFormatCode::Int8; break;
        case 2: format = SEGY::DataSampleFormatCode::Int16; break;
        case 4: format = SEGY::DataSampleFormatCode::Int32; break;
        default: format = SEGY::DataSampleFormatCode::Unknown; break;
    }
    m_customFields[canonicalName] = SEGY::HeaderField(byteLocation, fieldWidth, format);
    std::cout << "Added custom field: " << canonicalName << " at byte " << byteLocation << " (width: " << width << ")" << std::endl;
}

void SEGYReader::addAttrField(const std::string& name, int byteLocation, int width, SEGY::DataSampleFormatCode format) {
    std::string canonicalName = name;
    if (m_fieldAliases.find(name) != m_fieldAliases.end()) {
        canonicalName = m_fieldAliases[name];
    }
    
    int fieldWidth = width; // Support 1, 2, or 4 byte widths
    if (fieldWidth != 1 && fieldWidth != 2 && fieldWidth != 4) {
        fieldWidth = 2; // Default to 2 bytes for invalid widths
    }
    m_attrFields[canonicalName] = SEGY::HeaderField(byteLocation, fieldWidth, format);
    std::cout << "Added custom field: " << canonicalName << " at byte " << byteLocation << " (width: " << width << ")" << std::endl;
}

SEGY::Endianness SEGYReader::detectEndianness(const char* binaryHeader, const char* firstTraceHeader) {
    int intervalBE_bin = SEGY::readFieldFromHeaderInt(binaryHeader, m_fileInfo.sampleIntervalKey, SEGY::Endianness::BigEndian);
    int intervalLE_bin = SEGY::readFieldFromHeaderInt(binaryHeader, m_fileInfo.sampleIntervalKey, SEGY::Endianness::LittleEndian);
    int samplesBE_bin = SEGY::readFieldFromHeaderInt(binaryHeader, m_fileInfo.numSamplesKey, SEGY::Endianness::BigEndian);
    int samplesLE_bin = SEGY::readFieldFromHeaderInt(binaryHeader, m_fileInfo.numSamplesKey, SEGY::Endianness::LittleEndian);
    int formatBE_bin = SEGY::readFieldFromHeaderInt(binaryHeader, m_fileInfo.dataSampleFormatCodeKey, SEGY::Endianness::BigEndian);
    int formatLE_bin = SEGY::readFieldFromHeaderInt(binaryHeader, m_fileInfo.dataSampleFormatCodeKey, SEGY::Endianness::LittleEndian);
    
    std::cout << "Endianness detection:" << std::endl;
    std::cout << "  Binary Header - BE: interval=" << intervalBE_bin << ", samples=" << samplesBE_bin << ", format=" << formatBE_bin << std::endl;
    std::cout << "  Binary Header - LE: interval=" << intervalLE_bin << ", samples=" << samplesLE_bin << ", format=" << formatLE_bin << std::endl;
    
    bool beValid = (intervalBE_bin > 0 && intervalBE_bin < 100000) &&
                   (samplesBE_bin > 0 && samplesBE_bin < 100000) &&
                   (formatBE_bin >= 1 && formatBE_bin <= 16);
                   
    bool leValid = (intervalLE_bin > 0 && intervalLE_bin < 100000) &&
                   (samplesLE_bin > 0 && samplesLE_bin < 100000) &&
                   (formatLE_bin >= 1 && formatLE_bin <= 16);
    
    if (beValid && !leValid) {
        std::cout << "  Selected: Big Endian" << std::endl;
        return SEGY::Endianness::BigEndian;
    } else if (leValid && !beValid) {
        std::cout << "  Selected: Little Endian" << std::endl;
        return SEGY::Endianness::LittleEndian;
    }
    
    std::cout << "  Selected: Big Endian (default)" << std::endl;
    return SEGY::Endianness::BigEndian;
}

void SEGYReader::buildSegmentInfo(std::ifstream& file) {
    
    std::cout << "=== Building Segment Information (Single File OpenVDS Algorithm) ===" << std::endl;
    
    m_fileInfo.segments.clear();
    std::vector<char> traceHeader(SEGY::TraceHeaderSize);
    
    int64_t lastTrace = m_fileInfo.totalTraces - 1;
    
    // Read first trace, start first segment
    file.seekg(SEGY::TextualFileHeaderSize + SEGY::BinaryFileHeaderSize);
    file.read(traceHeader.data(), SEGY::TraceHeaderSize);
    if (file.gcount() != SEGY::TraceHeaderSize) return;
    
    int currentPrimaryKey = SEGY::readFieldFromHeaderInt(traceHeader.data(), m_fileInfo.primaryKey, m_fileInfo.headerEndianness);
    SEGYSegmentInfo currentSegment(currentPrimaryKey, 0);
    
    std::cout << "Starting first segment with PrimaryKey: " << currentPrimaryKey << std::endl;
    
    // **OpenVDS core algorithm: Continuously scan each trace, detect primary key changes**
    for (int64_t trace = 1; trace <= lastTrace; trace++) {
        file.seekg(SEGY::TextualFileHeaderSize + SEGY::BinaryFileHeaderSize + trace * m_fileInfo.traceByteSize);
        file.read(traceHeader.data(), SEGY::TraceHeaderSize);
        
        if (file.gcount() != SEGY::TraceHeaderSize) break;
        
        int primaryKey = SEGY::readFieldFromHeaderInt(traceHeader.data(), m_fileInfo.primaryKey, m_fileInfo.headerEndianness);
        
        if (primaryKey == currentSegment.primaryKey) {
            // **OpenVDS logic: Extend current segment**
            currentSegment.traceStop = trace;
        } else {
            // **OpenVDS logic: Complete current segment, start new segment**
            m_fileInfo.segments.push_back(currentSegment);
#ifdef DEBUG_EN             
            std::cout << "Completed segment: PrimaryKey=" << currentSegment.primaryKey 
                      << ", Traces=[" << currentSegment.traceStart << "-" << currentSegment.traceStop << "]"
                      << ", Count=" << currentSegment.traceCount() << std::endl;
#endif            
            // Start new segment
            currentSegment = SEGYSegmentInfo(primaryKey, trace);
        }
#ifdef DEBUG_EN        
        // Report progress every 1000 traces
        if (trace % 1000 == 0) {
            std::cout << "  Processed " << trace << "/" << lastTrace << " traces, current segments: " << m_fileInfo.segments.size() + 1 << std::endl;
        }
#endif         
    }
    
    // Add final segment
    m_fileInfo.segments.push_back(currentSegment);
    std::cout << "Final segment: PrimaryKey=" << currentSegment.primaryKey 
              << ", Traces=[" << currentSegment.traceStart << "-" << currentSegment.traceStop << "]"
              << ", Count=" << currentSegment.traceCount() << std::endl;
    
    std::cout << "Total segments created: " << m_fileInfo.segments.size() << std::endl;
    
    // Verify segment integrity
    int64_t totalTracesInSegments = 0;
    for (const auto& seg : m_fileInfo.segments) {
        totalTracesInSegments += seg.traceCount();
    }
    std::cout << "Verification: Total traces in segments = " << totalTracesInSegments 
              << ", Expected = " << m_fileInfo.totalTraces << std::endl;
    
    if (totalTracesInSegments != m_fileInfo.totalTraces) {
        std::cout << "WARNING: Segment trace count mismatch!" << std::endl;
    }
}

SEGYSegmentInfo SEGYReader::findRepresentativeSegment(int& primaryStep) {
    
    primaryStep = 0;
    
    if (m_fileInfo.segments.empty()) {
        return SEGYSegmentInfo(0, 0);
    }
    
    // **OpenVDS: Single file segment analysis**
    size_t totalSegments = m_fileInfo.segments.size();
    
    float bestScore = 0.0f;
    size_t bestIndex = 0;
    int segmentPrimaryStep = 0;
    
    std::cout << "\n=== OpenVDS Representative Segment Analysis (Single File) ===" << std::endl;
    std::cout << "Total segments: " << totalSegments << std::endl;
    
    // **OpenVDS core algorithm: Iterate through all segments**
    for (size_t i = 0; i < m_fileInfo.segments.size(); i++) {
        int64_t numTraces = m_fileInfo.segments[i].traceCount();
        
        // **Precise OpenVDS weight formula**
        float multiplier = 1.5f - std::abs(static_cast<float>(i) - static_cast<float>(totalSegments) / 2.0f) / static_cast<float>(totalSegments);
        
        float score = static_cast<float>(numTraces) * multiplier;

#ifdef DEBUG_EN        
        // Show detailed analysis process
        if (i < 5 || score > bestScore || (i % 50 == 0)) {
            std::cout << "  Index=" << i 
                      << ", PrimaryKey=" << m_fileInfo.segments[i].primaryKey 
                      << ", Traces=" << numTraces 
                      << ", Multiplier=" << std::fixed << std::setprecision(4) << multiplier
                      << ", Score=" << score;
            if (score > bestScore) std::cout << " [NEW BEST]";
            std::cout << std::endl;
        }
#endif        
        if (score > bestScore) {
            bestScore = score;
            bestIndex = i;
        }
        
        // **OpenVDS logic: Primary step calculation, intentionally ignore last segment**
        if (segmentPrimaryStep && (!primaryStep || std::abs(segmentPrimaryStep) < std::abs(primaryStep))) {
            primaryStep = segmentPrimaryStep;
        }
        
        if (i > 0) {
            segmentPrimaryStep = m_fileInfo.segments[i].primaryKey - m_fileInfo.segments[i - 1].primaryKey;
        }
    }
    
    // **OpenVDS post-processing**
    primaryStep = primaryStep ? primaryStep : std::max(segmentPrimaryStep, 1);
    
    std::cout << "\nSelected representative segment (Single File OpenVDS Algorithm):" << std::endl;
    std::cout << "  Index: " << bestIndex << std::endl;
    std::cout << "  PrimaryKey: " << m_fileInfo.segments[bestIndex].primaryKey << std::endl;
    std::cout << "  Score: " << bestScore << std::endl;
    std::cout << "  Primary Step: " << primaryStep << std::endl;
    
    return m_fileInfo.segments[bestIndex];
}

bool SEGYReader::analyzeSegment(std::ifstream& file, const SEGYSegmentInfo& segmentInfo, int& secondaryStep, int& fold) {
    
    std::cout << "\n=== OpenVDS Secondary Key Analysis (Single File) ===" << std::endl;
    std::cout << "Analyzing segment - PrimaryKey: " << segmentInfo.primaryKey 
              << ", Traces: " << segmentInfo.traceStart << "-" << segmentInfo.traceStop 
              << " (Count: " << segmentInfo.traceCount() << ")" << std::endl;
    
    secondaryStep = 0;
    fold = 1;
    
    // **OpenVDS gather analysis variables**
    int gatherSecondaryKey = 0, gatherFold = 0, gatherSecondaryStep = 0;
    std::map<int, int> secondaryKeyCount;
    
    int tracesAnalyzed = 0;
    int maxAnalyzeTraces = std::min(static_cast<int64_t>(2000), segmentInfo.traceCount());
    
    std::cout << "Will analyze " << maxAnalyzeTraces << " traces from this segment" << std::endl;
    
    for (int64_t trace = segmentInfo.traceStart; trace <= segmentInfo.traceStop && tracesAnalyzed < maxAnalyzeTraces; trace++) {
        std::vector<char> traceHeader(SEGY::TraceHeaderSize);
        file.seekg(SEGY::TextualFileHeaderSize + SEGY::BinaryFileHeaderSize + trace * m_fileInfo.traceByteSize);
        file.read(traceHeader.data(), SEGY::TraceHeaderSize);
        
        if (file.gcount() != SEGY::TraceHeaderSize) break;
        
        // **OpenVDS validation: Ensure trace belongs to correct segment**
        int tracePrimaryKey = SEGY::readFieldFromHeaderInt(traceHeader.data(), m_fileInfo.primaryKey, m_fileInfo.headerEndianness);
        if (tracePrimaryKey != segmentInfo.primaryKey) {
            std::cout << "Warning: trace " << trace << " has mismatched primary key " 
                      << tracePrimaryKey << " vs expected " << segmentInfo.primaryKey << std::endl;
            continue;
        }
        
        // Read secondary key
        int traceSecondaryKey = SEGY::readFieldFromHeaderInt(traceHeader.data(), m_fileInfo.secondaryKey, m_fileInfo.headerEndianness);
        
        // **Precise OpenVDS gather analysis logic**
        if (gatherFold > 0 && traceSecondaryKey == gatherSecondaryKey) {
            // Within same gather
            gatherFold++;
            fold = std::max(fold, gatherFold);
        } else {
            // New gather starts - **OpenVDS key logic: Intentionally ignore last gather's step**
            if (gatherSecondaryStep && (!secondaryStep || std::abs(gatherSecondaryStep) < std::abs(secondaryStep))) {
                secondaryStep = gatherSecondaryStep;
            }
            
            if (gatherFold > 0) {
                gatherSecondaryStep = traceSecondaryKey - gatherSecondaryKey;
            }
            
            gatherSecondaryKey = traceSecondaryKey;
            gatherFold = 1;
        }
        
        secondaryKeyCount[traceSecondaryKey]++;
        tracesAnalyzed++;
        
        // Report progress
        if (tracesAnalyzed % 200 == 0) {
            std::cout << "  Analyzed " << tracesAnalyzed << " traces, current secondary step: " << secondaryStep << std::endl;
        }
    }
    
    // **OpenVDS post-processing**
    secondaryStep = secondaryStep ? secondaryStep : std::max(gatherSecondaryStep, 1);
    
    std::cout << "Analysis complete:" << std::endl;
    std::cout << "  Traces analyzed: " << tracesAnalyzed << std::endl;
    std::cout << "  Secondary step: " << secondaryStep << std::endl;
    std::cout << "  Maximum fold: " << fold << std::endl;
    std::cout << "  Unique secondary keys: " << secondaryKeyCount.size() << std::endl;
    
    // Show secondary key distribution statistics
    if (!secondaryKeyCount.empty()) {
        auto minMax = std::minmax_element(secondaryKeyCount.begin(), secondaryKeyCount.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });
        std::cout << "  Secondary key range: [" << minMax.first->first << " - " << minMax.second->first << "]" << std::endl;
        
        std::cout << "  Sample distribution (first 10):" << std::endl;
        int count = 0;
        for (const auto& entry : secondaryKeyCount) {
            std::cout << "    SecondaryKey " << entry.first << ": " << entry.second << " traces" << std::endl;
            if (++count >= 10) break;
        }
    }
    
    return true;
}

void SEGYReader::calculateCoordinateRanges() {
    if (m_fileInfo.segments.empty()) return;
    
    std::cout << "\n=== Calculating Coordinate Ranges for Trace Index Conversion ===" << std::endl;
    
    // Determine if primary key is inline or crossline
    m_fileInfo.isPrimaryInline = 1; //(m_fileInfo.primaryKey.byteLocation == SEGY::TraceHeader::InlineNumberHeaderField.byteLocation);
    
    // Initialize ranges
    m_fileInfo.minInline = INT_MAX;
    m_fileInfo.maxInline = INT_MIN;
    m_fileInfo.minCrossline = INT_MAX;
    m_fileInfo.maxCrossline = INT_MIN;
    
    std::set<int> inlines, crosslines;
    
    // Scan through segments to find coordinate ranges
    std::ifstream file(m_filename, std::ios::binary);
    if (!file) return;
    
    std::vector<char> traceHeader(SEGY::TraceHeaderSize);
    int sampledTraces = 0;
    
    for (const auto& segment : m_fileInfo.segments) {
        
        for (int64_t trace = segment.traceStart; trace <= segment.traceStop; ++trace) {
            file.seekg(SEGY::TextualFileHeaderSize + SEGY::BinaryFileHeaderSize + trace * m_fileInfo.traceByteSize);
            file.read(traceHeader.data(), SEGY::TraceHeaderSize);
            
            if (file.gcount() != SEGY::TraceHeaderSize) break;
            
            int inlineNum = SEGY::readFieldFromHeaderInt(traceHeader.data(), 
                (m_fileInfo.isPrimaryInline ? m_fileInfo.primaryKey : m_fileInfo.secondaryKey), 
                m_fileInfo.headerEndianness);
            int crosslineNum = SEGY::readFieldFromHeaderInt(traceHeader.data(), 
                (m_fileInfo.isPrimaryInline ? m_fileInfo.secondaryKey : m_fileInfo.primaryKey), 
                m_fileInfo.headerEndianness);
            
            inlines.insert(inlineNum);
            crosslines.insert(crosslineNum);
            
            m_fileInfo.minInline = std::min(m_fileInfo.minInline, inlineNum);
            m_fileInfo.maxInline = std::max(m_fileInfo.maxInline, inlineNum);
            m_fileInfo.minCrossline = std::min(m_fileInfo.minCrossline, crosslineNum);
            m_fileInfo.maxCrossline = std::max(m_fileInfo.maxCrossline, crosslineNum);
            
            sampledTraces++;
        }
    }
    
    m_fileInfo.inlineCount = inlines.size();
    m_fileInfo.crosslineCount = crosslines.size();
    
    std::cout << "Coordinate ranges calculated:" << std::endl;
    std::cout << "  Inline range: [" << m_fileInfo.minInline << " - " << m_fileInfo.maxInline << "] (" << m_fileInfo.inlineCount << " unique values)" << std::endl;
    std::cout << "  Crossline range: [" << m_fileInfo.minCrossline << " - " << m_fileInfo.maxCrossline << "] (" << m_fileInfo.crosslineCount << " unique values)" << std::endl;
    std::cout << "  Primary key is: " << (m_fileInfo.isPrimaryInline ? "Inline" : "Crossline") << std::endl;
    std::cout << "  Sampled " << sampledTraces << " traces for range calculation" << std::endl;
}

int SEGYReader::coordinateToSampleIndex(int coordinate, int coordinateMin, int coordinateMax, int numSamples) const {
    if (coordinate == coordinateMin) return 0;
    if (numSamples <= 1) return 0;
    
    // OpenVDS formula: floor(((coordinate - coordinateMin) / (coordinateMax - coordinateMin)) * (numSamples - 1) + 0.5)
    float normalized = static_cast<float>(coordinate - coordinateMin) / static_cast<float>(coordinateMax - coordinateMin);
    return static_cast<int>(std::floor(normalized * (numSamples - 1) + 0.5f));
}

int64_t SEGYReader::calculateRectangularTraceIndex(int inlineNum, int crosslineNum) {
    // Validate coordinates are within range
    if (inlineNum < m_fileInfo.minInline || inlineNum > m_fileInfo.maxInline ||
        crosslineNum < m_fileInfo.minCrossline || crosslineNum > m_fileInfo.maxCrossline) {
        return -1; // Invalid coordinates
    }
    
    // Calculate grid position assuming regular rectangular layout
    int inlineOffset = (inlineNum - m_fileInfo.minInline) / (m_fileInfo.primaryStep > 0 ? m_fileInfo.primaryStep : 1);
    int crosslineOffset = (crosslineNum - m_fileInfo.minCrossline) / (m_fileInfo.secondaryStep > 0 ? m_fileInfo.secondaryStep : 1);
    
    int64_t calculatedIndex;
    
    if (m_fileInfo.isPrimaryInline) {
        calculatedIndex = static_cast<int64_t>(m_fileInfo.crosslineCount) * inlineOffset + crosslineOffset;
    } else {
        calculatedIndex = static_cast<int64_t>(m_fileInfo.inlineCount) * crosslineOffset + inlineOffset;
    }
    
    // Ensure index is within valid trace range
    if (calculatedIndex < 0 || calculatedIndex >= m_fileInfo.totalTraces) {
        return -1;
    }
        
    std::cout << "calculateRectangularTraceIndex " << calculatedIndex << std::endl;

    return calculatedIndex;
}

bool SEGYReader::verifyTraceCoordinates(std::ifstream& file, int64_t traceIndex, int expectedInline, int expectedCrossline) {
    
    std::vector<char> traceHeader(SEGY::TraceHeaderSize);
    file.seekg(SEGY::TextualFileHeaderSize + SEGY::BinaryFileHeaderSize + traceIndex * m_fileInfo.traceByteSize);
    file.read(traceHeader.data(), SEGY::TraceHeaderSize);
    
    if (file.gcount() != SEGY::TraceHeaderSize) return false;
    
    int actualInline = SEGY::readFieldFromHeaderInt(traceHeader.data(), 
        (m_fileInfo.isPrimaryInline ? m_fileInfo.primaryKey : m_fileInfo.secondaryKey), 
        m_fileInfo.headerEndianness);
    int actualCrossline = SEGY::readFieldFromHeaderInt(traceHeader.data(), 
        (m_fileInfo.isPrimaryInline ? m_fileInfo.secondaryKey : m_fileInfo.primaryKey), 
        m_fileInfo.headerEndianness);
        
    return (actualInline == expectedInline && actualCrossline == expectedCrossline);
}

int64_t SEGYReader::getTraceNumber(std::ifstream& file, int inlineNum, int crosslineNum) {
    std::cout << "\n=== Fast Trace Finder (Rectangular Grid + Fallback) ===" << std::endl;
    std::cout << "Input: Inline=" << inlineNum << ", Crossline=" << crosslineNum << std::endl;
    
    // Step 1: Try fast rectangular calculation
    std::cout << "\nStep 1: Attempting rectangular grid calculation..." << std::endl;
    std::cout << "Grid parameters:" << std::endl;
    std::cout << "  Primary key: " << (m_fileInfo.isPrimaryInline ? "Inline" : "Crossline") << std::endl;
    std::cout << "  Primary step: " << m_fileInfo.primaryStep << std::endl;
    std::cout << "  Secondary step: " << m_fileInfo.secondaryStep << std::endl;
    std::cout << "  Inline range: [" << m_fileInfo.minInline << " - " << m_fileInfo.maxInline << "] (" << m_fileInfo.inlineCount << " values)" << std::endl;
    std::cout << "  Crossline range: [" << m_fileInfo.minCrossline << " - " << m_fileInfo.maxCrossline << "] (" << m_fileInfo.crosslineCount << " values)" << std::endl;
    
    int64_t rectangularIndex = calculateRectangularTraceIndex(inlineNum, crosslineNum);
    
    if (rectangularIndex < 0) {
        std::cout << "Rectangular calculation failed: coordinates out of range" << std::endl;
        return findTraceNumber(file, inlineNum, crosslineNum); // Fallback to precise search
    }
    
    std::cout << "Rectangular calculation result: trace " << rectangularIndex << std::endl;
    
    // Step 2: Verify the calculated index
    std::cout << "\nStep 2: Verifying calculated index..." << std::endl;
    bool isCorrect = verifyTraceCoordinates(file, rectangularIndex, inlineNum, crosslineNum);
    
    if (isCorrect) {
        std::cout << "✓ Verification successful: rectangular calculation is correct!" << std::endl;
        std::cout << "Fast result: Inline " << inlineNum << ", Crossline " << crosslineNum 
                  << " -> Trace " << rectangularIndex << std::endl;
        return rectangularIndex;
    } else {
        std::cout << "✗ Verification failed: SEGY data is not regular rectangular grid" << std::endl;
        std::cout << "Falling back to precise OpenVDS algorithm..." << std::endl;
        return findTraceNumber(file, inlineNum, crosslineNum); // Fallback to precise search
    }
}

int64_t SEGYReader::findTraceNumber(std::ifstream& file, int inlineNum, int crosslineNum) {
    std::cout << "\n=== Finding Trace Number (OpenVDS Algorithm) ===" << std::endl;
    std::cout << "Input: Inline=" << inlineNum << ", Crossline=" << crosslineNum << std::endl;
    
    // Validate coordinates are within range
    if (inlineNum < m_fileInfo.minInline || inlineNum > m_fileInfo.maxInline ||
        crosslineNum < m_fileInfo.minCrossline || crosslineNum > m_fileInfo.maxCrossline) {
        std::cout << "Error: Coordinates outside valid range" << std::endl;
        std::cout << "  Valid inline range: [" << m_fileInfo.minInline << " - " << m_fileInfo.maxInline << "]" << std::endl;
        std::cout << "  Valid crossline range: [" << m_fileInfo.minCrossline << " - " << m_fileInfo.maxCrossline << "]" << std::endl;
        return -1;
    }
    
    // Step 1: Convert coordinates to indices using OpenVDS algorithm
    int inlineIndex = coordinateToSampleIndex(inlineNum, m_fileInfo.minInline, m_fileInfo.maxInline, m_fileInfo.inlineCount);
    int crosslineIndex = coordinateToSampleIndex(crosslineNum, m_fileInfo.minCrossline, m_fileInfo.maxCrossline, m_fileInfo.crosslineCount);
    
    std::cout << "Coordinate to index conversion:" << std::endl;
    std::cout << "  Inline " << inlineNum << " -> index " << inlineIndex << std::endl;
    std::cout << "  Crossline " << crosslineNum << " -> index " << crosslineIndex << std::endl;
    
    // Step 2: Determine primary and secondary indices based on key ordering
    int primaryIndex, secondaryIndex;
    int primaryCoord, secondaryCoord;
    
    if (m_fileInfo.isPrimaryInline) {
        primaryIndex = inlineIndex;
        secondaryIndex = crosslineIndex;
        primaryCoord = inlineNum;
        secondaryCoord = crosslineNum;
    } else {
        primaryIndex = crosslineIndex;
        secondaryIndex = inlineIndex;
        primaryCoord = crosslineNum;
        secondaryCoord = inlineNum;
    }
    
    std::cout << "Primary/Secondary mapping:" << std::endl;
    std::cout << "  Primary key (" << (m_fileInfo.isPrimaryInline ? "Inline" : "Crossline") << "): " << primaryCoord << " -> index " << primaryIndex << std::endl;
    std::cout << "  Secondary key (" << (m_fileInfo.isPrimaryInline ? "Crossline" : "Inline") << "): " << secondaryCoord << " -> index " << secondaryIndex << std::endl;
    
    // Step 3: Find the segment containing this primary key
    SEGYSegmentInfo* targetSegment = nullptr;
    for (auto& segment : m_fileInfo.segments) {
        if (segment.primaryKey == primaryCoord) {
            targetSegment = &segment;
            break;
        }
    }
    
    if (!targetSegment) {
        std::cout << "Error: No segment found for primary key " << primaryCoord << std::endl;
        return -1;
    }
    
    std::cout << "Found target segment:" << std::endl;
    std::cout << "  Primary key: " << targetSegment->primaryKey << std::endl;
    std::cout << "  Trace range: [" << targetSegment->traceStart << " - " << targetSegment->traceStop << "]" << std::endl;
    std::cout << "  Trace count: " << targetSegment->traceCount() << std::endl;
    
    std::vector<char> traceHeader(SEGY::TraceHeaderSize);
    
    for (int64_t trace = targetSegment->traceStart; trace <= targetSegment->traceStop; trace++) {
        file.seekg(SEGY::TextualFileHeaderSize + SEGY::BinaryFileHeaderSize + trace * m_fileInfo.traceByteSize);
        file.read(traceHeader.data(), SEGY::TraceHeaderSize);
        
        if (file.gcount() != SEGY::TraceHeaderSize) break;
        
        int traceInline = SEGY::readFieldFromHeaderInt(traceHeader.data(), 
            (m_fileInfo.isPrimaryInline ? m_fileInfo.primaryKey : m_fileInfo.secondaryKey), 
            m_fileInfo.headerEndianness);
        int traceCrossline = SEGY::readFieldFromHeaderInt(traceHeader.data(), 
            (m_fileInfo.isPrimaryInline ? m_fileInfo.secondaryKey : m_fileInfo.primaryKey), 
            m_fileInfo.headerEndianness);
        
        if (traceInline == inlineNum && traceCrossline == crosslineNum) {
            std::cout << "Found exact match at trace " << trace << std::endl;
            std::cout << "  Verification: Inline=" << traceInline << ", Crossline=" << traceCrossline << std::endl;
            return trace;
        }
    }
    
    std::cout << "Warning: Exact coordinate match not found in segment" << std::endl;
    
    // Step 5: Fallback - calculate estimated position within segment
    int64_t estimatedOffset = static_cast<int64_t>(secondaryIndex * (targetSegment->traceCount() / static_cast<float>(m_fileInfo.isPrimaryInline ? m_fileInfo.crosslineCount : m_fileInfo.inlineCount)));
    int64_t estimatedTrace = targetSegment->traceStart + estimatedOffset;
    
    if (estimatedTrace > targetSegment->traceStop) {
        estimatedTrace = targetSegment->traceStop;
    }
    
    std::cout << "Estimated trace position: " << estimatedTrace << " (offset " << estimatedOffset << " within segment)" << std::endl;
    return estimatedTrace;
}

bool SEGYReader::initialize(const std::string& m_filename) {
    this->m_filename = m_filename;
    
    std::ifstream file(m_filename, std::ios::binary);
    if (!file) {
        std::cerr << "Error: Cannot open file " << m_filename << std::endl;
        return false;
    }

    // Set primary and secondary key fields
    m_fileInfo.primaryKey = m_customFields.find("inlinenumber") != m_customFields.end() ? 
        m_customFields["inlinenumber"] : SEGY::TraceHeader::InlineNumberHeaderField;
    m_fileInfo.secondaryKey = m_customFields.find("crosslinenumber") != m_customFields.end() ? 
        m_customFields["crosslinenumber"] : SEGY::TraceHeader::CrosslineNumberHeaderField;
    m_fileInfo.numSamplesKey = m_customFields.find("numSamplesKey") != m_customFields.end() ? 
        m_customFields["numSamplesKey"] : SEGY::BinaryHeader::NumSamplesHeaderField;
    m_fileInfo.sampleIntervalKey = m_customFields.find("sampleIntervalKey") != m_customFields.end() ?
        m_customFields["sampleIntervalKey"] : SEGY::BinaryHeader::SampleIntervalHeaderField;
    m_fileInfo.dataSampleFormatCodeKey = m_customFields.find("dataSampleFormatCodeKey") != m_customFields.end() ?
        m_customFields["dataSampleFormatCodeKey"] : SEGY::BinaryHeader::DataSampleFormatCodeHeaderField;

    
    // Get file size
    file.seekg(0, std::ios::end);
    int64_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    
    // Read headers
    std::vector<char> textualHeader(SEGY::TextualFileHeaderSize);
    std::vector<char> binaryHeader(SEGY::BinaryFileHeaderSize);
    std::vector<char> firstTraceHeader(SEGY::TraceHeaderSize);
    
    file.read(textualHeader.data(), SEGY::TextualFileHeaderSize);
    file.read(binaryHeader.data(), SEGY::BinaryFileHeaderSize);
    file.read(firstTraceHeader.data(), SEGY::TraceHeaderSize);
    
    if (file.gcount() != SEGY::TraceHeaderSize) {
        std::cerr << "Error: Cannot read trace header" << std::endl;
        return false;
    }
    
    // Detect endianness
    m_fileInfo.headerEndianness = detectEndianness(binaryHeader.data(), firstTraceHeader.data());
    
    // Read file parameters
    int interval = SEGY::readFieldFromHeaderInt(binaryHeader.data(), m_fileInfo.sampleIntervalKey, m_fileInfo.headerEndianness);
    int samples = SEGY::readFieldFromHeaderInt(binaryHeader.data(), m_fileInfo.numSamplesKey, m_fileInfo.headerEndianness);
    int format = SEGY::readFieldFromHeaderInt(binaryHeader.data(), m_fileInfo.dataSampleFormatCodeKey, m_fileInfo.headerEndianness);

    // If binary header invalid, read from trace header
    if (interval <= 0 || samples <= 0) {
        std::cout << "Binary header invalid, using trace header..." << std::endl;
        return false;
        /*
        if (interval <= 0) {
            interval = SEGY::readFieldFromHeaderInt(firstTraceHeader.data(), SEGY::TraceHeader::SampleIntervalHeaderField, m_fileInfo.headerEndianness);
        }
        if (samples <= 0) {
            samples = SEGY::readFieldFromHeaderInt(firstTraceHeader.data(), SEGY::TraceHeader::NumSamplesHeaderField, m_fileInfo.headerEndianness);
        }
        */
    }

    m_fileInfo.sampleInterval = interval > 0 ? interval : 4000 ;
    m_fileInfo.sampleCount = samples > 0 ? samples : 1000;
    m_fileInfo.dataSampleFormatCode = static_cast<SEGY::DataSampleFormatCode>(format > 0 ? format : 5);
    
    // Calculate trace size and total trace count
    int sampleSize = 4; // Default 4 bytes
    switch(m_fileInfo.dataSampleFormatCode) {
        case SEGY::DataSampleFormatCode::Int16:
            sampleSize = 2; break;
        default:
            sampleSize = 4; break;
    }
    
    m_fileInfo.traceByteSize = SEGY::TraceHeaderSize + m_fileInfo.sampleCount * sampleSize;
    int64_t dataSize = fileSize - SEGY::TextualFileHeaderSize - SEGY::BinaryFileHeaderSize;
    m_fileInfo.totalTraces = dataSize / m_fileInfo.traceByteSize;
    
    // **Use true OpenVDS segment building algorithm**
    buildSegmentInfo(file);
    
    // Calculate coordinate ranges for trace index conversion
    calculateCoordinateRanges();
    
    //CalculateStepsWithOpenVDSMethod 
    std::cout << "\n=== OpenVDS Single File Step Calculation ===" << std::endl;
    
    int primaryStep = 0;
    int secondaryStep = 0;
    int fold = 1;
    
    // Use true OpenVDS segment analysis method
    SEGYSegmentInfo representativeSegment = findRepresentativeSegment(primaryStep);
    
    // Analyze secondary step of representative segment
    analyzeSegment(file, representativeSegment, secondaryStep, fold);
    
    // Store calculated steps for fast lookup
    m_fileInfo.primaryStep = primaryStep;
    m_fileInfo.secondaryStep = secondaryStep;
    
    // Output final results
    std::cout << "\n=== Final Results (OpenVDS Single File) ===" << std::endl;
    std::cout << "Primary Step (inline): " << primaryStep << std::endl;
    std::cout << "Secondary Step (crossline): " << secondaryStep << std::endl;
    std::cout << "Maximum Fold: " << fold << std::endl;
    std::cout << "Analysis Method: True OpenVDS Algorithm (Single File)" << std::endl;
    std::cout << "Representative Segment: PrimaryKey=" << representativeSegment.primaryKey 
              << ", Score=" << std::fixed << std::setprecision(2) << representativeSegment.score << std::endl;
    std::cout << "Algorithm Fidelity: Professional Grade (Single File)" << std::endl;

    m_initialized = true;
    
    return true;
}

void SEGYReader::printFileInfo() {
    std::cout << "\n=== OpenVDS Single File SEGY Analysis ===" << std::endl;
    std::cout << "File: " << m_filename << std::endl;
    std::cout << "Header Endianness: " << (m_fileInfo.headerEndianness == SEGY::Endianness::BigEndian ? "Big Endian" : "Little Endian") << std::endl;
    std::cout << "Sample Interval: " << m_fileInfo.sampleInterval << " us" << std::endl;
    std::cout << "Samples per Trace: " << m_fileInfo.sampleCount << std::endl;
    std::cout << "Trace Size: " << m_fileInfo.traceByteSize << " bytes" << std::endl;
    std::cout << "Total Traces: " << m_fileInfo.totalTraces << std::endl;
    
    // Show segment statistics
    if (!m_fileInfo.segments.empty()) {
        std::cout << "Total Segments: " << m_fileInfo.segments.size() << std::endl;
        
        // Calculate segment statistics
        int64_t minTraces = m_fileInfo.segments[0].traceCount();
        int64_t maxTraces = m_fileInfo.segments[0].traceCount();
        int64_t totalSegmentTraces = 0;
        
        for (const auto& seg : m_fileInfo.segments) {
            int64_t count = seg.traceCount();
            minTraces = std::min(minTraces, count);
            maxTraces = std::max(maxTraces, count);
            totalSegmentTraces += count;
        }
        
        std::cout << "Segment Statistics:" << std::endl;
        std::cout << "  Min traces per segment: " << minTraces << std::endl;
        std::cout << "  Max traces per segment: " << maxTraces << std::endl;
        std::cout << "  Average traces per segment: " << (totalSegmentTraces / m_fileInfo.segments.size()) << std::endl;
        
        // Show first few segments
        std::cout << "First few segments:" << std::endl;
        for (size_t i = 0; i < std::min(m_fileInfo.segments.size(), static_cast<size_t>(5)); i++) {
            const auto& seg = m_fileInfo.segments[i];
            std::cout << "  Segment " << i << ": PrimaryKey=" << seg.primaryKey 
                      << ", Traces=[" << seg.traceStart << "-" << seg.traceStop << "]"
                      << ", Count=" << seg.traceCount() << std::endl;
        }
    }
}

int SEGYReader::getSampleCodeSize() {
    int size = 4;
    switch (m_fileInfo.dataSampleFormatCode) {
        case SEGY::DataSampleFormatCode::IEEEFloat: {
            size = 4; break;
        }
        case SEGY::DataSampleFormatCode::Int32: {
            size = 4; break;
        }
        case SEGY::DataSampleFormatCode::Int8: {
            size = 1; break;
        }
        case SEGY::DataSampleFormatCode::Int16: {
            size = 2; break;
        }
        default:
            size = 4;
    }
    return size;    
}

bool SEGYReader::readTrace(std::ifstream& file, int64_t trace_num, char *data) {

    if(!data) {
        m_lastError = "Invalid data point";
        return false;        
    }

    // Calculate file position for this trace
    // SEGY format: 3200 byte textual header + 400 byte binary header + traces
    int64_t traceStartOffset = 3600 + trace_num * m_fileInfo.traceByteSize + 240;
    
    file.seekg(traceStartOffset);
    if (!file.good()) {
        m_lastError = "Failed to seek to trace position";
        return false;
    }

    int size = getSampleCodeSize();

    // Read sample data
    file.read(static_cast<char*>(data), m_fileInfo.sampleCount * size);
    if (file.gcount() != static_cast<std::streamsize>(m_fileInfo.sampleCount * size)) {
        m_lastError = "Failed to read complete trace data";
        return false;
    }

    // Convert binary data to float based on data format
    if (m_fileInfo.headerEndianness == SEGY::Endianness::BigEndian) {
        switch (m_fileInfo.dataSampleFormatCode) {
            case SEGY::DataSampleFormatCode::IEEEFloat: {
                // IEEE 32-bit floating point
                float* floatData = reinterpret_cast<float*>(data);
                for (int i = 0; i < m_fileInfo.sampleCount; i++) {
                    float value = floatData[i];
                    uint32_t temp;
                    std::memcpy(&temp, &value, 4);
                    temp = ntohl(temp);
                    std::memcpy(&value, &temp, 4);
                    floatData[i] = value;
                }
                break;
            }
            case SEGY::DataSampleFormatCode::Int32: {
                // 32-bit integer
                int32_t* intData = reinterpret_cast<int32_t*>(data);
                for (int i = 0; i < m_fileInfo.sampleCount; i++) {
                    int32_t value = ntohl(intData[i]);
                    intData[i] = static_cast<float>(value);
                }
                break;
            }
            case SEGY::DataSampleFormatCode::Int16: {
                // 16-bit integer
                int16_t* shortData = reinterpret_cast<int16_t*>(data);
                for (int i = 0; i < m_fileInfo.sampleCount; i++) {
                    int16_t value = ntohs(shortData[i]);
                    shortData[i] = static_cast<float>(value);
                }
                break;
            }
            default:
                // Fallback to IEEE Float format
                float* floatData = reinterpret_cast<float*>(data);
                for (int i = 0; i < m_fileInfo.sampleCount; i++) {
                    float value = floatData[i];
                    uint32_t temp;
                    std::memcpy(&temp, &value, 4);
                    temp = ntohl(temp);
                    std::memcpy(&value, &temp, 4);
                    floatData[i] = value;
                }
                break;
        }    
    }
    
    return true;
}

bool SEGYReader::readTrace(int inline_num, int crossline_num, std::vector<float>& traceData) {
    if (!m_initialized) {
        m_lastError = "SEGY reader not initialized";
        return false;
    }
    
    // Check bounds
    if (inline_num < m_fileInfo.minInline || inline_num > m_fileInfo.maxInline ||
        crossline_num < m_fileInfo.minCrossline || crossline_num > m_fileInfo.maxCrossline) {
        m_lastError = "Inline/Crossline out of bounds";
        return false;
    }
    
    try {
        // Resize trace data vector
        traceData.resize(m_fileInfo.sampleCount);
        // Open SEGY file for reading
        std::ifstream file(m_filename, std::ios::binary);
        if (!file.is_open()) {
            m_lastError = "Cannot open SEGY file for reading";
            return false;
        }
                
        // Calculate trace number from inline/crossline coordinates  
        int64_t traceNumber = getTraceNumber(file, inline_num, crossline_num);
        if (traceNumber < 0) {
            m_lastError = "Invalid trace coordinates";
            return false;
        }
        

        // Calculate file position for this trace
        // SEGY format: 3200 byte textual header + 400 byte binary header + traces
        int64_t traceStartOffset = 3600 + traceNumber * m_fileInfo.traceByteSize;
        
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
        if (m_fileInfo.headerEndianness == SEGY::Endianness::BigEndian) {
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
                    
                    int64_t searchOffset = 3600 + searchTraceNum * m_fileInfo.traceByteSize;
                    file.seekg(searchOffset);
                    
                    if (file.read(traceHeader, 240) && file.gcount() == 240) {
                        std::memcpy(&fileInline, &traceHeader[188], 4);
                        std::memcpy(&fileCrossline, &traceHeader[192], 4);

                        if (m_fileInfo.headerEndianness == SEGY::Endianness::BigEndian) {
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
        std::vector<char> rawSampleData(m_fileInfo.sampleCount * 4); // Assume 4 bytes per sample
        file.read(rawSampleData.data(), rawSampleData.size());
        if (file.gcount() != static_cast<std::streamsize>(rawSampleData.size())) {
            m_lastError = "Failed to read complete trace data";
            file.close();
            return false;
        }
        
        file.close();
        
        // Convert binary data to float based on data format
        switch (m_fileInfo.dataSampleFormatCode) {
            case SEGY::DataSampleFormatCode::IEEEFloat: {
                // IEEE 32-bit floating point
                const float* floatData = reinterpret_cast<const float*>(rawSampleData.data());
                for (int i = 0; i < m_fileInfo.sampleCount; i++) {
                    float value = floatData[i];
                    // Handle endianness conversion if needed
                    if (m_fileInfo.headerEndianness == SEGY::Endianness::BigEndian) {
                        uint32_t temp;
                        std::memcpy(&temp, &value, 4);
                        temp = ntohl(temp);
                        std::memcpy(&value, &temp, 4);
                    }
                    traceData[i] = value;
                }
                break;
            }
            case SEGY::DataSampleFormatCode::Int32: {
                // 32-bit integer
                const int32_t* intData = reinterpret_cast<const int32_t*>(rawSampleData.data());
                for (int i = 0; i < m_fileInfo.sampleCount; i++) {
                    int32_t value = intData[i];
                    // Handle endianness conversion if needed
                    if (m_fileInfo.headerEndianness == SEGY::Endianness::BigEndian) {
                        value = ntohl(value);
                    }
                    traceData[i] = static_cast<float>(value);
                }
                break;
            }
            case SEGY::DataSampleFormatCode::Int16: {
                // 16-bit integer
                const int16_t* shortData = reinterpret_cast<const int16_t*>(rawSampleData.data());
                for (int i = 0; i < m_fileInfo.sampleCount; i++) {
                    int16_t value = shortData[i];
                    // Handle endianness conversion if needed
                    if (m_fileInfo.headerEndianness == SEGY::Endianness::BigEndian) {
                        value = ntohs(value);
                    }
                    traceData[i] = static_cast<float>(value);
                }
                break;
            }
            default:
                // Fallback to IEEE Float format
                const float* floatData = reinterpret_cast<const float*>(rawSampleData.data());
                for (int i = 0; i < m_fileInfo.sampleCount; i++) {
                    float value = floatData[i];
                    if (m_fileInfo.headerEndianness == SEGY::Endianness::BigEndian) {
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
    if (inlineStart < m_fileInfo.minInline || inlineEnd > m_fileInfo.maxInline ||
        crosslineStart < m_fileInfo.minCrossline || crosslineEnd > m_fileInfo.maxCrossline ||
        inlineStart > inlineEnd || crosslineStart > crosslineEnd) {
        m_lastError = "Invalid region bounds";
        return false;
    }
    
    try {
        int regionInlines = inlineEnd - inlineStart + 1;
        int regionCrosslines = crosslineEnd - crosslineStart + 1;
        size_t totalSamples = size_t(regionInlines) * regionCrosslines * m_fileInfo.sampleCount;

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
                std::memcpy(&volumeData[idx], traceData.data(), m_fileInfo.sampleCount * sizeof(float));
                idx += m_fileInfo.sampleCount;
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

// Read multiple traces by primary index
bool SEGYReader::readTraceByPriIdx(int priIndex, int sndStart, int sndEnd,
                    int dataStart, int dataEnd, void* data)
{
    if (!m_initialized) {
        m_lastError = "SEGY reader not initialized";
        return false;
    }

    // Validate region bounds
    if (priIndex < m_fileInfo.minInline || priIndex > m_fileInfo.maxInline ||
        sndStart < m_fileInfo.minCrossline || sndEnd > m_fileInfo.maxCrossline ||
        sndStart > sndEnd || dataStart > dataEnd ||
        dataStart < 0 || dataEnd >= m_fileInfo.sampleCount) {
        m_lastError = "Invalid region bounds";
        return false;
    }

    if(!data) {
        m_lastError = "Invalid data point";
        return false;        
    }

    std::ifstream file(m_filename, std::ios::binary);
    if (!file) {
        m_lastError = "Error: Cannot open file for trace: " + m_filename ;
        return false;
    }

    try {
        char *volumeData = static_cast<char*>(data);
        
        std::cout << "Reading trace at primary index: " << priIndex 
                  << ", Crossline: " << sndStart << "-" << sndEnd << std::endl;
        
        // Read traces in the region
        std::vector<char> traceData;
        // Resize trace data vector
        int sampleCodeSize = getSampleCodeSize();
        traceData.resize(m_fileInfo.sampleCount*sampleCodeSize);
        int step = m_fileInfo.secondaryStep;
        int trace_length = (dataEnd - dataStart + 1) * sampleCodeSize;
        int idx = 0;
        int start = (sndStart - m_fileInfo.minCrossline) / step * step + m_fileInfo.minCrossline;
        int end = (sndEnd - m_fileInfo.minCrossline) / step * step + m_fileInfo.minCrossline;


        for (int xl = start; xl <= end; xl+=step) {
            //get trace number

            int64_t traceNum = getTraceNumber(file, priIndex, xl);

            if (traceNum < 0) {
                //skip
                std::cerr << "Warning: Trace number not found for Primary index: " << priIndex << ", Crossline: " << xl << std::endl;
                std::memset(&volumeData[idx], 0, trace_length);
                idx +=trace_length; 
                continue;               
            }

            // Read trace data
            if (!readTrace(file, traceNum, traceData.data())) {
                file.close();
                return false;
            }
            // Copy trace data to volume array
            std::memcpy(&volumeData[idx], &traceData.data()[dataStart*sampleCodeSize], trace_length);
            idx +=trace_length;
        }

        std::cout << "Successfully read data at primary index: " << priIndex << std::endl;
        file.close();
        return true;
        
    } catch (const std::exception& e) {
        m_lastError = std::string("Error reading trace: ") + e.what();
        file.close();
        return false;
    }

}

// Read multiple attributes by primary index
bool SEGYReader::readAttrByPriIdx(std::string attr, int priIndex, int sndStart, int sndEnd, void* data)
{
    if (!m_initialized) {
        m_lastError = "SEGY reader not initialized";
        return false;
    }

    // Validate region bounds
    if (priIndex < m_fileInfo.minInline || priIndex > m_fileInfo.maxInline ||
        sndStart < m_fileInfo.minCrossline || sndEnd > m_fileInfo.maxCrossline ||
        sndStart > sndEnd) {
        m_lastError = "Invalid region bounds";
        return false;
    }

    if(!data) {
        m_lastError = "Invalid data point";
        return false;        
    }

    if(m_attrFields.find(attr) == m_customFields.end()) {
        m_lastError = "Attribut " + attr + " doesn't exist.";
        return false;         
    }

    SEGY::HeaderField attrField = m_attrFields[attr];    

    std::ifstream file(m_filename, std::ios::binary);
    if (!file) {
        m_lastError = "Error: Cannot open file for trace: " + m_filename ;
        return false;
    }

    try {
        char *attrData = static_cast<char*>(data);
        
        std::cout << "Reading attribute data:" << attr << " Primary index: " << priIndex 
                  << ", Crossline: " << sndStart << "-" << sndEnd << std::endl;
        
        // Resize trace data vector
        int size = attrField.fieldWidth;
        int step = m_fileInfo.secondaryStep;
        int idx = 0;
        int start = (sndStart - m_fileInfo.minCrossline) / step * step + m_fileInfo.minCrossline;
        int end = (sndEnd - m_fileInfo.minCrossline) / step * step + m_fileInfo.minCrossline;

        for (int xl = start; xl <= end; xl+=step) {
            //get trace number
            int64_t traceNum = getTraceNumber(file, priIndex, xl);

            if (traceNum < 0) {
                //skip
                std::cerr << "Warning: Trace number not found for Primary index: " << priIndex << ", Crossline: " << xl << std::endl;
                std::memset(&attrData[idx], 0, size);
                idx += size; 
                continue;    
            }

            // Read trace header
            // Calculate file position for this trace
            // SEGY format: 3200 byte textual header + 400 byte binary header + traces
            int64_t traceStartOffset = 3600 + traceNum * m_fileInfo.traceByteSize;
            
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

            SEGY::readFieldFromHeader(traceHeader, &attrData[idx], attrField, m_fileInfo.headerEndianness);
            idx += size;
        }

        std::cout << "Successfully read attribute at primary index: " << priIndex << std::endl;
        file.close();
        return true;
        
    } catch (const std::exception& e) {
        m_lastError = std::string("Error reading attribute: ") + e.what();
        file.close();
        return false;
    }
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

bool SEGYReader::printTextualHeader(std::string filename) {
    try {
        std::cout << "Reading SEGY Textual Header (3200 bytes)..." << std::endl;
        m_logger->LogInfo(m_log_data, "Reading SEGY Textual Header (3200 bytes) from file: {}" , filename);
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            std::cout << "Error: Cannot open SEGY file: " << filename << std::endl;
            return false;
        }
        
        // Read the 3200-byte textual header
        char textualHeader[SEGY::TextualFileHeaderSize];
        file.read(textualHeader, SEGY::TextualFileHeaderSize);
        
        if (file.gcount() != SEGY::TextualFileHeaderSize) {
            std::cout << "Error: Failed to read complete textual header. Read " 
                      << file.gcount() << " bytes, expected " << SEGY::TextualFileHeaderSize << std::endl;
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


bool SEGYReader::getPrimaryKeyAxis(int& min_val, int& max_val, int& num_vals, int& step)
{
    if (!m_initialized) {
        m_lastError = "SEGY reader not initialized";
        return false;
    }

    min_val = m_fileInfo.minInline;
    max_val = m_fileInfo.maxInline;
    num_vals = m_fileInfo.inlineCount;
    step = m_fileInfo.primaryStep;
    return true;
}

bool SEGYReader::getSecondaryKeyAxis(int& min_val, int& max_val, int& num_vals, int& step)
{
    if (!m_initialized) {
        m_lastError = "SEGY reader not initialized";
        return false;
    }
    min_val = m_fileInfo.minCrossline;
    max_val = m_fileInfo.maxCrossline;
    num_vals = m_fileInfo.crosslineCount;
    step = m_fileInfo.secondaryStep;

    return true;    
}


bool SEGYReader::getDataAxis(float& min_val, float& max_val, int& num_vals, int& sinterval) 
{
    if (!m_initialized) {
        m_lastError = "SEGY reader not initialized";
        return false;
    }

    min_val = 0;

    max_val = m_fileInfo.sampleCount * m_fileInfo.sampleInterval / 1000.0f; // Convert microseconds to ms

    num_vals = m_fileInfo.sampleCount;

    sinterval = m_fileInfo.sampleInterval;

    return true;
}

