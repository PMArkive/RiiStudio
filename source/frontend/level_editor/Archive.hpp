#pragma once

#include <core/common.h>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

struct Archive {
  std::map<std::string, std::shared_ptr<Archive>> folders;
  std::map<std::string, std::vector<u8>> files;
};

//! Read a .szs/.carc file to a generic Archive
std::optional<Archive> ReadArchive(std::span<const u8> buf);

//! Write a .szs/.carc file from a generic Archive
std::vector<u8> WriteArchive(const Archive& arc);

/*
FindFile(arc, "pictures/dogs/1.png");

return arc.folders.find("pictures")?.folders.find("dogs")?.files.find("1.png");
*/
std::optional<std::vector<u8>> FindFile(Archive& arc, std::string path);