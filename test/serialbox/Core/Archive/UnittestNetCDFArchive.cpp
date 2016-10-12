//===-- serialbox/Core/Archive/UnittestNetCDFArchive.cpp ----------------------------*- C++ -*-===//
//
//                                    S E R I A L B O X
//
// This file is distributed under terms of BSD license.
// See LICENSE.txt for more information
//
//===------------------------------------------------------------------------------------------===//
//
/// \file
/// This file contains the unittests for the NetCDF Archive.
///
//===------------------------------------------------------------------------------------------===//

#include "Utility/SerializerTestBase.h"
#include "Utility/Storage.h"
#include "serialbox/Core/Archive/NetCDFArchive.h"
#include "serialbox/Core/Compiler.h"
#include <boost/algorithm/string.hpp>
#include <gtest/gtest.h>

#ifdef SERIALBOX_HAS_NETCDF

using namespace serialbox;
using namespace unittest;

//===------------------------------------------------------------------------------------------===//
//     Utility tests
//===------------------------------------------------------------------------------------------===//

namespace {

class NetCDFArchiveUtilityTest : public SerializerUnittestBase {};

} // anonymous namespace

TEST_F(NetCDFArchiveUtilityTest, Construction) {

  // -----------------------------------------------------------------------------------------------
  // Writing
  // -----------------------------------------------------------------------------------------------

  // Open fresh archive and write meta data to disk
  {
    NetCDFArchive b(OpenModeKind::Write, this->directory->path().string(), "field");
    b.updateMetaData();

    EXPECT_EQ(b.name(), "NetCDF");
    EXPECT_EQ(b.mode(), OpenModeKind::Write);
    EXPECT_EQ(b.prefix(), "field");
  }

  // Create directory if not already existent
  {
    NetCDFArchive b(OpenModeKind::Write, (this->directory->path() / "this-dir-is-created").string(),
                    "field");
    EXPECT_TRUE(boost::filesystem::exists(this->directory->path() / "this-dir-is-created"));
  }

  // -----------------------------------------------------------------------------------------------
  // Reading
  // -----------------------------------------------------------------------------------------------
  {
    NetCDFArchive b(OpenModeKind::Read, this->directory->path().string(), "field");
    b.updateMetaData();
  }

  // Throw Exception: Directory does not exist
  {
    EXPECT_THROW(NetCDFArchive(OpenModeKind::Read, (this->directory->path() / "not-a-dir").string(),
                               "field"),
                 Exception);
  }

  // -----------------------------------------------------------------------------------------------
  // Appending
  // -----------------------------------------------------------------------------------------------

  {
    EXPECT_NO_THROW(NetCDFArchive(OpenModeKind::Append, this->directory->path().string(), "field"));
  }

  // Create directory if not already existent
  {
    NetCDFArchive b(OpenModeKind::Append,
                    (this->directory->path() / "this-dir-is-created").string(), "field");
  }

  // Create directories if not already existent
  {
    NetCDFArchive b(OpenModeKind::Append, (this->directory->path() / "nest1" / "nest2").string(),
                    "field");
    EXPECT_TRUE(boost::filesystem::exists(this->directory->path() / "nest1" / "nest2"));
  }
}

TEST_F(NetCDFArchiveUtilityTest, MetaData) {
  using Storage = Storage<double>;

  Storage u_0_input(Storage::RowMajor, {5, 6, 7}, {{2, 2}, {4, 2}, {4, 5}}, Storage::random);

  NetCDFArchive archiveWrite(OpenModeKind::Write, this->directory->path().string(), "field");

  auto sv_u_0_input = u_0_input.toStorageView();
  archiveWrite.write(sv_u_0_input, "u");
  archiveWrite.updateMetaData();

  // Read meta data file to get in memory copy
  std::ifstream ifs(archiveWrite.metaDataFile());
  json::json j;
  ifs >> j;
  ifs.close();

  // Write meta file to disk (to corrupt it)
  std::string filename = archiveWrite.metaDataFile();
  auto toFile = [this, &filename](const json::json& jsonNode) -> void {
    std::ofstream ofs(filename, std::ios::out | std::ios::trunc);
    ofs << jsonNode.dump(4);
  };

  // -----------------------------------------------------------------------------------------------
  // Invlaid serialbox version
  // -----------------------------------------------------------------------------------------------
  {
    json::json corrupted = j;
    corrupted["serialbox_version"] = 100 * (SERIALBOX_VERSION_MAJOR - 1) +
                                     10 * SERIALBOX_VERSION_MINOR + SERIALBOX_VERSION_PATCH;
    toFile(corrupted);

    ASSERT_THROW(NetCDFArchive(OpenModeKind::Read, this->directory->path().string(), "field"),
                 Exception);
  }

  // -----------------------------------------------------------------------------------------------
  // Not a binary archive
  // -----------------------------------------------------------------------------------------------
  {
    json::json corrupted = j;
    corrupted["archive_name"] = "not-NetCDFArchive";
    toFile(corrupted);

    ASSERT_THROW(NetCDFArchive(OpenModeKind::Read, this->directory->path().string(), "field"),
                 Exception);
  }

  // -----------------------------------------------------------------------------------------------
  // Invalid binary archive version
  // -----------------------------------------------------------------------------------------------
  {
    json::json corrupted = j;
    corrupted["archive_version"] = -1;
    toFile(corrupted);

    ASSERT_THROW(NetCDFArchive(OpenModeKind::Read, this->directory->path().string(), "field"),
                 Exception);
  }

  // -----------------------------------------------------------------------------------------------
  // MetaData not found
  // -----------------------------------------------------------------------------------------------
  {
    boost::filesystem::remove(filename);
    ASSERT_THROW(NetCDFArchive(OpenModeKind::Read, this->directory->path().string(), "field"),
                 Exception);
  }
}

TEST_F(NetCDFArchiveUtilityTest, toString) {
  using Storage = Storage<double>;
  std::stringstream ss;

  Storage storage(Storage::ColMajor, {5, 1, 1});

  NetCDFArchive archive(OpenModeKind::Write, this->directory->path().string(), "field");
  StorageView sv = storage.toStorageView();
  archive.write(sv, "storage");

  ss << archive;

  EXPECT_TRUE(boost::algorithm::starts_with(ss.str(), "NetCDFArchive"));
  EXPECT_NE(ss.str().find("directory"), std::string::npos);
  EXPECT_NE(ss.str().find("mode"), std::string::npos);
  EXPECT_NE(ss.str().find("prefix"), std::string::npos);
  EXPECT_NE(ss.str().find("fieldMap"), std::string::npos);
}

//===------------------------------------------------------------------------------------------===//
//     Read/Write tests
//===------------------------------------------------------------------------------------------===//

namespace {

template <class T>
class NetCDFArchiveReadWriteTest : public SerializerUnittestBase {};

using TestTypes = testing::Types<double, float, int, std::int64_t>;

} // anonymous namespace

TYPED_TEST_CASE(NetCDFArchiveReadWriteTest, TestTypes);

TYPED_TEST(NetCDFArchiveReadWriteTest, WriteAndRead) {

  // -----------------------------------------------------------------------------------------------
  // Preparation
  // -----------------------------------------------------------------------------------------------
  using Storage = Storage<TypeParam>;

  // Prepare input data
  Storage u_0_input(Storage::RowMajor, {5, 6, 7}, {{2, 2}, {4, 2}, {4, 5}}, Storage::random);
  Storage u_1_input(Storage::RowMajor, {5, 6, 7}, {{2, 2}, {4, 2}, {4, 5}}, Storage::random);
  Storage u_2_input(Storage::RowMajor, {5, 6, 7}, {{2, 2}, {4, 2}, {4, 5}}, Storage::random);

  Storage v_0_input(Storage::ColMajor, {5, 1, 1}, Storage::random);
  Storage v_1_input(Storage::ColMajor, {5, 1, 1}, Storage::random);
  Storage v_2_input(Storage::ColMajor, {5, 1, 1}, Storage::random);

  Storage storage_2d_0_input(Storage::ColMajor, {26, 23}, {{2, 2}, {4, 2}}, Storage::random);
  Storage storage_2d_1_input(Storage::ColMajor, {26, 23}, {{2, 2}, {4, 2}}, Storage::random);

  Storage storage_7d_0_input(Storage::ColMajor, {2, 2, 2, 2, 2, 2, 2}, Storage::random);
  Storage storage_7d_1_input(Storage::ColMajor, {2, 2, 2, 2, 2, 2, 2}, Storage::random);

  // Prepare output
  Storage u_0_output(Storage::RowMajor, {5, 6, 7});
  Storage u_1_output(Storage::RowMajor, {5, 6, 7});
  Storage u_2_output(Storage::RowMajor, {5, 6, 7});

  Storage v_0_output(Storage::RowMajor, {5, 1, 1});
  Storage v_1_output(Storage::RowMajor, {5, 1, 1});
  Storage v_2_output(Storage::RowMajor, {5, 1, 1});

  Storage storage_2d_0_output(Storage::ColMajor, {26, 23}, {{2, 2}, {4, 2}});
  Storage storage_2d_1_output(Storage::ColMajor, {26, 23}, {{2, 2}, {4, 2}});

  Storage storage_7d_0_output(Storage::ColMajor, {2, 2, 2, 2, 2, 2, 2});
  Storage storage_7d_1_output(Storage::ColMajor, {2, 2, 2, 2, 2, 2, 2});

  // -----------------------------------------------------------------------------------------------
  // Writing (data and meta-data)
  // -----------------------------------------------------------------------------------------------
  {
    NetCDFArchive archiveWrite(OpenModeKind::Write, this->directory->path().string(), "field");

    EXPECT_STREQ(archiveWrite.directory().c_str(), this->directory->path().string().c_str());

    // u: id = 0
    {
      auto sv = u_0_input.toStorageView();
      FieldID fieldID = archiveWrite.write(sv, "u");
      ASSERT_EQ(fieldID.name, "u");
      ASSERT_EQ(fieldID.id, 0);
    }

    // u:  id = 1
    {
      auto sv = u_1_input.toStorageView();
      FieldID fieldID = archiveWrite.write(sv, "u");
      ASSERT_EQ(fieldID.name, "u");
      ASSERT_EQ(fieldID.id, 1);
    }

    // u:  id = 2
    {
      auto sv = u_2_input.toStorageView();
      FieldID fieldID = archiveWrite.write(sv, "u");
      ASSERT_EQ(fieldID.name, "u");
      ASSERT_EQ(fieldID.id, 2);
    }

    // v:  id = 0
    {
      auto sv = v_0_input.toStorageView();
      FieldID fieldID = archiveWrite.write(sv, "v");
      ASSERT_EQ(fieldID.name, "v");
      ASSERT_EQ(fieldID.id, 0);
    }

    // v:  id = 1
    {
      auto sv = v_1_input.toStorageView();
      FieldID fieldID = archiveWrite.write(sv, "v");
      ASSERT_EQ(fieldID.name, "v");
      ASSERT_EQ(fieldID.id, 1);
    }

    // v:  id = 2
    {
      auto sv = v_2_input.toStorageView();
      FieldID fieldID = archiveWrite.write(sv, "v");
      ASSERT_EQ(fieldID.name, "v");
      ASSERT_EQ(fieldID.id, 2);
    }

    // storage 2d
    auto sv_2d_0_input = storage_2d_0_input.toStorageView();
    archiveWrite.write(sv_2d_0_input, "storage_2d");

    auto sv_2d_1_input = storage_2d_1_input.toStorageView();
    archiveWrite.write(sv_2d_1_input, "storage_2d");

    // storage 7d
    auto sv_7d_0_input = storage_7d_0_input.toStorageView();
    archiveWrite.write(sv_7d_0_input, "storage_7d");

    auto sv_7d_1_input = storage_7d_1_input.toStorageView();
    archiveWrite.write(sv_7d_1_input, "storage_7d");

    // Check all exceptional cases
    auto sv_u_2_input = u_2_input.toStorageView();
    ASSERT_THROW(archiveWrite.read(sv_u_2_input, FieldID{"u", 2}), Exception);
  }

  // -----------------------------------------------------------------------------------------------
  // Reading
  // -----------------------------------------------------------------------------------------------
  {
    NetCDFArchive archiveRead(OpenModeKind::Read, this->directory->path().string(), "field");
    EXPECT_STREQ(archiveRead.directory().c_str(), this->directory->path().string().c_str());

    // u
    auto sv_u_0_output = u_0_output.toStorageView();
    ASSERT_NO_THROW(archiveRead.read(sv_u_0_output, FieldID{"u", 0}));

    auto sv_u_1_output = u_1_output.toStorageView();
    ASSERT_NO_THROW(archiveRead.read(sv_u_1_output, FieldID{"u", 1}));

    auto sv_u_2_output = u_2_output.toStorageView();
    ASSERT_NO_THROW(archiveRead.read(sv_u_2_output, FieldID{"u", 2}));

    // v
    auto sv_v_0_output = v_0_output.toStorageView();
    ASSERT_NO_THROW(archiveRead.read(sv_v_0_output, FieldID{"v", 0}));

    auto sv_v_1_output = v_1_output.toStorageView();
    ASSERT_NO_THROW(archiveRead.read(sv_v_1_output, FieldID{"v", 1}));

    auto sv_v_2_output = v_2_output.toStorageView();
    ASSERT_NO_THROW(archiveRead.read(sv_v_2_output, FieldID{"v", 2}));

    // Check all exceptional cases
    ASSERT_THROW(archiveRead.write(sv_u_2_output, "u"), Exception);
    ASSERT_THROW(archiveRead.read(sv_u_2_output, FieldID{"u", 1024}), Exception);
    ASSERT_THROW(archiveRead.read(sv_u_2_output, FieldID{"not-a-field", 0}), Exception);

    // storage 2d
    auto sv_2d_0_output = storage_2d_0_output.toStorageView();
    ASSERT_NO_THROW(archiveRead.read(sv_2d_0_output, FieldID{"storage_2d", 0}));

    auto sv_2d_1_output = storage_2d_1_output.toStorageView();
    ASSERT_NO_THROW(archiveRead.read(sv_2d_1_output, FieldID{"storage_2d", 1}));

    // storage 7d
    auto sv_7d_0_output = storage_7d_0_output.toStorageView();
    ASSERT_NO_THROW(archiveRead.read(sv_7d_0_output, FieldID{"storage_7d", 0}));

    auto sv_7d_1_output = storage_7d_1_output.toStorageView();
    ASSERT_NO_THROW(archiveRead.read(sv_7d_1_output, FieldID{"storage_7d", 1}));
  }

  // -----------------------------------------------------------------------------------------------
  // Validation
  // -----------------------------------------------------------------------------------------------
  ASSERT_TRUE(Storage::verify(u_0_output, u_0_input));
  ASSERT_TRUE(Storage::verify(u_1_output, u_1_input));
  ASSERT_TRUE(Storage::verify(u_2_output, u_2_input));

  ASSERT_TRUE(Storage::verify(v_0_output, v_0_input));
  ASSERT_TRUE(Storage::verify(v_1_output, v_1_input));
  ASSERT_TRUE(Storage::verify(v_2_output, v_2_input));

  ASSERT_TRUE(Storage::verify(storage_2d_0_output, storage_2d_0_input));
  ASSERT_TRUE(Storage::verify(storage_2d_1_output, storage_2d_1_input));

  ASSERT_TRUE(Storage::verify(storage_7d_0_output, storage_7d_0_input));
  ASSERT_TRUE(Storage::verify(storage_7d_1_output, storage_7d_1_input));

  // -----------------------------------------------------------------------------------------------
  // Cleanup
  // -----------------------------------------------------------------------------------------------
  {
    NetCDFArchive archiveWrite(OpenModeKind::Write, this->directory->path().string(), "field");
    EXPECT_FALSE(boost::filesystem::exists(this->directory->path() / ("field_u.nc")));
    EXPECT_FALSE(boost::filesystem::exists(this->directory->path() / ("field_v.nc")));
    EXPECT_FALSE(boost::filesystem::exists(this->directory->path() / ("field_storage_2d.nc")));
    EXPECT_FALSE(boost::filesystem::exists(this->directory->path() / ("field_storage_7d.nc")));
  }
}

#endif