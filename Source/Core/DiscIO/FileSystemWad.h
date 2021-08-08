// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "DiscIO/Filesystem.h"
#include "DiscIO/VolumeWad.h"

namespace DiscIO
{

class FileSystemWAD;

class FileInfoWAD : public FileInfo
{
public:
  FileInfoWAD(const FileSystemWAD& fs, u16 index);
  std::unique_ptr<FileInfo> clone() const override;
  const_iterator begin() const override;
  const_iterator end() const override;
  u64 GetOffset() const override;
  u32 GetSize() const override;
  bool IsRoot() const override;
  bool IsDirectory() const override;
  u32 GetTotalChildren() const override;
  std::string GetName() const override;
  bool NameCaseInsensitiveEquals(std::string_view other) const override;
  std::string GetPath() const override;

protected:
  uintptr_t GetAddress() const override;
  FileInfo& operator++() override;

  const FileSystemWAD& m_fs;

  // IOS::ES::Content::index
  u16 m_index;
};

class FileSystemWAD : public FileSystem
{
public:
  FileSystemWAD(const VolumeWAD& volume);

  bool IsValid() const override;
  const FileInfo& GetRoot() const override;
  std::unique_ptr<FileInfo> FindFileInfo(std::string_view path) const override;
  std::unique_ptr<FileInfo> FindFileInfo(u64 disc_offset) const override;

private:
  std::unique_ptr<FileInfo> FindFileInfo(std::string_view path, const FileInfo& file_info) const;

  const VolumeWAD& m_volume;
  u32 m_data_offset;
  std::vector<IOS::ES::Content> m_contents;
  FileInfoWAD m_root;

  friend class FileInfoWAD;
};

}