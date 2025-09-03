#include "VDSHandler.h"
#include <iostream>
#include <cassert>


#define KNOWNMETADATA_UNIT_MILLISECOND "ms"
#define KNOWNMETADATA_UNIT_METER "m"
#define KNOWNMETADATA_UNIT_FOOT "ft"
#define KNOWNMETADATA_UNIT_UNITLESS ""
#define KNOWNMETADATA_SURVEYCOORDINATE_INLINECROSSLINE_AXISNAME_SAMPLE "Sample"
#define KNOWNMETADATA_SURVEYCOORDINATE_INLINECROSSLINE_AXISNAME_CROSSLINE "Crossline"
#define KNOWNMETADATA_SURVEYCOORDINATE_INLINECROSSLINE_AXISNAME_INLINE "Inline"

VDSHandler::VDSHandler()
    : m_brickSize(64)
    , m_lodLevels(2)
    , m_compressionMethod(OpenVDS::CompressionMethod::Wavelet)
    , m_compressionTolerance(0.01f)
    , m_sampleCount(0)
    , m_timeMin(0.0f), m_timeMax(0.0f)
    , m_crosslineCount(0), m_crosslineMin(0), m_crosslineMax(0)
    , m_inlineCount(0), m_inlineMin(0), m_inlineMax(0)
    , m_sampleUnits(SampleUnits::Milliseconds)
    , m_primaryFormat(OpenVDS::VolumeDataFormat::Format_R32)
    , m_primaryAttributeName("Amplitude")
    , m_primaryAttributeUnit("")
    , m_primaryValueRange(-1000.0f, 1000.0f)
    , m_vdsHandle(nullptr)
    , m_initialized(false)
    , m_created(false)
{
    // Initialize logger
    m_logger = &gdlog::GdLogger::GetInstance();
    m_log_data = m_logger->Init("VDSHandler");
}

VDSHandler::~VDSHandler()
{
    Close();
}

bool VDSHandler::SetBasicParameters(const std::string& outputUrl,
                                   const std::string& connectionString,
                                   int brickSize,
                                   int lodLevels,
                                   OpenVDS::CompressionMethod compression,
                                   float compressionTolerance)
{
    m_outputUrl = outputUrl;
    m_connectionString = connectionString;
    m_brickSize = brickSize;
    m_lodLevels = lodLevels;
    m_compressionMethod = compression;
    m_compressionTolerance = compressionTolerance;
    
    // Validate parameters
    if (brickSize != 32 && brickSize != 64 && brickSize != 128 && brickSize != 256) {
        m_lastError = "Illegal brick size (must be 32, 64, 128 or 256)";
        return false;
    }
    
    if (lodLevels < 0 || lodLevels > 12) {
        m_lastError = "Illegal number of LOD levels (max is 12)";
        return false;
    }
    
    m_logger->LogInfo(m_log_data, "VDSHandler basic parameters set:");
    m_logger->LogInfo(m_log_data, "  Output URL: {}", outputUrl);
    m_logger->LogInfo(m_log_data, "  Brick size: {}", brickSize);
    m_logger->LogInfo(m_log_data, "  LOD levels: {}", lodLevels);
    m_logger->LogInfo(m_log_data, "  Compression: {}", static_cast<int>(compression));
    
    return true;
}

bool VDSHandler::SetDimensions(int sampleCount, float timeMin, float timeMax,
                              int crosslineCount, int crosslineMin, int crosslineMax,
                              int inlineCount, int inlineMin, int inlineMax,
                              SampleUnits sampleUnits)
{
    m_sampleCount = sampleCount;
    m_timeMin = timeMin;
    m_timeMax = timeMax;
    m_crosslineCount = crosslineCount;
    m_crosslineMin = crosslineMin;
    m_crosslineMax = crosslineMax;
    m_inlineCount = inlineCount;
    m_inlineMin = inlineMin;
    m_inlineMax = inlineMax;
    m_sampleUnits = sampleUnits;
    
    m_logger->LogInfo(m_log_data, "VDSHandler dimensions set:");
    m_logger->LogInfo(m_log_data, "  Sample: {} ({} to {} {})", sampleCount, timeMin, timeMax, GetUnitString(sampleUnits));
    m_logger->LogInfo(m_log_data, "  Crossline: {} ({} to {})", crosslineCount, crosslineMin, crosslineMax);
    m_logger->LogInfo(m_log_data, "  Inline: {} ({} to {})", inlineCount, inlineMin, inlineMax);

    m_initialized = true;
    return true;
}

bool VDSHandler::SetPrimaryChannel(OpenVDS::VolumeDataFormat format,
                                  const std::string& attributeName,
                                  const std::string& attributeUnit,
                                  const ValueRange& valueRange)
{
    m_primaryFormat = format;
    m_primaryAttributeName = attributeName;
    m_primaryAttributeUnit = attributeUnit;
    m_primaryValueRange = valueRange;
    
    m_logger->LogInfo(m_log_data, "VDSHandler primary channel set:");
    m_logger->LogInfo(m_log_data, "  Format: {}", static_cast<int>(format));
    m_logger->LogInfo(m_log_data, "  Name: {}", attributeName);
    m_logger->LogInfo(m_log_data, "  Unit: {}", (attributeUnit.empty() ? "(no unit)" : attributeUnit));
    m_logger->LogInfo(m_log_data, "  Range: {} to {}", valueRange.min, valueRange.max);

    return true;
}

bool VDSHandler::AddAttributeChannel(const VDSAttributeField& attrField)
{
    m_attributeFields.push_back(attrField);
    
    m_logger->LogInfo(m_log_data, "VDSHandler added attribute channel:");
    m_logger->LogInfo(m_log_data, "  Name: {}", attrField.name);
    m_logger->LogInfo(m_log_data, "  Format: {}", static_cast<int>(attrField.format));
    m_logger->LogInfo(m_log_data, "  Width: {} bytes", attrField.width);
    m_logger->LogInfo(m_log_data, "  Range: {} to {}", attrField.valueRange.min, attrField.valueRange.max);

    return true;
}

bool VDSHandler::CreateVDS()
{
    if (!m_initialized) {
        m_lastError = "VDSHandler not properly initialized";
        return false;
    }
    
    if (m_created) {
        m_lastError = "VDS already created";
        return false;
    }
    
    try {
        m_logger->LogInfo(m_log_data, "Creating VDS");
        
        // 1. Create LayoutDescriptor
        OpenVDS::VolumeDataLayoutDescriptor layoutDescriptor = CreateLayoutDescriptor();
        
        // 2. Create AxisDescriptors
        std::vector<OpenVDS::VolumeDataAxisDescriptor> axisDescriptors = CreateAxisDescriptors();
        
        // 3. Create ChannelDescriptors
        std::vector<OpenVDS::VolumeDataChannelDescriptor> channelDescriptors = CreateChannelDescriptors();
        
        // 4. Create MetadataContainer
        OpenVDS::MetadataContainer metadataContainer = CreateMetadataContainer();
        
        // 5. Create OpenOptions
        OpenVDS::Error createError;
        auto openOptions = OpenVDS::CreateOpenOptions(m_outputUrl, m_connectionString, createError);
        
        if (createError.code != 0) {
            m_lastError = "Failed to create OpenOptions: " + std::string(createError.string);
            return false;
        }
        
        // 6. Create VDS in one step
        m_vdsHandle = OpenVDS::Create(*openOptions, 
                                     layoutDescriptor, 
                                     axisDescriptors, 
                                     channelDescriptors, 
                                     metadataContainer, 
                                     m_compressionMethod, 
                                     m_compressionTolerance, 
                                     createError);
        
        if (createError.code != 0) {
            m_lastError = "Failed to create VDS: " + std::string(createError.string);
            return false;
        }
        
        if (!m_vdsHandle) {
            m_lastError = "VDS handle is null after creation";
            return false;
        }
        
        m_logger->LogInfo(m_log_data, "VDS created successfully");
        m_logger->LogInfo(m_log_data, "  Channels: {}", channelDescriptors.size());
        m_logger->LogInfo(m_log_data, "  Axes: {}", axisDescriptors.size());
        
        m_created = true;
        return true;
        
    } catch (const std::exception& e) {
        m_lastError = "Exception during VDS creation: " + std::string(e.what());
        return false;
    }
}

OpenVDS::VolumeDataLayoutDescriptor VDSHandler::CreateLayoutDescriptor()
{
    // Create LayoutDescriptor
    OpenVDS::VolumeDataLayoutDescriptor::BrickSize brickSizeEnum;
    
    switch (m_brickSize) {
        case 32:  brickSizeEnum = OpenVDS::VolumeDataLayoutDescriptor::BrickSize_32; break;
        case 64:  brickSizeEnum = OpenVDS::VolumeDataLayoutDescriptor::BrickSize_64; break;
        case 128: brickSizeEnum = OpenVDS::VolumeDataLayoutDescriptor::BrickSize_128; break;
        case 256: brickSizeEnum = OpenVDS::VolumeDataLayoutDescriptor::BrickSize_256; break;
        default:  brickSizeEnum = OpenVDS::VolumeDataLayoutDescriptor::BrickSize_64; break;
    }
    
    // Default values
    const int negativeMargin = 4;  // Default margin value
    const int positiveMargin = 4;
    const int brickSize2DMultiplier = 4;  // Fixed to 4
    const bool create2DLODs = false;  // Usually false
    
    OpenVDS::VolumeDataLayoutDescriptor::Options layoutOptions = 
        create2DLODs ? OpenVDS::VolumeDataLayoutDescriptor::Options_Create2DLODs 
                     : OpenVDS::VolumeDataLayoutDescriptor::Options_None;
    
    m_logger->LogInfo(m_log_data, "Creating LayoutDescriptor:");
    m_logger->LogInfo(m_log_data, "  BrickSize: {}", m_brickSize);
    m_logger->LogInfo(m_log_data, "  Margins: {}/{}", negativeMargin, positiveMargin);
    m_logger->LogInfo(m_log_data, "  2D Multiplier: {}", brickSize2DMultiplier);
    m_logger->LogInfo(m_log_data, "  LOD Levels: {}", m_lodLevels);
    
    return OpenVDS::VolumeDataLayoutDescriptor(
        brickSizeEnum,
        negativeMargin,
        positiveMargin,
        brickSize2DMultiplier,
        OpenVDS::VolumeDataLayoutDescriptor::LODLevels(m_lodLevels),
        layoutOptions
    );
}

std::vector<OpenVDS::VolumeDataAxisDescriptor> VDSHandler::CreateAxisDescriptors()
{
    std::vector<OpenVDS::VolumeDataAxisDescriptor> axisDescriptors;
    
    // createAxisDescriptors function
    const char* sampleUnit = GetUnitString(m_sampleUnits);
    
    m_logger->LogInfo(m_log_data, "Creating AxisDescriptors:");
    
    // Axis 0: Sample/Time
    axisDescriptors.emplace_back(
        m_sampleCount,
        KNOWNMETADATA_SURVEYCOORDINATE_INLINECROSSLINE_AXISNAME_SAMPLE,
        sampleUnit,
        m_timeMin,
        m_timeMax
    );
    m_logger->LogInfo(m_log_data, "  Axis 0 (Sample): {} samples, {} to {} {}", m_sampleCount, m_timeMin, m_timeMax, sampleUnit);
    
    // Axis 1: Crossline
    axisDescriptors.emplace_back(
        m_crosslineCount,
        KNOWNMETADATA_SURVEYCOORDINATE_INLINECROSSLINE_AXISNAME_CROSSLINE,
        KNOWNMETADATA_UNIT_UNITLESS,
        static_cast<float>(m_crosslineMin),
        static_cast<float>(m_crosslineMax)
    );
    m_logger->LogInfo(m_log_data, "  Axis 1 (Crossline): {} lines, {} to {}", m_crosslineCount, m_crosslineMin, m_crosslineMax);
    
    // Axis 2: Inline
    axisDescriptors.emplace_back(
        m_inlineCount,
        KNOWNMETADATA_SURVEYCOORDINATE_INLINECROSSLINE_AXISNAME_INLINE,
        KNOWNMETADATA_UNIT_UNITLESS,
        static_cast<float>(m_inlineMin),
        static_cast<float>(m_inlineMax)
    );
    m_logger->LogInfo(m_log_data, "  Axis 2 (Inline): {} lines, {} to {}", m_inlineCount, m_inlineMin, m_inlineMax);
    
    return axisDescriptors;
}

std::vector<OpenVDS::VolumeDataChannelDescriptor> VDSHandler::CreateChannelDescriptors()
{
    std::vector<OpenVDS::VolumeDataChannelDescriptor> channelDescriptors;
    
    m_logger->LogInfo(m_log_data, "Creating ChannelDescriptors:");
    
    // 1. Primary channel
    OpenVDS::VolumeDataFormat format = m_primaryFormat;
    
    // Calculate integerOffset and integerScale
    float integerOffset = -GetVDSIntegerOffsetForDataSampleFormat(m_primaryFormat);  // Note negative sign!
    float integerScale = 1.0f;  // SEGY doesn't support integer scaling by default
    
    // Adjust valueRange
    OpenVDS::FloatRange effectiveValueRange(
        m_primaryValueRange.min + integerOffset,
        m_primaryValueRange.max + integerOffset
    );
    
    channelDescriptors.emplace_back(
        format,
        OpenVDS::VolumeDataComponents::Components_1,
        m_primaryAttributeName.c_str(),
        m_primaryAttributeUnit.c_str(),
        effectiveValueRange.Min,
        effectiveValueRange.Max,
        OpenVDS::VolumeDataMapping::Direct,      // Important: mapping method
        1,                                       // Important: component mapping
        OpenVDS::VolumeDataChannelDescriptor::Default,  // Important: channel flags
        0.0f,                                    // noValue
        integerScale,                            // Important: integer scale
        integerOffset                           // Important: integer offset
    );
    
    m_logger->LogInfo(m_log_data, "  Primary channel: {}", m_primaryAttributeName);
    m_logger->LogInfo(m_log_data, "    Format: {}", static_cast<int>(format));
    m_logger->LogInfo(m_log_data, "    IntegerScale: {}", integerScale);
    m_logger->LogInfo(m_log_data, "    IntegerOffset: {}", integerOffset);
    m_logger->LogInfo(m_log_data, "    Effective range: {} to {}", effectiveValueRange.Min, effectiveValueRange.Max);

    // 2. Trace defined flag
    channelDescriptors.emplace_back(
        OpenVDS::VolumeDataFormat::Format_U8,
        OpenVDS::VolumeDataComponents::Components_1,
        "Trace",
        "",
        0.0f,
        1.0f,
        OpenVDS::VolumeDataMapping::PerTrace,
        OpenVDS::VolumeDataChannelDescriptor::DiscreteData
    );
    m_logger->LogInfo(m_log_data, "  Trace channel added (standard)");

    // 4. Attribute channels (create channel for each attribute field)
    for (const auto& attrField : m_attributeFields) {
        OpenVDS::VolumeDataFormat attrFormat = attrField.format;
        float attrIntegerOffset = -GetVDSIntegerOffsetForDataSampleFormat(attrField.format);
        float attrIntegerScale = 1.0f;
        
        OpenVDS::FloatRange attrEffectiveRange(
            attrField.valueRange.min + attrIntegerOffset,
            attrField.valueRange.max + attrIntegerOffset
        );
        
        channelDescriptors.emplace_back(
            attrFormat,
            OpenVDS::VolumeDataComponents::Components_1,
            attrField.name.c_str(),
            "",
            attrEffectiveRange.Min,
            attrEffectiveRange.Max,
            OpenVDS::VolumeDataMapping::PerTrace,  // Attribute data uses PerTrace mapping
            OpenVDS::VolumeDataChannelDescriptor::DiscreteData
        );
        
        m_logger->LogInfo(m_log_data, "  Attribute channel: {}", attrField.name);
        m_logger->LogInfo(m_log_data, "    Format: {}", static_cast<int>(attrFormat));
        m_logger->LogInfo(m_log_data, "    IntegerScale: {}", attrIntegerScale);
        m_logger->LogInfo(m_log_data, "    IntegerOffset: {}", attrIntegerOffset);
    }
    
    m_logger->LogInfo(m_log_data, "Total channels created: {}", channelDescriptors.size());
    return channelDescriptors;
}

OpenVDS::MetadataContainer VDSHandler::CreateMetadataContainer()
{
    OpenVDS::MetadataContainer metadataContainer;
    
    // Add basic metadata
    // Here we can add SEGY-related metadata such as coordinate systems, measurement units, etc.
    // For simplicity, create empty container for now, can be extended later
    
    m_logger->LogInfo(m_log_data, "MetadataContainer created (basic)");
    return metadataContainer;
}

float VDSHandler::GetVDSIntegerOffsetForDataSampleFormat(OpenVDS::VolumeDataFormat format)
{
    switch (format) {
        case OpenVDS::VolumeDataFormat::Format_U8:
            return -128.0f;  // int8 range -128 to 127, offset -128
        case OpenVDS::VolumeDataFormat::Format_U16:
            return -32768.0f;  // int16 range -32768 to 32767, offset -32768
        case OpenVDS::VolumeDataFormat::Format_U32:
            return -2147483648.0f;  // int32 range, offset -2^31
        case OpenVDS::VolumeDataFormat::Format_R32:
        default:
            return 0.0f;  // float format doesn't need offset
    }
}

const char* VDSHandler::GetUnitString(SampleUnits units)
{
    switch (units) {
        case SampleUnits::Milliseconds: return KNOWNMETADATA_UNIT_MILLISECOND;
        case SampleUnits::Meters:       return KNOWNMETADATA_UNIT_METER;
        case SampleUnits::Feet:         return KNOWNMETADATA_UNIT_FOOT;
        default:                        return KNOWNMETADATA_UNIT_MILLISECOND;
    }
}

void VDSHandler::Close()
{
    if (m_vdsHandle) {
        OpenVDS::Error closeError;
        OpenVDS::Close(m_vdsHandle, closeError);
        if (closeError.code != 0) {
            m_logger->LogWarning(m_log_data, "Error closing VDS: {}", closeError.string);
        }
        m_vdsHandle = nullptr;
        m_logger->LogInfo(m_log_data, "VDS closed successfully");
    }
    m_created = false;
}