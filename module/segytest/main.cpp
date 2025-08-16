#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include "src/segy_reader.h"

void printSEGYInfo(const SEGYReader& reader) {
    const SEGYVolumeInfo& info = reader.getVolumeInfo();
    
    std::cout << "\n=== SEGY File Information ===" << std::endl;
    std::cout << "Volume Dimensions:" << std::endl;
    std::cout << "  Inline Count: " << info.inlineCount << std::endl;
    std::cout << "  Crossline Count: " << info.crosslineCount << std::endl;
    std::cout << "  Sample Count: " << info.sampleCount << std::endl;
    std::cout << "  Total Traces: " << info.totalTraces << std::endl;
    
    std::cout << "\nCoordinate Ranges:" << std::endl;
    std::cout << "  Inlines: " << info.inlineStart << " - " << info.inlineEnd << std::endl;
    std::cout << "  Crosslines: " << info.crosslineStart << " - " << info.crosslineEnd << std::endl;
    std::cout << "  X Range: " << std::fixed << std::setprecision(2) << info.xMin << " - " << info.xMax << std::endl;
    std::cout << "  Y Range: " << std::fixed << std::setprecision(2) << info.yMin << " - " << info.yMax << std::endl;
    
    std::cout << "\nTemporal Information:" << std::endl;
    std::cout << "  Sample Interval: " << info.sampleInterval << " microseconds" << std::endl;
    std::cout << "  Start Time: " << info.startTime << " ms" << std::endl;
    
    std::cout << "\nData Format:" << std::endl;
    std::cout << "  Data Format: ";
    switch (info.dataFormat) {
        case SEGY::BinaryHeader::DataSampleFormatCode::IEEEFloat:
            std::cout << "IEEE Float (32-bit)";
            break;
        case SEGY::BinaryHeader::DataSampleFormatCode::Int32:
            std::cout << "32-bit Integer";
            break;
        case SEGY::BinaryHeader::DataSampleFormatCode::Int16:
            std::cout << "16-bit Integer";
            break;
        default:
            std::cout << "Unknown/Other";
            break;
    }
    std::cout << std::endl;
    
    std::cout << "  Endianness: ";
    if (info.endianness == SEGY::Endianness::BigEndian) {
        std::cout << "Big Endian";
    } else {
        std::cout << "Little Endian";
    }
    std::cout << std::endl;
    
    std::cout << "===============================\n" << std::endl;
}

void printTraceData(const std::vector<float>& traceData, int inline_num, int crossline_num, int maxSamples = 10) {
    std::cout << "\n--- Trace Data (IL:" << inline_num << ", XL:" << crossline_num << ") ---" << std::endl;
    std::cout << "Total samples: " << traceData.size() << std::endl;
    
    int samplesToShow = std::min(maxSamples, (int)traceData.size());
    std::cout << "First " << samplesToShow << " samples:" << std::endl;
    
    for (int i = 0; i < samplesToShow; i++) {
        std::cout << "  Sample " << std::setw(3) << i << ": " 
                  << std::fixed << std::setprecision(6) << traceData[i] << std::endl;
    }
    
    if (traceData.size() > maxSamples) {
        std::cout << "  ... (showing only first " << maxSamples << " of " << traceData.size() << " samples)" << std::endl;
    }
    
    // Show some basic statistics
    if (!traceData.empty()) {
        float minVal = *std::min_element(traceData.begin(), traceData.end());
        float maxVal = *std::max_element(traceData.begin(), traceData.end());
        
        double sum = 0.0;
        for (float val : traceData) {
            sum += val;
        }
        double mean = sum / traceData.size();
        
        std::cout << "\nTrace Statistics:" << std::endl;
        std::cout << "  Min: " << std::fixed << std::setprecision(6) << minVal << std::endl;
        std::cout << "  Max: " << std::fixed << std::setprecision(6) << maxVal << std::endl;
        std::cout << "  Mean: " << std::fixed << std::setprecision(6) << mean << std::endl;
    }
    
    std::cout << "------------------------------------------------\n" << std::endl;
}

int main() {
    std::cout << "SEGY File Reader Test Program" << std::endl;
    std::cout << "=============================" << std::endl;
    
    // 创建SEGY读取器实例
    SEGYReader reader;
    
    // SEGY文件路径
    std::string segyFilePath = "Demo.segy";
    
    std::cout << "Initializing SEGY reader for file: " << segyFilePath << std::endl;
    
    // 初始化读取器
    if (!reader.initialize(segyFilePath)) {
        std::cerr << "Failed to initialize SEGY reader: " << reader.getLastError() << std::endl;
        return 1;
    }
    
    reader.printTextualHeader();

    // 打印SEGY文件信息
    printSEGYInfo(reader);
    
    
    // 获取体积信息
    const SEGYVolumeInfo& volumeInfo = reader.getVolumeInfo();
    
    // 读取并显示第一个trace
    if (volumeInfo.inlineCount > 0 && volumeInfo.crosslineCount > 0) {
        int firstInline = volumeInfo.inlineStart;
        int firstCrossline = volumeInfo.crosslineStart;
        
        std::vector<float> traceData;
        std::cout << "Reading first trace (IL:" << firstInline << ", XL:" << firstCrossline << ")..." << std::endl;
        
        if (reader.readTrace(firstInline, firstCrossline, traceData)) {
            printTraceData(traceData, firstInline, firstCrossline);
        } else {
            std::cerr << "Failed to read first trace: " << reader.getLastError() << std::endl;
        }
        
        // 如果有足够的traces，读取中间的一个trace
        if (volumeInfo.inlineCount > 1 || volumeInfo.crosslineCount > 1) {
            int midInline = volumeInfo.inlineStart + volumeInfo.inlineCount / 2;
            int midCrossline = volumeInfo.crosslineStart + volumeInfo.crosslineCount / 2;
            
            std::cout << "Reading middle trace (IL:" << midInline << ", XL:" << midCrossline << ")..." << std::endl;
            
            if (reader.readTrace(midInline, midCrossline, traceData)) {
                printTraceData(traceData, midInline, midCrossline, 5); // 只显示5个样本
            } else {
                std::cerr << "Failed to read middle trace: " << reader.getLastError() << std::endl;
            }
        }
        
        // 测试读取小区域数据
        if (volumeInfo.inlineCount >= 2 && volumeInfo.crosslineCount >= 2) {
            std::cout << "Reading small region data..." << std::endl;
            
            int regionInlineStart = volumeInfo.inlineStart;
            int regionInlineEnd = std::min(volumeInfo.inlineStart + 10, volumeInfo.inlineEnd);
            int regionCrosslineStart = volumeInfo.crosslineStart;
            int regionCrosslineEnd = std::min(volumeInfo.crosslineStart + 10, volumeInfo.crosslineEnd);
            
            std::vector<float> regionData;
            if (reader.readTraceRegion(regionInlineStart, regionInlineEnd,
                                     regionCrosslineStart, regionCrosslineEnd,
                                     regionData)) {
                std::cout << "Successfully read region data:" << std::endl;
                std::cout << "  Region: IL" << regionInlineStart << "-" << regionInlineEnd 
                          << ", XL" << regionCrosslineStart << "-" << regionCrosslineEnd << std::endl;
                std::cout << "  Total samples in region: " << regionData.size() << std::endl;
                std::cout << "  Expected samples: " 
                          << (regionInlineEnd - regionInlineStart + 1) * 
                             (regionCrosslineEnd - regionCrosslineStart + 1) * 
                             volumeInfo.sampleCount << std::endl;
            } else {
                std::cerr << "Failed to read region data: " << reader.getLastError() << std::endl;
            }
        }
    } else {
        std::cerr << "No valid traces found in SEGY file" << std::endl;
        return 1;
    }
    
    std::cout << "\nSEGY file analysis completed successfully!" << std::endl;
    return 0;
}