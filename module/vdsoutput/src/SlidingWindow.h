#pragma once

#include <vector>
#include <cstring>
#include <algorithm>
#include <iostream>

/**
 * Universal sliding window for managing memory-efficient data processing
 * Template-based data loader allows external control of data reading logic
 */
class SlidingWindow {
private:
    // === Core data members ===
    std::vector<char> m_buffer;          // Raw byte buffer
    
    // === Window parameters ===
    int m_windowCapacity;                // Window capacity (total inline count, usually 2×brickSize)
    int m_brickSize;                     // Number of inlines processed per batch
    size_t m_inlineSize;                 // Single inline byte size (elementSize × elementNum)
    
    // === Window state ===
    int m_windowStartIdx;                // Window start inline index (global)
    int m_windowEndIdx;                  // Window end inline index (global) 
    int m_validInlineCount;              // Valid inline count in window
    
    // === Configuration parameters ===
    size_t m_elementSize;                // Single element byte size
    int m_elementNum;                    // Element count per inline
    
public:
    // === Constructor ===
    SlidingWindow(int brickSize, size_t elementSize, int elementNum)
        : m_brickSize(brickSize)
        , m_elementSize(elementSize)
        , m_elementNum(elementNum)
        , m_windowCapacity(2 * brickSize)
        , m_inlineSize(elementSize * elementNum)
        , m_windowStartIdx(0)
        , m_windowEndIdx(0)
        , m_validInlineCount(0) {
        
        // Allocate buffer memory
        size_t totalBufferSize = m_windowCapacity * m_inlineSize;
        m_buffer.resize(totalBufferSize);
    }
    
    // === Destructor ===
    ~SlidingWindow() = default;
    
    // Disable copy constructor and assignment
    SlidingWindow(const SlidingWindow&) = delete;
    SlidingWindow& operator=(const SlidingWindow&) = delete;
    
    // === Status query interface ===
    
    // Get basic window information
    int getBrickSize() const { return m_brickSize; }
    int getWindowCapacity() const { return m_windowCapacity; }
    size_t getInlineSize() const { return m_inlineSize; }
    
    // Get current window state
    int getWindowStartIdx() const { return m_windowStartIdx; }
    int getWindowEndIdx() const { return m_windowEndIdx; }
    int getValidInlineCount() const { return m_validInlineCount; }
    
    // Check window status
    bool isEmpty() const { return m_validInlineCount == 0; }
    bool isFull() const { return m_validInlineCount >= m_windowCapacity; }
    
    // Check if specified inline is in window
    bool containsInline(int globalInlineIdx) const {
        return globalInlineIdx >= m_windowStartIdx && globalInlineIdx < m_windowEndIdx;
    }
    
    // === Data access interface ===
    
    // Get raw data pointer for specified inline in window
    char* getInlineData(int globalInlineIdx) {
        if (!containsInline(globalInlineIdx)) {
            return nullptr;
        }
        int localOffset = globalInlineIdx - m_windowStartIdx;
        return m_buffer.data() + localOffset * m_inlineSize;
    }
    
    const char* getInlineData(int globalInlineIdx) const {
        if (!containsInline(globalInlineIdx)) {
            return nullptr;
        }
        int localOffset = globalInlineIdx - m_windowStartIdx;
        return m_buffer.data() + localOffset * m_inlineSize;
    }
    
    // Get data pointer at specified position in window (relative to window start)
    char* getInlineDataByOffset(int offsetInWindow) {
        if (offsetInWindow < 0 || offsetInWindow >= m_validInlineCount) {
            return nullptr;
        }
        return m_buffer.data() + offsetInWindow * m_inlineSize;
    }
    
    // === Window operation interface ===
    
    // Initial fill - external provides data loading function
    template<typename DataLoader>
    bool fillInitial(int startGlobalIdx, int count, DataLoader loader) {
        if (count > m_windowCapacity) {
            return false;
        }
        
        for (int i = 0; i < count; ++i) {
            char* targetPtr = m_buffer.data() + i * m_inlineSize;
            if (!loader(startGlobalIdx + i, targetPtr, m_inlineSize)) {
                return false;
            }
        }
        
        m_windowStartIdx = startGlobalIdx;
        m_windowEndIdx = startGlobalIdx + count;
        m_validInlineCount = count;
        return true;
    }
    
    // Slide window - discard first brickSize, read new brickSize
    template<typename DataLoader>
    bool slide(int totalInlineCount, DataLoader loader) {
        // Step 1: Move existing data
        if (!slideInternal()) {
            return false;
        }
        
        // Step 2: Read new data
        int newStartIdx = m_windowStartIdx + m_brickSize;
        int newCount = std::min(m_brickSize, totalInlineCount - newStartIdx);

        
        if (newCount > 0) {
            for (int i = 0; i < newCount; ++i) {
                char* targetPtr = m_buffer.data() + (m_brickSize + i) * m_inlineSize;
                if (!loader(newStartIdx + i, targetPtr, m_inlineSize)) {
                    return false;
                }
            }
            
            m_validInlineCount += newCount;
            m_windowEndIdx = newStartIdx + newCount;
        }else {
            return false;
        }
        
        return true;
    }
    
    // Extract specified range data to external buffer (legacy method)
    bool extractRange(int startGlobalIdx, int count, char* outputBuffer) const {
        if (!containsInline(startGlobalIdx) || 
            !containsInline(startGlobalIdx + count - 1)) {
            return false;
        }
        
        int localStartOffset = startGlobalIdx - m_windowStartIdx;
        const char* srcPtr = m_buffer.data() + localStartOffset * m_inlineSize;
        size_t copySize = count * m_inlineSize;
        
        std::memcpy(outputBuffer, srcPtr, copySize);
        return true;
    }
    
    // Zero-copy version: get direct pointer to range data
    const char* getRangePointer(int startGlobalIdx, int count, size_t& dataSize) const {
        if (!containsInline(startGlobalIdx) || 
            !containsInline(startGlobalIdx + count - 1)) {
            dataSize = 0;
            return nullptr;
        }
        
        int localStartOffset = startGlobalIdx - m_windowStartIdx;
        const char* srcPtr = m_buffer.data() + localStartOffset * m_inlineSize;
        dataSize = count * m_inlineSize;
        
        return srcPtr;
    }
    
    // Validate window data integrity
    bool validateWindow() const {
        // Check window state consistency
        if (m_windowEndIdx < m_windowStartIdx) {
            return false;
        }
        
        if (m_validInlineCount != (m_windowEndIdx - m_windowStartIdx)) {
            return false;
        }
        
        if (m_validInlineCount > m_windowCapacity) {
            return false;
        }
        
        return true;
    }
    
private:
    // Internal slide operation: move second half data to first half
    bool slideInternal() {
        if (m_validInlineCount < m_brickSize) {
            return false;
        }
        
        size_t moveSize = m_brickSize * m_inlineSize;
        char* srcPtr = m_buffer.data() + moveSize;
        
        std::memmove(m_buffer.data(), srcPtr, moveSize);
        
        // Update window state
        m_windowStartIdx += m_brickSize;
        m_validInlineCount = m_brickSize;
        
        return true;
    }
    
    // Calculate memory statistics
    size_t getTotalMemoryUsage() const {
        return m_buffer.size();
    }
    
    size_t getUsedMemorySize() const {
        return m_validInlineCount * m_inlineSize;
    }
    
    // Validate memory access safety
    bool validateMemoryAccess(int globalInlineIdx, size_t accessSize) const {
        if (!containsInline(globalInlineIdx)) {
            return false;
        }
        
        int localOffset = globalInlineIdx - m_windowStartIdx;
        size_t bufferOffset = localOffset * m_inlineSize;
        
        return (bufferOffset + accessSize) <= m_buffer.size();
    }
};