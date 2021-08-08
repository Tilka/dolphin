// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/Align.h"
#include "DiscIO/FileSystemWad.h"

namespace DiscIO
{

FileInfoWAD::FileInfoWAD(const FileSystemWAD& fs, u16 index)
    : m_fs(fs), m_index(index)
{
}

std::unique_ptr<FileInfo> FileInfoWAD::clone() const
{
  return std::make_unique<FileInfoWAD>(m_fs, m_index);
}

FileInfoWAD::const_iterator FileInfoWAD::begin() const
{
  return const_iterator(std::make_unique<FileInfoWAD>(m_fs, 0));
}

FileInfoWAD::const_iterator FileInfoWAD::end() const
{
  return const_iterator(std::make_unique<FileInfoWAD>(m_fs, (u32)m_fs.m_contents.size()));
}

u64 FileInfoWAD::GetOffset() const
{
  u64 offset = m_fs.m_data_offset;
  for (u16 i = 0; i < m_index; ++i)
    offset += Common::AlignUp(m_fs.m_contents[i].size, 0x40);
  return offset;
}

u32 FileInfoWAD::GetSize() const
{
  return (u32)m_fs.m_contents[m_index].size;
}

bool FileInfoWAD::IsRoot() const
{
  return m_index == 0xFFFF;
}
bool FileInfoWAD::IsDirectory() const
{
  return IsRoot();
}

u32 FileInfoWAD::GetTotalChildren() const
{
  return (u32)m_fs.m_contents.size();
}

std::string FileInfoWAD::GetName() const
{
  const auto& content = m_fs.m_contents[m_index];
  return fmt::format("content_{:08x}_{:04x}.app", content.id, content.index);
}

bool FileInfoWAD::NameCaseInsensitiveEquals(std::string_view other) const
{
  return GetName() == other;
}

std::string FileInfoWAD::GetPath() const
{
  return GetName();
}

uintptr_t FileInfoWAD::GetAddress() const
{
  return m_index;
}

FileInfo& FileInfoWAD::operator++()
{
  m_index++;
  return *this;
}

FileSystemWAD::FileSystemWAD(const VolumeWAD& volume)
    : m_volume(volume), m_data_offset(m_volume.GetDataOffset()),
    m_contents(m_volume.GetTMD().GetContents()), m_root(*this, 0xFFFF)
{
}

bool FileSystemWAD::IsValid() const
{
  return true;
}

const FileInfo& FileSystemWAD::GetRoot() const
{
  return m_root;
}

std::unique_ptr<FileInfo> FileSystemWAD::FindFileInfo(std::string_view path) const
{
  return FindFileInfo(path, m_root);
}

// TODO: deduplicate this with FileSystemGCWii, looks generic enough to put it directly into FileSystem, maybe?
std::unique_ptr<FileInfo> FileSystemWAD::FindFileInfo(std::string_view path,
                                                      const FileInfo& file_info) const
{
  // Given a path like "directory1/directory2/fileA.bin", this function will
  // find directory1 and then call itself to search for "directory2/fileA.bin".

  const size_t name_start = path.find_first_not_of('/');
  if (name_start == std::string::npos)
    return file_info.clone();  // We're done

  const size_t name_end = path.find('/', name_start);
  const std::string_view name = path.substr(name_start, name_end - name_start);
  const std::string_view rest_of_path =
      (name_end != std::string::npos) ? path.substr(name_end + 1) : "";

  for (const FileInfo& child : file_info)
  {
    // We need case insensitive comparison since some games have OPENING.BNR instead of opening.bnr
    if (child.NameCaseInsensitiveEquals(name))
    {
      // A match is found. The rest of the path is passed on to finish the search.
      std::unique_ptr<FileInfo> result = FindFileInfo(rest_of_path, child);

      // If the search wasn't successful, the loop continues, just in case there's a second
      // file info that matches searching_for (which probably won't happen in practice)
      if (result)
        return result;
    }
  }

  return nullptr;
}

std::unique_ptr<FileInfo> FileSystemWAD::FindFileInfo(u64 disc_offset) const
{
  // TODO (required for FileMonitor)
  ERROR_LOG_FMT(DISCIO, "not implemented: FindFileInfo({})", disc_offset);
  return {};
}

}  // namespace DiscIO