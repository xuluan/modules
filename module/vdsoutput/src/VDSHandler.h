#ifndef VDS_HANDLER_H
#define VDS_HANDLER_H

#include <OpenVDS/OpenVDS.h>
#include <OpenVDS/VolumeDataLayoutDescriptor.h>
#include <OpenVDS/VolumeDataAxisDescriptor.h>
#include <OpenVDS/VolumeDataChannelDescriptor.h>
#include <OpenVDS/MetadataContainer.h>
#include <string>
#include <vector>
#include <memory>
#include "GdLogger.h"

// SEGY sample units enum
enum class SampleUnits {
    Milliseconds,
    Meters,
    Feet
};

// Value range structure
struct ValueRange {
    float min;
    float max;
    
    ValueRange(float min_val = 0.0f, float max_val = 0.0f) : min(min_val), max(max_val) {}
};

// Attribute field information for VDS channel creation
struct VDSAttributeField {
    std::string name;
    OpenVDS::VolumeDataFormat format;
    int width;
    ValueRange valueRange;
};

/**
 * VDSHandler
 * 
 * This class directly uses OpenVDS native API
 * 1. Uses VolumeDataLayoutDescriptor 
 * 2. Uses VolumeDataAxisDescriptor 
 * 3. Uses VolumeDataChannelDescriptor
 * 4. Uses MetadataContainer
 * 5. One-time call to OpenVDS::Create
 */
class VDSHandler {
public:
    VDSHandler();
    ~VDSHandler();

    // Set basic parameters
    bool SetBasicParameters(const std::string& outputUrl,
                           const std::string& connectionString,
                           int brickSize = 64,
                           int lodLevels = 2,
                           OpenVDS::CompressionMethod compression = OpenVDS::CompressionMethod::Wavelet,
                           float compressionTolerance = 0.01f);

    // Set dimension information
    bool SetDimensions(int sampleCount, float timeMin, float timeMax,
                      int crosslineCount, int crosslineMin, int crosslineMax,
                      int inlineCount, int inlineMin, int inlineMax,
                      SampleUnits sampleUnits = SampleUnits::Milliseconds);

    // Set primary data channel
    bool SetPrimaryChannel(OpenVDS::VolumeDataFormat format,
                          const std::string& attributeName = "Amplitude",
                          const std::string& attributeUnit = "",
                          const ValueRange& valueRange = ValueRange(-1000.0f, 1000.0f));

    // Add attribute channel
    bool AddAttributeChannel(const VDSAttributeField& attrField);

    // Create VDS file
    bool CreateVDS();

    // Get created VDS handle
    OpenVDS::VDSHandle GetVDSHandle() const { return m_vdsHandle; }

    // Get error information
    const std::string& GetLastError() const { return m_lastError; }

    // Close VDS
    void Close();

private:
    // Create LayoutDescriptor
    OpenVDS::VolumeDataLayoutDescriptor CreateLayoutDescriptor();

    // Create AxisDescriptors
    std::vector<OpenVDS::VolumeDataAxisDescriptor> CreateAxisDescriptors();

    // Create ChannelDescriptors
    std::vector<OpenVDS::VolumeDataChannelDescriptor> CreateChannelDescriptors();

    // Create MetadataContainer
    OpenVDS::MetadataContainer CreateMetadataContainer();

    // Get integer offset for SEGY format
    float GetVDSIntegerOffsetForDataSampleFormat(OpenVDS::VolumeDataFormat format);

    // Get unit string
    const char* GetUnitString(SampleUnits units);

private:
    // Basic parameters
    std::string m_outputUrl;
    std::string m_connectionString;
    int m_brickSize;
    int m_lodLevels;
    OpenVDS::CompressionMethod m_compressionMethod;
    float m_compressionTolerance;

    // Dimension information
    int m_sampleCount;
    float m_timeMin, m_timeMax;
    int m_crosslineCount, m_crosslineMin, m_crosslineMax;
    int m_inlineCount, m_inlineMin, m_inlineMax;
    SampleUnits m_sampleUnits;

    // Channel information
    OpenVDS::VolumeDataFormat m_primaryFormat;
    std::string m_primaryAttributeName;
    std::string m_primaryAttributeUnit;
    ValueRange m_primaryValueRange;
    std::vector<VDSAttributeField> m_attributeFields;

    // OpenVDS object
    OpenVDS::VDSHandle m_vdsHandle;
    
    // Logger members
    gdlog::GdLogger* m_logger;
    void* m_log_data;
    
    // Error information
    std::string m_lastError;
    bool m_initialized;
    bool m_created;
};

#endif // VDS_HANDLER_H