#include "segy_writer.h"
#include <iostream>
#include <algorithm>
#include <sstream>

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

SEGYWriter::SEGYWriter() 
    : m_initialized(false), m_fileCreated(false), m_log_data(nullptr) {
    
    m_logger = &gdlog::GdLogger::GetInstance();
    m_log_data = m_logger->Init("SEGYWriter");
}

SEGYWriter::~SEGYWriter() {
}

void SEGYWriter::addBinaryField(const std::string& name, int byteLocation, int width, SEGY::DataSampleFormatCode format) {
    SEGY::HeaderField field;
    field.byteLocation = byteLocation;
    field.fieldWidth = width;
    field.fieldType = format;
    m_binaryFields[name] = field;
}

void SEGYWriter::addTraceField(const std::string& name, int byteLocation, int width, SEGY::DataSampleFormatCode format) {
    SEGY::HeaderField field;
    field.byteLocation = byteLocation;
    field.fieldWidth = width;
    field.fieldType = format;
    m_traceFields[name] = field;
}

bool SEGYWriter::initialize(const std::string& filename, SEGYWriteInfo writeInfo) {
    m_filename = filename;
    m_writeInfo = writeInfo;

    try {
        // Open output file in binary mode
        std::ofstream file(filename, std::ios::binary | std::ios::out | std::ios::trunc);
        if (!file.is_open()) {
            m_lastError = "Cannot open file for writing: " + filename;
            return false;
        }
        
        // Write SEGY headers
        if (!writeTextualHeader(file)) {
            m_lastError = "Failed to write textual header";
            return false;
        }
        
        if (!writeBinaryHeader(file)) {
            m_lastError = "Failed to write binary header";
            return false;
        }

        int traceCount = m_writeInfo.inlineCount * m_writeInfo.crosslineCount;
        int traceSize = 240 + m_writeInfo.traceByteSize;
        for(int i = 0; i < traceCount; ++i) {
            // Write empty trace headers and data
            std::vector<char> emptyTraceHeader(traceSize, 0);
            file.write(emptyTraceHeader.data(), traceSize);
            if (file.fail()) {
                m_lastError = "Failed to write empty trace at index: " + std::to_string(i);
                return false;
            }            
        }

        m_initialized = true;
        m_fileCreated = true;
        
        m_logger->LogInfo(m_log_data, "SEGYWriter initialized for file: {}", filename);
        file.close();
        return true;
        
    } catch (const std::exception& e) {
        m_lastError = "Exception during initialization: " + std::string(e.what());
        return false;
    }
}

bool SEGYWriter::finalize() {
    m_initialized = false;
    m_logger->LogInfo(m_log_data, "SEGYWriter finalized for file: {}", m_filename);
    return true;
}

bool SEGYWriter::writeTraceData(std::ofstream& file, int inlineNum, int crosslineNum, char* sampleData) {
    if (!m_initialized) {
        m_lastError = "Writer not initialized";
        return false;
    }
    
    try {
        // Calculate trace position in file
        int64_t filePosition = calculateFilePosition(inlineNum, crosslineNum);
        if (filePosition < 0) {
            m_lastError = "Invalid trace coordinates: " + std::to_string(inlineNum) + ", " + std::to_string(crosslineNum);
            return false;
        }

        int64_t traceHeaderSize = 240;
        filePosition += traceHeaderSize;

        // Seek to position and write sample data
        file.seekp(filePosition, std::ios::beg);
        convertSampleDataForWriting(sampleData);
        file.write(static_cast<const char*>(sampleData), m_writeInfo.traceByteSize);
        
        if (file.fail()) {
            m_lastError = "Failed to write trace data at position";
            return false;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        m_lastError = "Exception during trace data writing: " + std::string(e.what());
        return false;
    }
}

bool SEGYWriter::writeTraceHeader(std::ofstream& file, int inlineNum, int crosslineNum, char* data, int offset, int len) {
    if (!m_initialized) {
        m_lastError = "Writer not initialized";
        return false;
    }
    
    try {
        // Calculate trace position in file
        int64_t filePosition = calculateFilePosition(inlineNum, crosslineNum);
        if (filePosition < 0) {
            m_lastError = "Invalid trace coordinates: " + std::to_string(inlineNum) + ", " + std::to_string(crosslineNum);
            return false;
        }

        if (m_writeInfo.headerEndianness == SEGY::Endianness::BigEndian) {
            if (len == 4) {
                uint32_t* value = reinterpret_cast<uint32_t*>(data);
                *value = ((*value & 0xFF) << 24) | ((*value & 0xFF00) << 8) | 
                        ((*value & 0xFF0000) >> 8) | ((*value & 0xFF000000) >> 24);
            } else if (len == 2) {
                uint16_t* value = reinterpret_cast<uint16_t*>(data);
                *value = ((*value & 0xFF) << 8) | ((*value & 0xFF00) >> 8);
            }        
        }
        
        // Seek to position and write trace header
        file.seekp(filePosition + offset - 1, std::ios::beg);
        file.write(data, len);
        
        if (file.fail()) {
            m_lastError = "Failed to write trace header at position";
            return false;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        m_lastError = "Exception during trace header writing: " + std::string(e.what());
        return false;
    }
}

// Private helper functions
void SEGYWriter::generateTextualHeader(std::vector<char>& textualHeader) {
    textualHeader.resize(3200, ' ');
    
    // Fill header with spaces (EBCDIC space = 0x40)
    for (size_t i = 0; i < 3200; ++i) {
        textualHeader[i] = asciiToEbcdic(' ');
    }
    
    // Create properly formatted SEGY textual header lines (80 characters each)
    std::vector<std::string> headerLines;
    
    if (!m_writeInfo.textualHeaderContent.empty()) {
        // Parse custom textual header content, ensuring 80-char lines
        std::istringstream iss(m_writeInfo.textualHeaderContent);
        std::string line;
        int lineNum = 1;
        
        while (std::getline(iss, line) && lineNum <= 40) {
            // Ensure line starts with card identifier if not present
            if (line.length() < 3 || line.substr(0, 1) != "C") {
                line = "C" + std::string(2 - std::to_string(lineNum).length(), '0') + std::to_string(lineNum) + " " + line;
            }
            
            // Pad or truncate to exactly 80 characters
            if (line.length() > 80) {
                line = line.substr(0, 80);
            } else {
                line.resize(80, ' ');
            }
            
            headerLines.push_back(line);
            lineNum++;
        }
    } else {
        // Generate default header content with proper formatting
        headerLines.push_back("C01 SEGY file generated by SEGYWriter                                           ");
        
        std::string inlineInfo = "C02 Inline range: " + std::to_string(m_writeInfo.minInline) + " - " + std::to_string(m_writeInfo.maxInline);
        inlineInfo.resize(80, ' ');
        headerLines.push_back(inlineInfo);
        
        std::string crosslineInfo = "C03 Crossline range: " + std::to_string(m_writeInfo.minCrossline) + " - " + std::to_string(m_writeInfo.maxCrossline);
        crosslineInfo.resize(80, ' ');
        headerLines.push_back(crosslineInfo);
        
        std::string sampleInfo = "C04 Sample count: " + std::to_string(m_writeInfo.sampleCount);
        sampleInfo.resize(80, ' ');
        headerLines.push_back(sampleInfo);
        
        std::string intervalInfo = "C05 Sample interval: " + std::to_string(m_writeInfo.sampleInterval) + " microseconds";
        intervalInfo.resize(80, ' ');
        headerLines.push_back(intervalInfo);
        
        headerLines.push_back("C06                                                                             ");
        headerLines.push_back("C07 Data format: IEEE 32-bit floating point                                    ");
        headerLines.push_back("C08 Coordinate system: Inline/Crossline                                        ");
        headerLines.push_back("C09                                                                             ");
        headerLines.push_back("C10                                                                             ");
        headerLines.push_back("C11                                                                             ");
        headerLines.push_back("C12                                                                             ");
        headerLines.push_back("C13                                                                             ");
        headerLines.push_back("C14                                                                             ");
        headerLines.push_back("C15                                                                             ");
        headerLines.push_back("C16                                                                             ");
        headerLines.push_back("C17                                                                             ");
        headerLines.push_back("C18                                                                             ");
        headerLines.push_back("C19                                                                             ");
        headerLines.push_back("C20                                                                             ");
        headerLines.push_back("C21                                                                             ");
        headerLines.push_back("C22                                                                             ");
        headerLines.push_back("C23                                                                             ");
        headerLines.push_back("C24                                                                             ");
        headerLines.push_back("C25                                                                             ");
        headerLines.push_back("C26                                                                             ");
        headerLines.push_back("C27                                                                             ");
        headerLines.push_back("C28                                                                             ");
        headerLines.push_back("C29                                                                             ");
        headerLines.push_back("C30                                                                             ");
        headerLines.push_back("C31                                                                             ");
        headerLines.push_back("C32                                                                             ");
        headerLines.push_back("C33                                                                             ");
        headerLines.push_back("C34                                                                             ");
        headerLines.push_back("C35                                                                             ");
        headerLines.push_back("C36                                                                             ");
        headerLines.push_back("C37                                                                             ");
        headerLines.push_back("C38                                                                             ");
        headerLines.push_back("C39                                                                             ");
        headerLines.push_back("C40 END EBCDIC                                                                  ");
    }
    
    // Fill remaining lines if less than 40 lines provided
    while (headerLines.size() < 40) {
        std::string emptyLine = "C" + std::string(2 - std::to_string(headerLines.size() + 1).length(), '0') + 
                               std::to_string(headerLines.size() + 1) + std::string(76, ' ');
        headerLines.push_back(emptyLine);
    }
    
    // Convert all lines to EBCDIC and copy to header buffer
    for (size_t lineIdx = 0; lineIdx < std::min(headerLines.size(), size_t(40)); ++lineIdx) {
        const std::string& line = headerLines[lineIdx];
        for (size_t charIdx = 0; charIdx < 80; ++charIdx) {
            char asciiChar = (charIdx < line.length()) ? line[charIdx] : ' ';
            textualHeader[lineIdx * 80 + charIdx] = asciiToEbcdic(static_cast<unsigned char>(asciiChar));
        }
    }
}

void SEGYWriter::generateBinaryHeader(std::vector<char>& binaryHeader) {

    for(auto binary_field = m_binaryFields.begin(); binary_field != m_binaryFields.end(); ++binary_field) {
        const std::string& name = binary_field->first;
        const SEGY::HeaderField& field = binary_field->second;

        if (name == "DataFormatCode") {
            int16_t value = static_cast<int16_t>(m_writeInfo.dataSampleFormatCode);
            writeFieldToHeader(binaryHeader.data(), &value, field);
        } else if (name == "SampleInterval") {
            int16_t value = static_cast<int16_t>(m_writeInfo.sampleInterval);
            writeFieldToHeader(binaryHeader.data(), &value, field);
        } else if (name == "NumSamples") {
            int16_t value = static_cast<int16_t>(m_writeInfo.sampleCount);
            writeFieldToHeader(binaryHeader.data(), &value, field);
        } else {

        }
    }

}

bool SEGYWriter::writeTextualHeader(std::ofstream& file) {
    std::vector<char> textualHeader;
    generateTextualHeader(textualHeader);

    file.write(textualHeader.data(), 3200);
    return !file.fail();
}

bool SEGYWriter::writeBinaryHeader(std::ofstream& file) {
    std::vector<char> binaryHeader(400, 0);
    generateBinaryHeader(binaryHeader);
    
    file.write(binaryHeader.data(), 400);
    return !file.fail();
}

char SEGYWriter::asciiToEbcdic(unsigned char asciiChar) {
    // Simplified ASCII to EBCDIC conversion table
    static const unsigned char ebcdicTable[256] = {
        0x00, 0x01, 0x02, 0x03, 0x37, 0x2D, 0x2E, 0x2F, 0x16, 0x05, 0x15, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
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

void SEGYWriter::writeFieldToHeader(void* header, const void* data, const SEGY::HeaderField& headerField) {
    char* headerPtr = static_cast<char*>(header);
    const char* dataPtr = static_cast<const char*>(data);
    SEGY::Endianness endianness = m_writeInfo.headerEndianness;

    // Calculate offset (SEGY headers are 1-based, so subtract 1)
    int offset = headerField.byteLocation - 1;
    
    if (headerField.fieldWidth == 2) {
        // 16-bit field
        short value = *static_cast<const short*>(data);
        if (endianness == SEGY::Endianness::BigEndian) {
            value = ((value & 0xFF) << 8) | ((value & 0xFF00) >> 8);
        }
        std::memcpy(headerPtr + offset, &value, 2);
    } else if (headerField.fieldWidth == 4) {
        // 32-bit field
        int value = *static_cast<const int*>(data);
        if (endianness == SEGY::Endianness::BigEndian) {
            value = ((value & 0xFF) << 24) | ((value & 0xFF00) << 8) | 
                    ((value & 0xFF0000) >> 8) | ((value & 0xFF000000) >> 24);
        }
        std::memcpy(headerPtr + offset, &value, 4);
    } else {
        // Variable width field - copy as is
        std::memcpy(headerPtr + offset, dataPtr, headerField.fieldWidth);
    }
}

void SEGYWriter::convertSampleDataForWriting(char* data) {
    // Convert sample data based on format and endianness
    if (m_writeInfo.headerEndianness == SEGY::Endianness::BigEndian) {
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

        for (size_t i = 0; i < m_writeInfo.sampleCount; ++i) {
            if (sampleSize == 4) {
                uint32_t* value = reinterpret_cast<uint32_t*>(&data[i*sampleSize]);
                *value = ((*value & 0xFF) << 24) | ((*value & 0xFF00) << 8) | 
                        ((*value & 0xFF0000) >> 8) | ((*value & 0xFF000000) >> 24);
            } else if (sampleSize == 2) {
                uint16_t* value = reinterpret_cast<uint16_t*>(&data[i*sampleSize]);
                *value = ((*value & 0xFF) << 8) | ((*value & 0xFF00) >> 8);
            }
        }
    }
}

int64_t SEGYWriter::calculateFilePosition(int inlineNum, int crosslineNum) {
    int inline_idx = (inlineNum - m_writeInfo.minInline) / m_writeInfo.primaryStep;
    int crossline_idx = (crosslineNum - m_writeInfo.minCrossline) / m_writeInfo.secondaryStep;
    int64_t count =  inline_idx * m_writeInfo.crosslineCount + crossline_idx;
    int traceSize = 240 + m_writeInfo.traceByteSize;

    return count * traceSize;

}