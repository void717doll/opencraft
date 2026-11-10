#ifndef SIGNTEXT_HPP_
#define SIGNTEXT_HPP_

#include <array>
#include <string>
#include "common/world/BlockMetadata.hpp"

namespace sign {
using Lines = std::array<std::string, 4>;
inline std::string cleanLine(const std::string &text) {
	std::string result;
	for (unsigned char c : text) {
		if (c >= 32 && c <= 126 && result.size() < 15) result += char(c);
	}
	return result;
}
inline Lines read(BlockMetadata &meta) {
	Lines lines;
	for (int i = 0; i < 4; ++i) {
		std::string key = "sign_line_" + std::to_string(i);
		if (meta.hasString(key)) lines[i] = cleanLine(meta.get<std::string>(key));
	}
	return lines;
}
inline void write(BlockMetadata &meta, const Lines &lines) {
	for (int i = 0; i < 4; ++i)
		meta.setString("sign_line_" + std::to_string(i), cleanLine(lines[i]));
}
}
#endif
