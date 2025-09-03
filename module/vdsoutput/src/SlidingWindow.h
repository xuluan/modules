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
    
    // Check if specified inline is in window
    bool containsInline(int globalInlineIdx) const {
        return globalInlineIdx >= m_windowStartIdx && globalInlineIdx < m_windowEndIdx;
    }

    // === Public interface methods ===

    // Slide operation: move second half data to first half
    bool slide() {
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

    //fill one inline
    bool fill(char *data) {
        //if space enough
        if (m_validInlineCount >= m_brickSize*2) {
            return false;
        }
        int offset = m_inlineSize * m_validInlineCount;
        std::memcpy(m_buffer.data() + offset, data, m_inlineSize);
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

};