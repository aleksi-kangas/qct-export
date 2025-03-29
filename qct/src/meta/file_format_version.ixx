module;

#include <cstdint>
#include <string>

export module qct:meta.version;

export namespace qct::meta {
/**
 *
 */
enum class FileFormatVersion : std::int32_t {
  QUICK_CHART = 0x00000002,
  QUICK_CHART_SUPPORTING_LICENSE_MANAGEMENT = 0x00000004,
  QC3 = 0x20000001
};

std::string to_string(const FileFormatVersion magic_number) {
  switch (magic_number) {
    case FileFormatVersion::QUICK_CHART:
      return "Quick Chart";
    case FileFormatVersion::QUICK_CHART_SUPPORTING_LICENSE_MANAGEMENT:
      return "Quick Chart supporting License Management";
    case FileFormatVersion::QC3:
      return "QC3 Format";
    default:
      return "Unknown";
  }
}
}  // namespace qct::meta
