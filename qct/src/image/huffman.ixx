module;

#include <cstdint>
#include <format>
#include <fstream>
#include <vector>

export module qct:image.huffman;

import :common.exception;
import :image;
import :palette;
import :palette.sub;
import :util.reader;

/**
 * Decode a huffman-encoded image tile.
 * @param file to read from
 * @param image_tile_byte_offset byte offset of the image tile
 * @param palette the palette
 * @return decoded image tile bytes
 */
namespace qct::image::huffman {
export tile_bytes_2D_t decode(std::ifstream& file, std::int32_t image_tile_byte_offset,
                              const palette::Palette& palette);

class HuffmanCodeBook {
 public:
  [[nodiscard]] bool isColor() const;
  [[nodiscard]] bool isColor(std::int32_t node) const;

  [[nodiscard]] bool isFarBranch() const;
  [[nodiscard]] bool isFarBranch(std::int32_t node) const;

  [[nodiscard]] bool isNearBranch() const;
  [[nodiscard]] bool isNearBranch(std::int32_t node) const;

  [[nodiscard]] bool isValid() const;

  [[nodiscard]] palette::Color getColor(const palette::Palette& palette) const;
  [[nodiscard]] palette::Color getColor(const palette::Palette& palette, std::int32_t node) const;

  [[nodiscard]] std::int32_t size() const;

  void resetPointer();
  void step(bool bit);

  static HuffmanCodeBook parse(std::ifstream& file, std::int32_t& tree_start_byte_offset);

 private:
  std::vector<std::uint8_t> bytes_{};
  std::int32_t pointer_{0};

  [[nodiscard]] std::int32_t farBranchJumpSize(std::int32_t node) const;
  [[nodiscard]] std::int32_t nearBranchJumpSize(std::int32_t node) const;
};

}  // namespace qct::image::huffman

namespace qct::image::huffman {
tile_bytes_2D_t decode(std::ifstream& file, const std::int32_t image_tile_byte_offset,
                       const palette::Palette& palette) {
  tile_bytes_2D_t tile{};
  std::int32_t current_byte_offset{image_tile_byte_offset + 1};
  auto tree = HuffmanCodeBook::parse(file, current_byte_offset);
  if (tree.size() == 1) {
    const auto [red, green, blue] = tree.getColor(palette, 0);
    for (std::int32_t pixel_index = 0; pixel_index < TILE_PIXEL_COUNT; ++pixel_index) {
      const std::int32_t y = pixel_index * palette::COLOR_CHANNELS / TILE_ROW_BYTE_COUNT;
      const std::int32_t x = pixel_index * palette::COLOR_CHANNELS % TILE_ROW_BYTE_COUNT;
      tile[y][x + 0] = red;
      tile[y][x + 1] = green;
      tile[y][x + 2] = blue;
    }
    return tile;
  }
  std::uint8_t current_byte = util::readByte(file, current_byte_offset++);
  std::int32_t bit_count{8};
  std::int32_t pixel_index{0};
  tree.resetPointer();
  while (pixel_index < TILE_PIXEL_COUNT) {
    if (tree.isColor()) {
      const std::int32_t y = pixel_index * palette::COLOR_CHANNELS / TILE_ROW_BYTE_COUNT;
      const std::int32_t x = pixel_index * palette::COLOR_CHANNELS % TILE_ROW_BYTE_COUNT;
      const auto [red, green, blue] = tree.getColor(palette);
      tile[y][x + 0] = red;
      tile[y][x + 1] = green;
      tile[y][x + 2] = blue;
      ++pixel_index;
      tree.resetPointer();
      continue;
    }
    const bool bit = current_byte & 1;
    tree.step(bit);

    current_byte >>= 1;
    --bit_count;
    if (bit_count == 0) {
      current_byte = util::readByte(file, current_byte_offset++);
      bit_count = 8;
    }
  }
  return tile;
}

bool HuffmanCodeBook::isColor() const {
  return isColor(pointer_);
}

bool HuffmanCodeBook::isColor(const std::int32_t node) const {
  return static_cast<std::int32_t>(bytes_[node]) < 128;
}
bool HuffmanCodeBook::isFarBranch() const {
  return isFarBranch(pointer_);
}

bool HuffmanCodeBook::isFarBranch(const std::int32_t node) const {
  return static_cast<std::int32_t>(bytes_[node]) == 128;
}

bool HuffmanCodeBook::isNearBranch() const {
  return isNearBranch(pointer_);
}

bool HuffmanCodeBook::isNearBranch(const std::int32_t node) const {
  return static_cast<std::int32_t>(bytes_[node]) > 128;
}

bool HuffmanCodeBook::isValid() const {
  if (size() == 0)
    return false;
  if (size() == 1)
    return true;
  for (std::int32_t i = 0; i < size(); ++i) {
    if (isColor(i))
      continue;
    if (isFarBranch(i)) {
      if (i + 2 >= size())
        return false;
      if (i + farBranchJumpSize(i) >= size())
        return false;
      i += 2;
    } else if (isNearBranch(i)) {
      if (i + nearBranchJumpSize(i) >= size())
        return false;
    } else {
      return false;
    }
  }
  return true;
}

palette::Color HuffmanCodeBook::getColor(const palette::Palette& palette) const {
  return getColor(palette, pointer_);
}
palette::Color HuffmanCodeBook::getColor(const palette::Palette& palette, const std::int32_t node) const {
  if (!isColor(node))
    throw common::QctException{std::format("Attempting to get color from non-color node={}", node)};
  return palette.colors[static_cast<std::int32_t>(bytes_[node])];
}

std::int32_t HuffmanCodeBook::size() const {
  return static_cast<std::int32_t>(bytes_.size());
}

void HuffmanCodeBook::resetPointer() {
  pointer_ = 0;
}

void HuffmanCodeBook::step(const bool bit) {
  if (bit) {  // Right, i.e. jump
    if (isNearBranch(pointer_)) {
      pointer_ += nearBranchJumpSize(pointer_);
    } else if (isFarBranch()) {
      pointer_ += farBranchJumpSize(pointer_);
    } else {
      throw common::QctException{"Attempting to step in a non-branch node"};
    }
  } else {  // Left, i.e. no jump
    if (isFarBranch(pointer_)) {
      pointer_ += 3;
    } else if (isNearBranch(pointer_)) {
      ++pointer_;
    } else {
      throw common::QctException{"Attempting to step in a non-branch node"};
    }
  }
}

HuffmanCodeBook HuffmanCodeBook::parse(std::ifstream& file, std::int32_t& tree_start_byte_offset) {
  HuffmanCodeBook tree{};
  tree.bytes_.reserve(256);
  std::int32_t color_count{0};
  std::int32_t branch_count{0};
  while (color_count <= branch_count) {
    tree.bytes_.push_back(util::readByte(file, tree_start_byte_offset++));
    if (tree.isFarBranch(tree.size() - 1)) {
      tree.bytes_.push_back(util::readByte(file, tree_start_byte_offset++));
      tree.bytes_.push_back(util::readByte(file, tree_start_byte_offset++));
      ++branch_count;
    } else if (tree.isNearBranch(tree.size() - 1)) {
      ++branch_count;
    } else {
      ++color_count;
    }
  }
  if (!tree.isValid()) {
    throw common::QctException{"Invalid Huffman tree"};
  }
  return tree;
}

std::int32_t HuffmanCodeBook::farBranchJumpSize(const std::int32_t node) const {
  return 65537 - (256 * static_cast<std::int32_t>(bytes_[node + 2]) + static_cast<std::int32_t>(bytes_[node + 1])) + 2;
}
std::int32_t HuffmanCodeBook::nearBranchJumpSize(const std::int32_t node) const {
  return 257 - static_cast<std::int32_t>(bytes_[node]);
}

}  // namespace qct::image::huffman
